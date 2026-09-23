$ErrorActionPreference = "Stop"

$root = Join-Path $PSScriptRoot "AlfaRaceX.Updater"
$branding = Join-Path $PSScriptRoot "branding"
$assets = Join-Path $root "Assets"
New-Item -ItemType Directory -Force -Path $assets | Out-Null

$iconPng = Join-Path $assets "ARX-256.png"
$iconIco = Join-Path $assets "ARX.ico"
$splashPng = Join-Path $assets "AlfaRaceX-Splash.png"
$splashJpg = Join-Path $assets "AlfaRaceX-Splash.jpg"

rsvg-convert -w 256 -h 256 (Join-Path $branding "ARX.svg") -o $iconPng
convert $iconPng -define icon:auto-resize=256,128,64,48,32,16 $iconIco

rsvg-convert -w 960 -h 540 (Join-Path $branding "Splash.svg") -o $splashPng
convert $splashPng -quality 90 $splashJpg

Write-Host "AlfaRaceX branding assets prepared."
