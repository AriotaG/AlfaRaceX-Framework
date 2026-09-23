$ErrorActionPreference = "Stop"

$root = Join-Path $PSScriptRoot "AlfaRaceX.Updater"
$branding = Join-Path $PSScriptRoot "branding"
$assets = Join-Path $root "Assets"
New-Item -ItemType Directory -Force -Path $assets | Out-Null

function Join-Base64Parts([string]$pattern, [string]$destination) {
    $parts = Get-ChildItem -Path $branding -Filter $pattern | Sort-Object Name
    if ($parts.Count -eq 0) { throw "Branding asset parts not found: $pattern" }
    $base64 = ($parts | ForEach-Object { (Get-Content $_.FullName -Raw).Trim() }) -join ""
    [IO.File]::WriteAllBytes($destination, [Convert]::FromBase64String($base64))
}

Join-Base64Parts "ARX.ico.b64.part*" (Join-Path $assets "ARX.ico")
Join-Base64Parts "Splash.jpg.b64.part*" (Join-Path $assets "AlfaRaceX-Splash.jpg")

Write-Host "AlfaRaceX branding assets prepared."
