from pathlib import Path


def test_run_phone_bat_prepares_lan_and_starts_run():
    script = Path("run-phone.bat").read_text(encoding="utf-8")

    assert "open-llama-lan-8080.ps1" in script
    assert "run.bat" in script
    assert "Preparing LAN access" in script
    assert "-Use64KContext" in script
