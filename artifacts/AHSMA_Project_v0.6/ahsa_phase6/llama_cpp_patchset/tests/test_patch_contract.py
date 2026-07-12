from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def test_three_ordered_patches():
    names = [p.name for p in sorted((ROOT/"patches").glob("*.patch"))]
    assert names == [
        "0001-ahsma-build-and-fused-op.patch",
        "0002-ahsma-context-lifecycle.patch",
        "0003-ahsma-safe-accessor.patch",
    ]

def test_disabled_by_default_contract():
    text = (ROOT/"patches"/"0002-ahsma-context-lifecycle.patch").read_text()
    assert 'getenv("LLAMA_AHSMA")' in text
    assert "cparams.ahsma_enabled" in text
    assert "LLAMA_AHSMA=1" not in text  # code does not force enablement

def test_no_attention_replacement_yet():
    all_text = "\n".join(p.read_text() for p in (ROOT/"patches").glob("*.patch"))
    assert "ggml_flash_attn_ext" not in all_text
    assert "GGML_OP_AHSMA_ATTN" not in all_text

def test_new_sources_present():
    assert (ROOT/"new_files"/"llama-ahsma.h").exists()
    assert (ROOT/"new_files"/"llama-ahsma.cpp").exists()
