param(
    [string]$RemoteSubnet = "LocalSubnet"
)

$ErrorActionPreference = "Stop"

function Test-IsAdministrator {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($identity)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

if (-not (Test-IsAdministrator)) {
    $powershellExe = Join-Path $PSHOME "powershell.exe"
    $args = @(
        "-NoProfile"
        "-ExecutionPolicy"
        "Bypass"
        "-File"
        "`"$PSCommandPath`""
        "-RemoteSubnet"
        $RemoteSubnet
    )
    Start-Process -FilePath $powershellExe -Verb RunAs -ArgumentList $args
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

function Get-LanIPv4Addresses {
    (& ipconfig) |
        Select-String -Pattern '(\d{1,3}\.){3}\d{1,3}' |
        ForEach-Object { $_.Matches[0].Value } |
        Where-Object { $_ -ne "127.0.0.1" -and $_ -notlike "169.254*" } |
        Select-Object -Unique
}

Write-Host "Firewall rule created: $ruleName"
Write-Host "Allowed subnet: $RemoteSubnet"
foreach ($ip in Get-LanIPv4Addresses) {
    Write-Host "Open on phone: http://$ip`:8080/"
}
