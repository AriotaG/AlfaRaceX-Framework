$ErrorActionPreference = "Stop"

$root = Join-Path $PSScriptRoot "AlfaRaceX.Updater"
$branding = Join-Path $PSScriptRoot "branding"
$assets = Join-Path $root "Assets"
New-Item -ItemType Directory -Force -Path $assets | Out-Null

$iconPng = Join-Path $assets "ARX-256.png"
$iconIco = Join-Path $assets "ARX.ico"
$splashPng = Join-Path $assets "AlfaRaceX-Splash.png"
$splashJpg = Join-Path $assets "AlfaRaceX-Splash.jpg"
$heroPng = Join-Path $assets "ARX-Hero.png"
$heroJpg = Join-Path $assets "ARX-Hero.jpg"
$watermarkJpg = Join-Path $assets "ARX-Watermark.jpg"
$reference = Join-Path $PSScriptRoot "..\docs\images\alfaracex-updater-dashboard.jpg"

rsvg-convert -w 256 -h 256 (Join-Path $branding "ARX.svg") -o $iconPng
convert $iconPng -define icon:auto-resize=256,128,64,48,32,16 $iconIco

rsvg-convert -w 960 -h 540 (Join-Path $branding "Splash.svg") -o $splashPng
convert $splashPng -quality 90 $splashJpg

if (-not (Test-Path $reference)) {
    throw "Reference dashboard image not found: $reference"
}

# Static artwork comes directly from the approved reference screenshot.
convert $reference -crop "562x251+838+35" +repage -resize "720x280^" -gravity center -extent 720x280 $heroPng
convert $heroPng -quality 92 $heroJpg

convert $reference -crop "220x200+35+575" +repage -resize "220x200!" -quality 92 $watermarkJpg

Write-Host "AlfaRaceX branding assets prepared."
