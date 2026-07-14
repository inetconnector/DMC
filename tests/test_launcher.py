from pathlib import Path


def test_launcher_exposes_256k_context_and_dry_run():
    script = Path("scripts/windows/start-llama-lan.ps1").read_text(encoding="utf-8")

    assert "[switch]$Use256KContext" in script
    assert "[switch]$DryRun" in script
    assert "context-fallback" in script
    assert "[status] trying the largest available context first" in script
    assert "[status] trying context $candidateContext" in script
    assert "[status] running with context $selectedContextSize" in script
    assert "262144" in script
    assert "131072" in script
    assert "65536" in script
    assert "256k-experimental" in script
