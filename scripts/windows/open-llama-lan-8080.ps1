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
    Write-Host "Firewall setup skipped: administrator rights are required."
    Write-Host "Run this script from an elevated PowerShell if you want the LAN rule."
    exit 0
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
    [System.Net.NetworkInformation.NetworkInterface]::GetAllNetworkInterfaces() |
        Where-Object {
            $_.OperationalStatus -eq [System.Net.NetworkInformation.OperationalStatus]::Up -and
            $_.NetworkInterfaceType -in @(
                [System.Net.NetworkInformation.NetworkInterfaceType]::Wireless80211,
                [System.Net.NetworkInformation.NetworkInterfaceType]::Ethernet
            ) -and
            $_.Description -notmatch 'Hyper-V|WSL|Virtual|Loopback|Miniport|WAN'
        } |
        ForEach-Object {
            $properties = $_.GetIPProperties()
            if (-not $properties.GatewayAddresses) {
                return
            }

            $properties.UnicastAddresses |
                Where-Object {
                    $_.Address.AddressFamily -eq [System.Net.Sockets.AddressFamily]::InterNetwork -and
                    $_.Address.IPAddressToString -notin @("127.0.0.1") -and
                    $_.Address.IPAddressToString -notlike "169.254*"
                } |
                ForEach-Object { $_.Address.IPAddressToString }
        } |
        Sort-Object -Unique
}

Write-Host "Firewall rule created: $ruleName"
Write-Host "Allowed subnet: $RemoteSubnet"
foreach ($ip in Get-LanIPv4Addresses) {
    Write-Host "Open on phone: http://$ip`:8080/"
}
