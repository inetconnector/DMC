from pathlib import Path


def test_firewall_script_uses_local_subnet_and_prints_lan_ips():
    script = Path("scripts/windows/open-llama-lan-8080.ps1").read_text(encoding="utf-8")

    assert 'LocalSubnet' in script
    assert 'Get-LanIPv4Addresses' in script
    assert 'Wireless80211' in script
    assert 'Open on phone: http://$ip`:8080/' in script
