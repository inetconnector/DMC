from pathlib import Path


def test_run_phone_bat_prepares_lan_and_starts_run():
    script = Path("run-phone.bat").read_text(encoding="utf-8")

    assert "open-llama-lan-8080.ps1" in script
    assert "run.bat" in script
    assert "[status] preparing LAN access" in script
    assert "[status] dry run, skipping LAN firewall setup" in script
    assert "[status] starting DMC for phone access" in script
    assert "Preparing LAN access" in script
    assert "-Use64KContext" not in script
