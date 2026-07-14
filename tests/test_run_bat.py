from pathlib import Path


def test_run_bat_launches_server_and_opens_browser():
    script = Path("run.bat").read_text(encoding="utf-8")

    assert "start-llama-lan.ps1" in script
    assert "http://127.0.0.1:8080/" in script
    assert "findstr /I /C:\"-DryRun\"" in script
