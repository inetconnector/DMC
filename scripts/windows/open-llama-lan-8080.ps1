param(
    [string]$RemoteSubnet = "192.168.1.0/24"
)

$ErrorActionPreference = "Stop"

function Test-IsAdministrator {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($identity)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

if (-not (Test-IsAdministrator)) {
    $args = @(
        "-NoProfile"
        "-ExecutionPolicy"
        "Bypass"
        "-File"
        "`"$PSCommandPath`""
        "-RemoteSubnet"
        $RemoteSubnet
    )
    Start-Process -FilePath "powershell.exe" -Verb RunAs -ArgumentList $args
    exit
}

$ruleName = "DMC llama.cpp LAN 8080"

if (Get-NetFirewallRule -DisplayName $ruleName -ErrorAction SilentlyContinue) {
    Remove-NetFirewallRule -DisplayName $ruleName
}

New-NetFirewallRule `
    -DisplayName $ruleName `
    -Direction Inbound `
    -Action Allow `
    -Protocol TCP `
    -LocalPort 8080 `
    -Profile Private `
    -RemoteAddress $RemoteSubnet | Out-Null

Write-Host "Firewall rule created: $ruleName"
Write-Host "Allowed subnet: $RemoteSubnet"
Write-Host "Open on phone: http://192.168.1.94:8080/"
