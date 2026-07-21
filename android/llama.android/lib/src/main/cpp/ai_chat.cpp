#include <android/log.h>
#include <jni.h>
#include <algorithm>
#include <iomanip>
#include <cmath>
#include <cstring>
#include <string>
#include <unistd.h>
#include <sampling.h>

#include "dmc_reference.hpp"
#include "logging.h"
#include "chat.h"
#include "common.h"
#include "llama.h"

template<class T>
static std::string join(const std::vector<T> &values, const std::string &delim) {
    std::ostringstream str;
    for (size_t i = 0; i < values.size(); i++) {
        str << values[i];
        if (i < values.size() - 1) { str << delim; }
    }
    return str.str();
}

static std::string list_non_cpu_backends() {
    std::vector<std::string> backends;
    for (size_t i = 0; i < ggml_backend_reg_count(); i++) {
        auto * reg = ggml_backend_reg_get(i);
        if (!reg) {
            continue;
        }

        std::string name = ggml_backend_reg_name(reg);
        if (name != "CPU") {
            backends.push_back(name);
        }
    }
    return backends.empty() ? "CPU" : join(backends, ",");
}

/**
 * LLama resources: context, model, batch and sampler
 */
constexpr int   N_THREADS_MIN           = 2;
constexpr int   N_THREADS_MAX           = 6;
constexpr int   N_THREADS_HEADROOM      = 1;

constexpr int   MIN_CONTEXT_SIZE        = 4096;
constexpr int   DMC_PHYSICAL_CONTEXT    = 16384;
constexpr int   DMC_MIN_OUTPUT_RESERVE  = 512;
constexpr int   DMC_CONTINUATION_RESERVE = 4096;
constexpr int   DMC_SYSTEM_PREFIX_MAX   = 1024;
constexpr int   OVERFLOW_HEADROOM       = 4;
constexpr int   BATCH_SIZE              = 512;
constexpr float DEFAULT_SAMPLER_TEMP    = 0.3f;

static llama_model                      * g_model;
static llama_context                    * g_context;
static llama_batch                        g_batch;
static common_chat_templates_ptr          g_chat_templates;
static common_sampler                   * g_sampler;
static bool                               g_enable_thinking = false;
static bool                               g_batch_initialized = false;
static llama_tokens                       g_dmc_token_history;
static std::size_t                        g_dmc_system_tokens = 0;
static int                                g_dmc_logical_context_window = 0;
static bool                               g_dmc_has_compacted = false;
static std::uint64_t                      g_dmc_rebuild_count = 0;

static std::string trim_copy(const std::string &text) {
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }
    const auto last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

static int runtime_context_window_size(const llama_context *context) {
    if (context == nullptr) {
        return 0;
    }
    return static_cast<int>(llama_n_ctx(context));
}

static int runtime_context_limit(const llama_context *context) {
    return std::max(0, runtime_context_window_size(context) - OVERFLOW_HEADROOM);
}

static std::vector<int> build_context_candidates(llama_model *model, int requested_n_ctx) {
    std::vector<int> candidates;
    if (model == nullptr) {
        return candidates;
    }

    const int trained_context_size = std::max(0, llama_model_n_ctx_train(model));
    const int automatic_context = trained_context_size > 0
        ? std::min(trained_context_size, DMC_PHYSICAL_CONTEXT)
        : DMC_PHYSICAL_CONTEXT;
    const int initial_context = std::max(
        requested_n_ctx > 0 ? requested_n_ctx : automatic_context,
        MIN_CONTEXT_SIZE);

    auto add_candidate = [&](const int candidate) {
        if (candidate < MIN_CONTEXT_SIZE) {
            return;
        }
        if (std::find(candidates.begin(), candidates.end(), candidate) == candidates.end()) {
            candidates.push_back(candidate);
        }
    };

    add_candidate(initial_context);

    for (size_t i = 0; i < candidates.size(); ++i) {
        const int candidate = candidates[i];
        if (candidate <= MIN_CONTEXT_SIZE) {
            continue;
        }

        const int next_candidate = std::max(MIN_CONTEXT_SIZE, candidate / 2);
        add_candidate(next_candidate);
    }

    return candidates;
}

static std::string strip_model_tags(std::string text) {
    auto remove_block = [&](const std::string &open_tag, const std::string &close_tag) {
        size_t start = 0;
        while ((start = text.find(open_tag, start)) != std::string::npos) {
            const size_t end = text.find(close_tag, start + open_tag.size());
            if (end == std::string::npos) {
                text.erase(start);
                break;
            }
            text.erase(start, end + close_tag.size() - start);
        }
    };

    remove_block("<|channel>thought", "<channel|>");
    remove_block("<think>", "</think>");
    remove_block("<|think|>", "</|think|>");

    auto remove_unused_token = [&](const std::string &prefix, const char closing_char) {
        size_t start = 0;
        while ((start = text.find(prefix, start)) != std::string::npos) {
            const size_t end = text.find(closing_char, start + prefix.size());
            if (end == std::string::npos) {
                text.erase(start);
                break;
            }
            text.erase(start, end - start + 1);
        }
    };
    remove_unused_token("<unused", '>');
    remove_unused_token("[unused", ']');

    const char *tokens_to_remove[] = {
        "<|channel>thought",
        "<channel|>",
        "<think>",
        "</think>",
        "Thinking Process:"
    };
    for (const char *token: tokens_to_remove) {
        size_t pos = 0;
        while ((pos = text.find(token, pos)) != std::string::npos) {
            text.erase(pos, std::strlen(token));
        }
    }

    return trim_copy(text);
}

static bool is_hidden_output_piece(const llama_token token, const std::string &piece) {
    if (g_model != nullptr) {
        const auto *vocab = llama_model_get_vocab(g_model);
        if (vocab != nullptr && (llama_vocab_get_attr(vocab, token) & LLAMA_TOKEN_ATTR_UNUSED) != 0) {
            return true;
        }
    }

    if (piece.rfind("<unused", 0) == 0 || piece.rfind("[unused", 0) == 0) {
        return true;
    }

    const char *hidden_pieces[] = {
        "<|channel>thought",
        "<channel|>",
        "<think>",
        "</think>",
        "<|think|>",
        "</|think|>",
        "Thinking Process:"
    };

    for (const char *hidden_piece : hidden_pieces) {
        if (piece == hidden_piece) {
            return true;
        }
    }

    return false;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_arm_aichat_internal_InferenceEngineImpl_init(JNIEnv *env, jobject /*unused*/, jstring nativeLibDir) {
    // Set llama log handler to Android
    llama_log_set(aichat_android_log_callback, nullptr);

    // Loading all CPU backend variants
    const auto *path_to_backend = env->GetStringUTFChars(nativeLibDir, 0);
    LOGi("Loading backends from %s", path_to_backend);
    ggml_backend_load_all_from_path(path_to_backend);
    env->ReleaseStringUTFChars(nativeLibDir, path_to_backend);

    // Initialize backends
    llama_backend_init();
    LOGi("Available backends after init: %s", list_non_cpu_backends().c_str());
    LOGi("Backend initiated; Log handler set.");
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_arm_aichat_internal_InferenceEngineImpl_load(JNIEnv *env, jobject, jstring jmodel_path) {
    llama_model_params model_params = llama_model_default_params();
    model_params.n_gpu_layers = 0;
    model_params.split_mode = LLAMA_SPLIT_MODE_NONE;
    LOGi("CPU-only mode: model offload disabled.");

    const auto *model_path = env->GetStringUTFChars(jmodel_path, 0);
    LOGd("%s: Loading model from: \n%s\n", __func__, model_path);

    auto *model = llama_model_load_from_file(model_path, model_params);
    env->ReleaseStringUTFChars(jmodel_path, model_path);
    if (!model) {
        return 1;
    }
    g_model = model;
    return 0;
}

static llama_context *init_context(llama_model *model, const int requested_n_ctx = 0) {
    if (!model) {
        LOGe("%s: model cannot be null", __func__);
        return nullptr;
    }

    // Multi-threading setup
    const int online_cpus = std::max(1, (int) sysconf(_SC_NPROCESSORS_ONLN));
    const int n_threads = std::max(N_THREADS_MIN, std::min(N_THREADS_MAX, online_cpus - N_THREADS_HEADROOM));
    const int n_threads_batch = std::min(N_THREADS_MAX, n_threads + 1);
    LOGi("%s: Using %d threads (%d for batch processing)", __func__, n_threads, n_threads_batch);

    const int trained_context_size = llama_model_n_ctx_train(model);
    const auto candidates = build_context_candidates(model, requested_n_ctx);
    if (candidates.empty()) {
        LOGe("%s: no viable context candidates available", __func__);
        return nullptr;
    }

    for (const int n_ctx : candidates) {
        // Context parameters setup
        llama_context_params ctx_params = llama_context_default_params();
        if (trained_context_size > 0 && n_ctx > trained_context_size) {
            LOGw("%s: trying %d context tokens while the model reports %d trained context tokens",
                 __func__, n_ctx, trained_context_size);
        }
        LOGi("%s: trying context window %d", __func__, n_ctx);
        ctx_params.n_ctx = n_ctx;
        ctx_params.n_batch = BATCH_SIZE;
        ctx_params.n_ubatch = BATCH_SIZE;
        ctx_params.n_threads = n_threads;
        ctx_params.n_threads_batch = n_threads_batch;
        ctx_params.flash_attn_type = LLAMA_FLASH_ATTN_TYPE_ENABLED;
        ctx_params.offload_kqv = false;
        ctx_params.op_offload = false;

        auto *context = llama_init_from_model(model, ctx_params);
        if (context != nullptr) {
            LOGi("%s: selected context window %d", __func__, runtime_context_window_size(context));
            return context;
        }

        LOGw("%s: context window %d failed; trying a smaller fallback", __func__, n_ctx);
    }

    LOGe("%s: failed to initialize any usable context window", __func__);
    return nullptr;
}

static common_sampler *new_sampler(float temp) {
    common_params_sampling sparams;
    sparams.temp = temp;
    return common_sampler_init(g_model, sparams);
}

static dmc::DMCConfig runtime_dmc_config() {
    dmc::DMCConfig cfg;
    cfg.block_size = 64;
    cfg.local_window = 2048;
    cfg.global_tokens = std::max<std::size_t>(
        64,
        std::min<std::size_t>(g_dmc_system_tokens, DMC_SYSTEM_PREFIX_MAX));
    cfg.replay_levels = 7;
    return cfg;
}

static bool validate_dmc_runtime() {
    try {
        const auto first = dmc::plan_runtime_context(32768, 4096, runtime_dmc_config());
        const auto second = dmc::plan_runtime_context(32768, 4096, runtime_dmc_config());
        const bool valid = first.compacted &&
            first.token_ids == second.token_ids &&
            !first.token_ids.empty() &&
            first.token_ids.size() <= 4096 &&
            first.token_ids.back() == 32767;
        if (!valid) {
            LOGe("DMC_RUNTIME self-test failed");
            return false;
        }
        LOGi("DMC_RUNTIME self-test passed: logical=%d selected=%d spans=%d",
             32768,
             (int) first.token_ids.size(),
             (int) first.spans.size());
        return true;
    } catch (const std::exception &e) {
        LOGe("DMC_RUNTIME self-test failed: %s", e.what());
        return false;
    }
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_arm_aichat_internal_InferenceEngineImpl_prepare(JNIEnv * /*env*/, jobject /*unused*/) {
    auto *context = init_context(g_model);
    if (!context) { return 1; }
    g_context = context;
    g_batch = llama_batch_init(BATCH_SIZE, 0, 1);
    g_batch_initialized = true;
    g_chat_templates = common_chat_templates_init(g_model, "");
    g_sampler = new_sampler(DEFAULT_SAMPLER_TEMP);
    g_dmc_token_history.clear();
    g_dmc_system_tokens = 0;
    g_dmc_has_compacted = false;
    g_dmc_rebuild_count = 0;
    g_dmc_logical_context_window = std::max(
        runtime_context_window_size(g_context),
        llama_model_n_ctx_train(g_model));
    if (!validate_dmc_runtime()) {
        return 2;
    }
    LOGi("DMC_RUNTIME enabled=1 physical_context=%d logical_context=%d block=%d local=%d global=%d levels=%d",
         runtime_context_window_size(g_context),
         g_dmc_logical_context_window,
         (int) runtime_dmc_config().block_size,
         (int) runtime_dmc_config().local_window,
         (int) runtime_dmc_config().global_tokens,
         (int) runtime_dmc_config().replay_levels);
    return 0;
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_arm_aichat_internal_InferenceEngineImpl_setEnableThinking(JNIEnv * /*env*/, jobject /*unused*/, jboolean enableThinking) {
    g_enable_thinking = enableThinking == JNI_TRUE;
    LOGi("%s: enable_thinking=%s", __func__, g_enable_thinking ? "true" : "false");
    return 0;
}

static std::string get_backend() {
    return list_non_cpu_backends();
}

extern "C"
JNIEXPORT jstring JNICALL
Java_com_arm_aichat_internal_InferenceEngineImpl_systemInfo(JNIEnv *env, jobject /*unused*/) {
    return env->NewStringUTF(llama_print_system_info());
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_arm_aichat_internal_InferenceEngineImpl_nativeContextWindowSize(JNIEnv * /*env*/, jobject /*unused*/) {
    return static_cast<jint>(g_dmc_logical_context_window);
}

extern "C"
JNIEXPORT jstring JNICALL
Java_com_arm_aichat_internal_InferenceEngineImpl_benchModel(JNIEnv *env, jobject /*unused*/, jint pp, jint tg,
                                                      jint pl, jint nr) {
    auto *context = init_context(g_model, pp);
    if (!context) {
        const auto *const err_msg = "Fail to init_context! Bench aborted.";
        LOGe(err_msg);
        return env->NewStringUTF(err_msg);
    }

    auto pp_avg = 0.0;
    auto tg_avg = 0.0;
    auto pp_std = 0.0;
    auto tg_std = 0.0;

    const uint32_t n_ctx = llama_n_ctx(context);
    LOGi("n_ctx = %d", n_ctx);

    int i, j;
    int nri;
    for (nri = 0; nri < nr; nri++) {
        LOGi("Benchmark prompt processing (pp = %d)", pp);

        common_batch_clear(g_batch);

        const int n_tokens = pp;
        for (i = 0; i < n_tokens; i++) {
            common_batch_add(g_batch, 0, i, {0}, false);
        }

        g_batch.logits[g_batch.n_tokens - 1] = true;
        llama_memory_clear(llama_get_memory(context), false);

        const auto t_pp_start = ggml_time_us();
        if (llama_decode(context, g_batch) != 0) {
            LOGe("llama_decode() failed during prompt processing");
        }
        const auto t_pp_end = ggml_time_us();

        // bench text generation

        LOGi("Benchmark text generation (tg = %d)", tg);

        llama_memory_clear(llama_get_memory(context), false);
        const auto t_tg_start = ggml_time_us();
        for (i = 0; i < tg; i++) {
            common_batch_clear(g_batch);
            for (j = 0; j < pl; j++) {
                common_batch_add(g_batch, 0, i, {j}, true);
            }

            if (llama_decode(context, g_batch) != 0) {
                LOGe("llama_decode() failed during text generation");
            }
        }
        const auto t_tg_end = ggml_time_us();

        llama_memory_clear(llama_get_memory(context), false);

        const auto t_pp = double(t_pp_end - t_pp_start) / 1000000.0;
        const auto t_tg = double(t_tg_end - t_tg_start) / 1000000.0;

        const auto speed_pp = double(pp) / t_pp;
        const auto speed_tg = double(pl * tg) / t_tg;

        pp_avg += speed_pp;
        tg_avg += speed_tg;

        pp_std += speed_pp * speed_pp;
        tg_std += speed_tg * speed_tg;

        LOGi("pp %f t/s, tg %f t/s", speed_pp, speed_tg);
    }

    llama_free(context);

    pp_avg /= double(nr);
    tg_avg /= double(nr);

    if (nr > 1) {
        pp_std = sqrt(pp_std / double(nr - 1) - pp_avg * pp_avg * double(nr) / double(nr - 1));
        tg_std = sqrt(tg_std / double(nr - 1) - tg_avg * tg_avg * double(nr) / double(nr - 1));
    } else {
        pp_std = 0;
        tg_std = 0;
    }

    char model_desc[128];
    llama_model_desc(g_model, model_desc, sizeof(model_desc));

    const auto model_size = double(llama_model_size(g_model)) / 1024.0 / 1024.0 / 1024.0;
    const auto model_n_params = double(llama_model_n_params(g_model)) / 1e9;

    const auto backend = get_backend();
    std::stringstream result;
    result << std::setprecision(3);
    result << "| model | size | params | backend | test | t/s |\n";
    result << "| --- | --- | --- | --- | --- | --- |\n";
    result << "| " << model_desc << " | " << model_size << "GiB | " << model_n_params << "B | "
           << backend << " | pp " << pp << " | " << pp_avg << " ± " << pp_std << " |\n";
    result << "| " << model_desc << " | " << model_size << "GiB | " << model_n_params << "B | "
           << backend << " | tg " << tg << " | " << tg_avg << " ± " << tg_std << " |\n";
    return env->NewStringUTF(result.str().c_str());
}


/**
 * Completion loop's long-term states:
 * - chat management
 * - position tracking
 */
constexpr const char *ROLE_SYSTEM       = "system";
constexpr const char *ROLE_USER         = "user";
constexpr const char *ROLE_ASSISTANT    = "assistant";

static std::vector<common_chat_msg> chat_msgs;
static llama_pos system_prompt_position;
static llama_pos current_position;

static std::string format_chat_message(const std::string &role, const std::string &content) {
    common_chat_msg new_msg;
    new_msg.role = role;
    new_msg.content = content;

    if (!g_chat_templates) {
        chat_msgs.push_back(new_msg);
        return content;
    }

    const bool use_jinja = common_chat_templates_was_explicit(g_chat_templates.get());

    try {
        common_chat_templates_inputs inputs;
        inputs.use_jinja = use_jinja;
        if (g_model != nullptr) {
            const auto *vocab = llama_model_get_vocab(g_model);
            if (vocab != nullptr) {
                inputs.add_bos = llama_vocab_get_add_bos(vocab);
                inputs.add_eos = llama_vocab_get_add_eos(vocab);
            }
        }
        inputs.enable_thinking = g_enable_thinking;
        inputs.chat_template_kwargs["enable_thinking"] = g_enable_thinking ? "true" : "false";

        std::string fmt_past;
        if (!chat_msgs.empty()) {
            inputs.messages = chat_msgs;
            inputs.add_generation_prompt = false;
            fmt_past = common_chat_templates_apply(g_chat_templates.get(), inputs).prompt;
        }

        inputs.messages.push_back(new_msg);
        inputs.add_generation_prompt = role == ROLE_USER;
        const auto fmt_new = common_chat_templates_apply(g_chat_templates.get(), inputs).prompt;
        std::string formatted;
        if (role == ROLE_USER && !fmt_past.empty() && fmt_past.back() == '\n') {
            formatted = "\n";
        }
        formatted += fmt_new.substr(fmt_past.size(), fmt_new.size() - fmt_past.size());
        chat_msgs.push_back(new_msg);
        LOGi("%s: Formatted and added %s message: \n%s\n", __func__, role.c_str(), formatted.c_str());
        return formatted;
    } catch (const std::exception &e) {
        LOGw("%s: Chat template formatting failed for %s message, using raw content instead: %s",
             __func__, role.c_str(), e.what());
        chat_msgs.push_back(new_msg);
        return content;
    }
}

static void reset_long_term_states(const bool clear_kv_cache = true) {
    chat_msgs.clear();
    system_prompt_position = 0;
    current_position = 0;
    g_dmc_token_history.clear();
    g_dmc_system_tokens = 0;
    g_dmc_has_compacted = false;
    g_dmc_rebuild_count = 0;

    if (clear_kv_cache && g_context)
        llama_memory_clear(llama_get_memory(g_context), false);
}

/**
 * Completion loop's short-term states:
 * - optional explicit output-token limit
 * - token chars caching
 * - current assistant message being generated
 */
static std::int64_t generation_token_limit;
static std::int64_t generated_token_count;
static bool assistant_message_finalized;
static std::string cached_token_chars;
static std::ostringstream assistant_ss;

static void reset_short_term_states() {
    generation_token_limit = -1;
    generated_token_count = 0;
    assistant_message_finalized = false;
    cached_token_chars.clear();
    assistant_ss.str("");
}

static void finalize_assistant_message() {
    if (assistant_message_finalized) {
        return;
    }
    format_chat_message(ROLE_ASSISTANT, strip_model_tags(assistant_ss.str()));
    assistant_message_finalized = true;
}

static int decode_tokens_in_batches(
        llama_context *context,
        llama_batch &batch,
        const llama_tokens &tokens,
        const llama_pos start_pos,
        const bool compute_last_logit = false) {
    // Process tokens in batches using the global batch
    LOGd("%s: Decode %d tokens starting at position %d", __func__, (int) tokens.size(), start_pos);
    for (int i = 0; i < (int) tokens.size(); i += BATCH_SIZE) {
        const int cur_batch_size = std::min((int) tokens.size() - i, BATCH_SIZE);
        common_batch_clear(batch);
        LOGv("%s: Preparing a batch size of %d starting at: %d", __func__, cur_batch_size, i);

        const int context_limit = runtime_context_limit(context);
        // Keep the native window strict on mobile: no recursive context shifting.
        if (start_pos + i + cur_batch_size > context_limit) {
            LOGe("%s: Current batch exceeds context window at position %d (limit %d)",
                 __func__, start_pos + i + cur_batch_size - 1, context_limit - 1);
            return 1;
        }

        // Add tokens to the batch with proper positions
        for (int j = 0; j < cur_batch_size; j++) {
            const llama_token token_id = tokens[i + j];
            const llama_pos position = start_pos + i + j;
            const bool want_logit = compute_last_logit && (i + j == tokens.size() - 1);
            common_batch_add(batch, token_id, position, {0}, want_logit);
        }

        // Decode this batch
        const int decode_result = llama_decode(context, batch);
        if (decode_result) {
            LOGe("%s: llama_decode failed w/ %d", __func__, decode_result);
            return 1;
        }
    }
    return 0;
}

static int rebuild_dmc_context(
        const llama_tokens &logical_history,
        const std::size_t physical_token_budget,
        const bool compute_last_logit) {
    if (g_context == nullptr || logical_history.empty()) {
        LOGe("DMC_RUNTIME cannot rebuild an empty or unavailable context");
        return 1;
    }

    try {
        const auto plan = dmc::plan_runtime_context(
            logical_history.size(),
            physical_token_budget,
            runtime_dmc_config());
        if (plan.token_ids.empty()) {
            LOGe("DMC_RUNTIME produced an empty token plan");
            return 2;
        }

        llama_tokens selected_tokens;
        selected_tokens.reserve(plan.token_ids.size());
        for (const auto history_index : plan.token_ids) {
            selected_tokens.push_back(logical_history.at(history_index));
        }

        llama_memory_clear(llama_get_memory(g_context), false);
        if (decode_tokens_in_batches(
                g_context,
                g_batch,
                selected_tokens,
                0,
                compute_last_logit)) {
            LOGe("DMC_RUNTIME failed to rehydrate selected KV context");
            return 3;
        }

        current_position = static_cast<llama_pos>(selected_tokens.size());
        g_dmc_token_history = logical_history;
        g_dmc_has_compacted = plan.compacted;
        g_dmc_rebuild_count++;
        LOGi("DMC_RUNTIME rebuild=%llu logical=%d selected=%d spans=%d physical_budget=%d compacted=%s",
             (unsigned long long) g_dmc_rebuild_count,
             (int) logical_history.size(),
             (int) selected_tokens.size(),
             (int) plan.spans.size(),
             (int) physical_token_budget,
             plan.compacted ? "true" : "false");
        return 0;
    } catch (const std::exception &e) {
        LOGe("DMC_RUNTIME rebuild failed: %s", e.what());
        return 4;
    }
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_arm_aichat_internal_InferenceEngineImpl_processSystemPrompt(
        JNIEnv *env,
        jobject /*unused*/,
        jstring jsystem_prompt
) {
    try {
        // Reset long-term & short-term states
        reset_long_term_states();
        reset_short_term_states();

        // Obtain system prompt from JEnv
        const auto *system_prompt = env->GetStringUTFChars(jsystem_prompt, nullptr);
        LOGd("%s: System prompt received: \n%s", __func__, system_prompt);
        std::string system_prompt_text(system_prompt);
        env->ReleaseStringUTFChars(jsystem_prompt, system_prompt);

        // Format system prompt if applicable
        const bool has_chat_template = common_chat_templates_was_explicit(g_chat_templates.get());
        std::string formatted_system_prompt(system_prompt_text);
        if (has_chat_template) {
            formatted_system_prompt = format_chat_message(ROLE_SYSTEM, system_prompt_text);
        }

        // Tokenize system prompt
        const auto system_tokens = common_tokenize(g_context, formatted_system_prompt,
                                                   has_chat_template, has_chat_template);
        for (auto id: system_tokens) {
            LOGv("token: `%s`\t -> `%d`", common_token_to_piece(g_context, id).c_str(), id);
        }

        g_dmc_system_tokens = system_tokens.size();
        const int physical_budget = std::max(
            1,
            runtime_context_limit(g_context) - DMC_MIN_OUTPUT_RESERVE);
        if ((int) system_tokens.size() > physical_budget) {
            const int rebuild_result = rebuild_dmc_context(
                system_tokens,
                static_cast<std::size_t>(physical_budget),
                false);
            if (rebuild_result != 0) {
                LOGe("%s: DMC system-prompt rebuild failed: %d", __func__, rebuild_result);
                return 1;
            }
        } else {
            if (decode_tokens_in_batches(g_context, g_batch, system_tokens, current_position)) {
                LOGe("%s: llama_decode() failed!", __func__);
                return 2;
            }
            current_position = static_cast<llama_pos>(system_tokens.size());
            g_dmc_token_history = system_tokens;
            LOGi("DMC_RUNTIME dense system prefix: logical=%d physical=%d",
                 (int) g_dmc_token_history.size(),
                 (int) current_position);
        }

        system_prompt_position = current_position;
        return 0;
    } catch (const std::exception &e) {
        LOGe("%s: failed: %s", __func__, e.what());
        return 3;
    }
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_arm_aichat_internal_InferenceEngineImpl_processUserPrompt(
        JNIEnv *env,
        jobject /*unused*/,
        jstring juser_prompt,
        jint n_predict
) {
    try {
        // Reset short-term states
        reset_short_term_states();

        // Obtain and tokenize user prompt
        const auto *const user_prompt = env->GetStringUTFChars(juser_prompt, nullptr);
        LOGd("%s: User prompt received: \n%s", __func__, user_prompt);
        std::string user_prompt_text(user_prompt);
        env->ReleaseStringUTFChars(juser_prompt, user_prompt);

        // Format user prompt if applicable
        const bool has_chat_template = common_chat_templates_was_explicit(g_chat_templates.get());
        std::string formatted_user_prompt(user_prompt_text);
        if (has_chat_template) {
            formatted_user_prompt = format_chat_message(ROLE_USER, user_prompt_text);
        }

        // Decode formatted user prompts
        auto user_tokens = common_tokenize(g_context, formatted_user_prompt, has_chat_template, has_chat_template);
        for (auto id: user_tokens) {
            LOGv("token: `%s`\t -> `%d`", common_token_to_piece(g_context, id).c_str(), id);
        }

        llama_tokens logical_history = g_dmc_token_history;
        logical_history.insert(logical_history.end(), user_tokens.begin(), user_tokens.end());

        const int context_limit = runtime_context_limit(g_context);
        const int requested_output_reserve = n_predict < 0
            ? DMC_CONTINUATION_RESERVE
            : std::max(DMC_MIN_OUTPUT_RESERVE, (int) n_predict);
        const int output_reserve = std::min(
            requested_output_reserve,
            std::max(1, context_limit / 2));
        const int physical_prompt_budget = std::max(1, context_limit - output_reserve);
        const bool logical_history_exceeds_budget =
            logical_history.size() > static_cast<std::size_t>(physical_prompt_budget);
        const bool physical_history_exceeds_budget =
            current_position + static_cast<llama_pos>(user_tokens.size()) > physical_prompt_budget;
        const bool use_dmc_selection =
            logical_history_exceeds_budget && physical_history_exceeds_budget;

        if (use_dmc_selection) {
            const int rebuild_result = rebuild_dmc_context(
                logical_history,
                static_cast<std::size_t>(physical_prompt_budget),
                true);
            if (rebuild_result != 0) {
                LOGe("%s: DMC user-prompt rebuild failed: %d", __func__, rebuild_result);
                return 1;
            }
        } else {
            if (decode_tokens_in_batches(g_context, g_batch, user_tokens, current_position, true)) {
                LOGe("%s: llama_decode() failed!", __func__);
                return 2;
            }
            current_position += static_cast<llama_pos>(user_tokens.size());
            g_dmc_token_history = std::move(logical_history);
            LOGi("DMC_RUNTIME incremental turn: mode=%s logical=%d physical=%d budget=%d",
                 g_dmc_has_compacted ? "selected-reuse" : "dense",
                 (int) g_dmc_token_history.size(),
                 (int) current_position,
                 physical_prompt_budget);
        }

        // A negative value means generate until model EOG. The physical KV
        // window is compacted by DMC during generation and is not an output cap.
        generation_token_limit = n_predict > 0 ? n_predict : -1;
        LOGi("GENERATION limit=%lld (%s)",
             (long long) generation_token_limit,
             generation_token_limit < 0 ? "model-eog" : "explicit");
        return 0;
    } catch (const std::exception &e) {
        LOGe("%s: failed: %s", __func__, e.what());
        return 3;
    }
}

static bool is_valid_utf8(const char *string) {
    if (!string) { return true; }

    const auto *bytes = (const unsigned char *) string;
    int num;

    while (*bytes != 0x00) {
        if ((*bytes & 0x80) == 0x00) {
            // U+0000 to U+007F
            num = 1;
        } else if ((*bytes & 0xE0) == 0xC0) {
            // U+0080 to U+07FF
            num = 2;
        } else if ((*bytes & 0xF0) == 0xE0) {
            // U+0800 to U+FFFF
            num = 3;
        } else if ((*bytes & 0xF8) == 0xF0) {
            // U+10000 to U+10FFFF
            num = 4;
        } else {
            return false;
        }

        bytes += 1;
        for (int i = 1; i < num; ++i) {
            if ((*bytes & 0xC0) != 0x80) {
                return false;
            }
            bytes += 1;
        }
    }
    return true;
}

extern "C"
JNIEXPORT jstring JNICALL
Java_com_arm_aichat_internal_InferenceEngineImpl_generateNextToken(
        JNIEnv *env,
        jobject /*unused*/
) {
    if (generation_token_limit >= 0 && generated_token_count >= generation_token_limit) {
        LOGi("GENERATION explicit token limit reached: %lld", (long long) generation_token_limit);
        finalize_assistant_message();
        return nullptr;
    }

    // The canonical history is larger than the mobile KV cache. Rehydrate a
    // DMC-selected context and continue from its final token instead of
    // silently truncating a visible answer at the physical context boundary.
    const int context_limit = runtime_context_limit(g_context);
    if (current_position >= context_limit) {
        const int continuation_budget = std::max(1, context_limit - DMC_CONTINUATION_RESERVE);
        LOGi("DMC_RUNTIME generation continuation: logical=%d physical=%d budget=%d",
             (int) g_dmc_token_history.size(),
             (int) current_position,
             continuation_budget);
        const int rebuild_result = rebuild_dmc_context(
            g_dmc_token_history,
            static_cast<std::size_t>(continuation_budget),
            true);
        if (rebuild_result != 0 || current_position >= context_limit) {
            LOGe("%s: DMC continuation rebuild failed: result=%d physical=%d limit=%d",
                 __func__, rebuild_result, (int) current_position, context_limit);
            jclass exception_class = env->FindClass("java/lang/IllegalStateException");
            if (exception_class != nullptr) {
                env->ThrowNew(exception_class, "DMC could not continue generation after KV compaction");
            }
            return nullptr;
        }
    }

    // Sample next token
    const auto new_token_id = common_sampler_sample(g_sampler, g_context, -1);
    common_sampler_accept(g_sampler, new_token_id, true);

    // Populate the batch with new token, then decode
    common_batch_clear(g_batch);
    common_batch_add(g_batch, new_token_id, current_position, {0}, true);
    if (llama_decode(g_context, g_batch) != 0) {
        LOGe("%s: llama_decode() failed for generated token", __func__);
        return nullptr;
    }

    // Preserve every decoded token in the canonical logical history, including
    // EOG. DMC can therefore rebuild the exact same chat-template stream later.
    g_dmc_token_history.push_back(new_token_id);
    current_position++;

    // Stop if next token is EOG
    if (llama_vocab_is_eog(llama_model_get_vocab(g_model), new_token_id)) {
        LOGi("GENERATION completed by model EOG after %lld tokens", (long long) generated_token_count);
        finalize_assistant_message();
        return nullptr;
    }

    generated_token_count++;

    // If not EOG, convert to text
    auto new_token_chars = common_token_to_piece(g_context, new_token_id);
    if (is_hidden_output_piece(new_token_id, new_token_chars)) {
        LOGv("id: %d,\thidden piece: `%s`", new_token_id, new_token_chars.c_str());
        return env->NewStringUTF("");
    }
    cached_token_chars += new_token_chars;

    // Create and return a valid UTF-8 Java string
    jstring result = nullptr;
    if (is_valid_utf8(cached_token_chars.c_str())) {
        result = env->NewStringUTF(cached_token_chars.c_str());
        LOGv("id: %d,\tcached: `%s`,\tnew: `%s`", new_token_id, cached_token_chars.c_str(), new_token_chars.c_str());

        assistant_ss << cached_token_chars;
        cached_token_chars.clear();
    } else {
        LOGv("id: %d,\tappend to cache", new_token_id);
        result = env->NewStringUTF("");
    }
    return result;
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_arm_aichat_internal_InferenceEngineImpl_warmupModel(JNIEnv *env, jobject /*unused*/) {
    if (g_model == nullptr || g_context == nullptr || !g_batch_initialized || g_sampler == nullptr) {
        LOGw("%s: warmup skipped because the model is not ready", __func__);
        return 1;
    }

    const bool previous_enable_thinking = g_enable_thinking;
    auto restore_runtime_state = [&]() -> bool {
        reset_long_term_states();
        reset_short_term_states();

        common_sampler *replacement_sampler = new_sampler(DEFAULT_SAMPLER_TEMP);
        if (!replacement_sampler) {
            LOGe("%s: failed to recreate sampler after warmup", __func__);
            g_enable_thinking = previous_enable_thinking;
            return false;
        }

        if (g_sampler) {
            common_sampler_free(g_sampler);
        }
        g_sampler = replacement_sampler;
        g_enable_thinking = previous_enable_thinking;
        return true;
    };

    try {
        LOGi("%s: Starting model warmup", __func__);
        g_enable_thinking = false;

        reset_long_term_states();
        reset_short_term_states();

        jstring warmup_prompt = env->NewStringUTF("Warmup");
        if (warmup_prompt == nullptr) {
            LOGe("%s: failed to allocate warmup prompt", __func__);
            return restore_runtime_state() ? 0 : 2;
        }

        const jint process_result =
            Java_com_arm_aichat_internal_InferenceEngineImpl_processUserPrompt(env, nullptr, warmup_prompt, 1);
        env->DeleteLocalRef(warmup_prompt);

        if (process_result != 0) {
            LOGw("%s: warmup prompt processing returned %d", __func__, process_result);
        } else {
            jstring token = Java_com_arm_aichat_internal_InferenceEngineImpl_generateNextToken(env, nullptr);
            if (token != nullptr) {
                env->DeleteLocalRef(token);
            }
        }

        if (!restore_runtime_state()) {
            return 3;
        }

        LOGi("%s: Model warmup complete", __func__);
        return 0;
    } catch (const std::exception &e) {
        LOGe("%s: warmup failed: %s", __func__, e.what());
        return restore_runtime_state() ? 0 : 4;
    }
}


extern "C"
JNIEXPORT void JNICALL
Java_com_arm_aichat_internal_InferenceEngineImpl_unload(JNIEnv * /*unused*/, jobject /*unused*/) {
    // Reset long-term & short-term states
    reset_long_term_states();
    reset_short_term_states();

    // Free up resources
    if (g_sampler) {
        common_sampler_free(g_sampler);
        g_sampler = nullptr;
    }
    g_enable_thinking = false;
    g_chat_templates.reset();
    if (g_batch_initialized) {
        llama_batch_free(g_batch);
        g_batch = {};
        g_batch_initialized = false;
    }
    if (g_context) {
        llama_free(g_context);
        g_context = nullptr;
    }
    if (g_model) {
        llama_model_free(g_model);
        g_model = nullptr;
    }
    g_dmc_logical_context_window = 0;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_arm_aichat_internal_InferenceEngineImpl_shutdown(JNIEnv *, jobject /*unused*/) {
    llama_backend_free();
}
