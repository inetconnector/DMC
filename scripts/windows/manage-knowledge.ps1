param(
    [string]$KnowledgeRoot = (Join-Path $PSScriptRoot "..\..\runtime\knowledge"),
    [string]$CatalogPath = (Join-Path $PSScriptRoot "..\..\knowledge\source-catalog.json")
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

function Resolve-Python {
    foreach ($candidate in @("python", "py")) {
        $command = Get-Command $candidate -ErrorAction SilentlyContinue
        if ($command) {
            return $command.Source
        }
    }
    foreach ($candidate in @(
        (Join-Path $env:LOCALAPPDATA "Programs\Python\Python313\python.exe"),
        (Join-Path $env:LOCALAPPDATA "Programs\Python\Python312\python.exe"),
        (Join-Path $env:LOCALAPPDATA "Programs\Python\Python311\python.exe"),
        "C:\Program Files\Python313\python.exe",
        "C:\Program Files\Python312\python.exe",
        "C:\Program Files\Python311\python.exe"
    )) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            return $candidate
        }
    }
    throw "Python 3.10 or newer is required for Windows knowledge modules."
}

function Invoke-KnowledgeRuntime {
    param([Parameter(Mandatory = $true)][string[]]$Arguments)
    $output = & $script:Python $script:RuntimeScript @Arguments 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw ($output | Out-String).Trim()
    }
    return ($output | Out-String).Trim()
}

function Get-CheckedSourceIds {
    $ids = @()
    foreach ($index in $script:SourceList.CheckedIndices) {
        $ids += $script:Sources[[int]$index].id
    }
    return $ids
}

function Save-Selection {
    $selection = [ordered]@{
        formatVersion = 1
        selectedSourceIds = @(Get-CheckedSourceIds)
        updatedAt = (Get-Date).ToUniversalTime().ToString("o")
    }
    $selection | ConvertTo-Json -Depth 4 |
        Set-Content -LiteralPath $script:SelectionPath -Encoding UTF8
    $script:StatusLabel.Text = "Auswahl gespeichert: $($selection.selectedSourceIds.Count) Quellen"
}

function Selected-Source {
    $index = $script:SourceList.SelectedIndex
    if ($index -lt 0) {
        [System.Windows.Forms.MessageBox]::Show(
            "Bitte zuerst eine Quelle in der Liste markieren.",
            "InetMind Wissensquellen",
            [System.Windows.Forms.MessageBoxButtons]::OK,
            [System.Windows.Forms.MessageBoxIcon]::Information
        ) | Out-Null
        return $null
    }
    return $script:Sources[$index]
}

function Open-SelectedSource {
    $source = Selected-Source
    if (-not $source) {
        return
    }
    Start-Process $source.officialUrl
    $script:StatusLabel.Text = "Offizielle Quelle geöffnet: $($source.name)"
}

function Import-SourceFile {
    $source = Selected-Source
    if (-not $source) {
        return
    }

    $picker = New-Object System.Windows.Forms.OpenFileDialog
    $picker.Title = "Offizielle Quelldatei oder DMC-Wissenspaket auswählen"
    $picker.Filter = "Wissensquellen|*.dmcknowledge;*.zip;*.xml;*.csv;*.txt;*.jsonl|Alle Dateien|*.*"
    $picker.Multiselect = $false
    if ($picker.ShowDialog() -ne [System.Windows.Forms.DialogResult]::OK) {
        return
    }

    $packagePath = $picker.FileName
    $temporaryPackage = $null
    try {
        if ([IO.Path]::GetExtension($packagePath) -ne ".dmcknowledge") {
            $temporaryPackage = Join-Path ([IO.Path]::GetTempPath()) (
                "inetmind-$($source.id)-$([guid]::NewGuid().ToString('N')).dmcknowledge"
            )
            $language = if ($source.id -in @("who.icd11", "ncbi.pubmed", "ncbi.pmc-oa-commercial",
                    "clinicaltrials.gov", "dailymed.spl", "openfda.drug", "ema.pms",
                    "snomed.international")) { "en" } else { "de" }
            $jurisdiction = if ($source.id -like "bfarm.*" -or $source.id -eq "awmf.guidelines") {
                "DE"
            } elseif ($source.id -in @("dailymed.spl", "openfda.drug", "clinicaltrials.gov")) {
                "US"
            } elseif ($source.id -eq "ema.pms") {
                "EU"
            } else {
                "international"
            }
            $convertArguments = @(
                "--catalog", $script:CatalogPath,
                "--source-id", $source.id,
                "--input", $packagePath,
                "--output", $temporaryPackage,
                "--language", $language,
                "--jurisdiction", $jurisdiction,
                "--confirm-lawful-source"
            )
            $converted = & $script:Python $script:PrepareScript @convertArguments 2>&1
            if ($LASTEXITCODE -ne 0) {
                throw ($converted | Out-String).Trim()
            }
            $packagePath = $temporaryPackage
        }

        $message = Invoke-KnowledgeRuntime -Arguments @(
            "install", "--root", $script:KnowledgeRoot, $packagePath
        )
        Refresh-InstalledModules
        $script:StatusLabel.Text = $message
        [System.Windows.Forms.MessageBox]::Show(
            "$message`n`nDas Modul wird bei der nächsten Anfrage automatisch zusammen mit allen anderen aktivierten Modulen berücksichtigt.",
            "Import abgeschlossen",
            [System.Windows.Forms.MessageBoxButtons]::OK,
            [System.Windows.Forms.MessageBoxIcon]::Information
        ) | Out-Null
    } catch {
        [System.Windows.Forms.MessageBox]::Show(
            $_.Exception.Message,
            "Import fehlgeschlagen",
            [System.Windows.Forms.MessageBoxButtons]::OK,
            [System.Windows.Forms.MessageBoxIcon]::Error
        ) | Out-Null
    } finally {
        if ($temporaryPackage -and (Test-Path -LiteralPath $temporaryPackage)) {
            Remove-Item -LiteralPath $temporaryPackage -Force
        }
    }
}

function Refresh-InstalledModules {
    $script:InstalledList.Items.Clear()
    $json = Invoke-KnowledgeRuntime -Arguments @("list", "--root", $script:KnowledgeRoot, "--json")
    $modules = if ($json) { @($json | ConvertFrom-Json) } else { @() }
    foreach ($module in $modules) {
        $state = if ($module.enabled) { "aktiv" } else { "inaktiv" }
        $script:InstalledList.Items.Add(
            "$($module.name) $($module.version) | $($module.record_count) Einträge | $state"
        ) | Out-Null
    }
    $script:InstalledModules = $modules
    $script:InstalledSummary.Text = if ($modules.Count) {
        "$($modules.Count) installierte Module. Alle aktivierten Module werden gemeinsam durchsucht."
    } else {
        "Noch keine Module installiert. Quelle auswählen, Bedingungen prüfen und Datei importieren."
    }
}

function Toggle-InstalledModule {
    $index = $script:InstalledList.SelectedIndex
    if ($index -lt 0) {
        return
    }
    $module = $script:InstalledModules[$index]
    $command = if ($module.enabled) { "disable" } else { "enable" }
    Invoke-KnowledgeRuntime -Arguments @(
        $command, "--root", $script:KnowledgeRoot, $module.id
    ) | Out-Null
    Refresh-InstalledModules
}

$script:Python = Resolve-Python
$script:CatalogPath = (Resolve-Path -LiteralPath $CatalogPath).Path
$script:RuntimeScript = (Resolve-Path -LiteralPath (
    Join-Path $PSScriptRoot "..\knowledge\knowledge_runtime.py"
)).Path
$script:PrepareScript = (Resolve-Path -LiteralPath (
    Join-Path $PSScriptRoot "..\knowledge\prepare_knowledge_file.py"
)).Path
New-Item -ItemType Directory -Path $KnowledgeRoot -Force | Out-Null
$script:KnowledgeRoot = (Resolve-Path -LiteralPath $KnowledgeRoot).Path
$script:SelectionPath = Join-Path $script:KnowledgeRoot "source-selection.json"

Invoke-KnowledgeRuntime -Arguments @("validate-catalog", $script:CatalogPath) | Out-Null
$catalog = Get-Content -LiteralPath $script:CatalogPath -Raw | ConvertFrom-Json
$script:Sources = @($catalog.sources | Where-Object { $_.windows })

$savedIds = @()
if (Test-Path -LiteralPath $script:SelectionPath) {
    try {
        $savedIds = @((Get-Content -LiteralPath $script:SelectionPath -Raw |
            ConvertFrom-Json).selectedSourceIds)
    } catch {
        $savedIds = @()
    }
}
if (-not $savedIds.Count) {
    $savedIds = @($script:Sources | Where-Object defaultSelected | ForEach-Object id)
}

$form = New-Object System.Windows.Forms.Form
$form.Text = "InetMind - medizinische Offline-Wissensquellen"
$form.Size = New-Object System.Drawing.Size(920, 680)
$form.MinimumSize = New-Object System.Drawing.Size(760, 560)
$form.StartPosition = "CenterScreen"
$form.Font = New-Object System.Drawing.Font("Segoe UI", 10)

$intro = New-Object System.Windows.Forms.Label
$intro.Text = "Offizielle Quellen auswählen. Ein Häkchen bedeutet Empfehlung und Update-Beobachtung, nicht automatische Lizenzannahme. Daten bleiben nach dem Import lokal."
$intro.Location = New-Object System.Drawing.Point(18, 16)
$intro.Size = New-Object System.Drawing.Size(865, 44)
$intro.Anchor = "Top,Left,Right"
$form.Controls.Add($intro)

$script:SourceList = New-Object System.Windows.Forms.CheckedListBox
$script:SourceList.CheckOnClick = $true
$script:SourceList.Location = New-Object System.Drawing.Point(18, 68)
$script:SourceList.Size = New-Object System.Drawing.Size(865, 320)
$script:SourceList.Anchor = "Top,Left,Right"
$script:SourceList.HorizontalScrollbar = $true
for ($index = 0; $index -lt $script:Sources.Count; $index++) {
    $source = $script:Sources[$index]
    $label = "$($source.name) | $($source.publisher) | $($source.category) | $($source.access)"
    $script:SourceList.Items.Add($label, ($source.id -in $savedIds)) | Out-Null
}
$form.Controls.Add($script:SourceList)

$saveButton = New-Object System.Windows.Forms.Button
$saveButton.Text = "Auswahl speichern"
$saveButton.Location = New-Object System.Drawing.Point(18, 402)
$saveButton.Size = New-Object System.Drawing.Size(170, 38)
$saveButton.Add_Click({ Save-Selection })
$form.Controls.Add($saveButton)

$openButton = New-Object System.Windows.Forms.Button
$openButton.Text = "Quelle / Update öffnen"
$openButton.Location = New-Object System.Drawing.Point(200, 402)
$openButton.Size = New-Object System.Drawing.Size(190, 38)
$openButton.Add_Click({ Open-SelectedSource })
$form.Controls.Add($openButton)

$importButton = New-Object System.Windows.Forms.Button
$importButton.Text = "Datei importieren"
$importButton.Location = New-Object System.Drawing.Point(402, 402)
$importButton.Size = New-Object System.Drawing.Size(170, 38)
$importButton.Add_Click({ Import-SourceFile })
$form.Controls.Add($importButton)

$script:InstalledSummary = New-Object System.Windows.Forms.Label
$script:InstalledSummary.Location = New-Object System.Drawing.Point(18, 458)
$script:InstalledSummary.Size = New-Object System.Drawing.Size(865, 28)
$script:InstalledSummary.Anchor = "Top,Left,Right"
$form.Controls.Add($script:InstalledSummary)

$script:InstalledList = New-Object System.Windows.Forms.ListBox
$script:InstalledList.Location = New-Object System.Drawing.Point(18, 490)
$script:InstalledList.Size = New-Object System.Drawing.Size(700, 96)
$script:InstalledList.Anchor = "Top,Bottom,Left,Right"
$form.Controls.Add($script:InstalledList)

$toggleButton = New-Object System.Windows.Forms.Button
$toggleButton.Text = "Aktivieren / deaktivieren"
$toggleButton.Location = New-Object System.Drawing.Point(730, 490)
$toggleButton.Size = New-Object System.Drawing.Size(153, 42)
$toggleButton.Anchor = "Top,Right"
$toggleButton.Add_Click({ Toggle-InstalledModule })
$form.Controls.Add($toggleButton)

$refreshButton = New-Object System.Windows.Forms.Button
$refreshButton.Text = "Neu laden"
$refreshButton.Location = New-Object System.Drawing.Point(730, 544)
$refreshButton.Size = New-Object System.Drawing.Size(153, 42)
$refreshButton.Anchor = "Top,Right"
$refreshButton.Add_Click({ Refresh-InstalledModules })
$form.Controls.Add($refreshButton)

$script:StatusLabel = New-Object System.Windows.Forms.Label
$script:StatusLabel.Text = "Bereit"
$script:StatusLabel.Location = New-Object System.Drawing.Point(18, 606)
$script:StatusLabel.Size = New-Object System.Drawing.Size(865, 24)
$script:StatusLabel.Anchor = "Bottom,Left,Right"
$form.Controls.Add($script:StatusLabel)

Refresh-InstalledModules
[void]$form.ShowDialog()
