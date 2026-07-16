#include <android/log.h>
#include <jni.h>
#include <algorithm>
#include <iomanip>
#include <cmath>
#include <cstring>
#include <string>
#include <unistd.h>
#include <sampling.h>

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
    const int initial_context = std::max(requested_n_ctx > 0 ? requested_n_ctx : trained_context_size, MIN_CONTEXT_SIZE);

    auto add_candidate = [&](const int candidate) {
        if (candidate < MIN_CONTEXT_SIZE) {
            return;
        }
        if (std::find(candidates.begin(), candidates.end(), candidate) == candidates.end()) {
            candidates.push_back(candidate);
        }
    };

    add_candidate(initial_context);
    add_candidate(trained_context_size);

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
    return static_cast<jint>(runtime_context_window_size(g_context));
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

    if (clear_kv_cache && g_context)
        llama_memory_clear(llama_get_memory(g_context), false);
}

/**
 * Completion loop's short-term states:
 * - stop generation position
 * - token chars caching
 * - current assistant message being generated
 */
static llama_pos stop_generation_position;
static std::string cached_token_chars;
static std::ostringstream assistant_ss;

static void reset_short_term_states() {
    stop_generation_position = 0;
    cached_token_chars.clear();
    assistant_ss.str("");
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

        // Handle context overflow
        const int max_batch_size = runtime_context_limit(g_context);
        if ((int) system_tokens.size() > max_batch_size) {
            LOGe("%s: System prompt too long for context! %d tokens, max: %d",
                 __func__, (int) system_tokens.size(), max_batch_size);
            return 1;
        }

        // Decode system tokens in batches
        if (decode_tokens_in_batches(g_context, g_batch, system_tokens, current_position)) {
            LOGe("%s: llama_decode() failed!", __func__);
            return 2;
        }

        // Update position
        system_prompt_position = current_position = (int) system_tokens.size();
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

        // Ensure user prompt doesn't exceed the remaining context window by truncating if necessary.
        const int user_prompt_size = (int) user_tokens.size();
        const int available_context = std::max(0, runtime_context_limit(g_context) - (int) current_position);
        if (available_context == 0) {
            LOGe("%s: No context remaining for user prompt", __func__);
            return 1;
        }
        if (user_prompt_size > available_context) {
            const int skipped_tokens = user_prompt_size - available_context;
            user_tokens.resize(available_context);
            LOGw("%s: User prompt too long! Skipped %d tokens!", __func__, skipped_tokens);
        }

        // Decode user tokens in batches
        if (decode_tokens_in_batches(g_context, g_batch, user_tokens, current_position, true)) {
            LOGe("%s: llama_decode() failed!", __func__);
            return 2;
        }

        // Update the generation stop position without exceeding the native window.
        current_position += (int) user_tokens.size();
        const llama_pos generation_limit = runtime_context_limit(g_context);
        if (n_predict < 0) {
            stop_generation_position = generation_limit;
        } else {
            stop_generation_position = std::min(current_position + n_predict, generation_limit);
        }
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
    // Stop generation once the fixed mobile context window is full.
    const int context_limit = runtime_context_limit(g_context);
    if (current_position >= context_limit) {
        LOGw("%s: Context full; stopping generation at position %d", __func__, current_position);
        return nullptr;
    }

    // Stop if reaching the marked position
    if (current_position >= stop_generation_position) {
        LOGw("%s: STOP: hitting stop position: %d", __func__, stop_generation_position);
        return nullptr;
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

    // Update position
    current_position++;

    // Stop if next token is EOG
    if (llama_vocab_is_eog(llama_model_get_vocab(g_model), new_token_id)) {
        LOGd("id: %d,\tIS EOG!\nSTOP.", new_token_id);
        format_chat_message(ROLE_ASSISTANT, strip_model_tags(assistant_ss.str()));
        return nullptr;
    }

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
}

extern "C"
JNIEXPORT void JNICALL
Java_com_arm_aichat_internal_InferenceEngineImpl_shutdown(JNIEnv *, jobject /*unused*/) {
    llama_backend_free();
}
