param(
    [Parameter(Mandatory=$true)][string]$Setup,
    [Parameter(Mandatory=$true)][string]$InstallRoot
)
$ErrorActionPreference = 'Stop'
# This mutates installer registration: run only on an ephemeral hosted CI runner.
if ($env:GITHUB_ACTIONS -ne 'true') { throw 'Installer lifecycle test requires an isolated GitHub Actions runner.' }
$testInstall = [IO.Path]::GetFullPath($InstallRoot)
$runnerRoot = [IO.Path]::GetFullPath($env:RUNNER_TEMP).TrimEnd('\') + '\'
if (!$testInstall.StartsWith($runnerRoot, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Installation test must stay inside RUNNER_TEMP.'
}
if (Test-Path -LiteralPath $testInstall) { throw 'Installation test directory must be fresh.' }
$testData = Join-Path $env:LOCALAPPDATA 'AlfaRaceX\Desktop'
if (Test-Path -LiteralPath $testData) { throw 'Existing application data must not be touched by this test.' }
$setupExe = (Resolve-Path -LiteralPath $Setup).Path

function Run-TestProcess([string]$File, [string[]]$Arguments, [int]$Timeout=120000) {
    Write-Output "RUN: $File $Arguments"
    $proc = Start-Process -FilePath $File -ArgumentList $Arguments -WindowStyle Hidden -PassThru
    if (!$proc.WaitForExit($Timeout)) {
        Stop-Process -Id $proc.Id
        throw "Timeout: $File"
    }
    if ($proc.ExitCode -ne 0) { throw "$File returned $($proc.ExitCode)" }
    Write-Output "PASS: $File $Arguments"
}
function Install-TestApp {
    Run-TestProcess $setupExe @('--silent','--installto',('"' + $testInstall + '"'))
    if (!(Test-Path -LiteralPath (Join-Path $testInstall 'current\AlfaRaceX.exe'))) {
        throw 'Installed application missing.'
    }
}
function Assert-FixtureUnchanged {
    foreach ($entry in $fixtureHashes.GetEnumerator()) {
        if (!(Test-Path -LiteralPath $entry.Key) -or
            (Get-FileHash -LiteralPath $entry.Key -Algorithm SHA256).Hash -ne $entry.Value) {
            throw "Persistent fixture changed or disappeared: $($entry.Key)"
        }
    }
}

Install-TestApp
$installedExe = Join-Path $testInstall 'current\AlfaRaceX.exe'
Run-TestProcess $installedExe @('--ui-smoke-test') 60000

# Copy a database produced by the real UI/disclaimer test into the normal data
# location. These are synthetic CI records, never a user's backups or database.
$smokeRoot = Get-ChildItem -LiteralPath ([IO.Path]::GetTempPath()) -Directory -Filter 'AlfaRaceX-Smoke-*' |
    Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName 'ui-smoke-result.txt') } |
    Sort-Object LastWriteTime -Descending | Select-Object -First 1
if (!$smokeRoot) { throw 'UI smoke evidence missing.' }
if ((Get-Content -LiteralPath (Join-Path $smokeRoot.FullName 'ui-smoke-result.txt') -Raw) -notlike 'PASS:*') {
    throw 'UI smoke evidence does not report PASS.'
}
New-Item -ItemType Directory -Path (Join-Path $testData 'Backups') -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $smokeRoot.FullName 'alfaracex.db') -Destination $testData
Set-Content -LiteralPath (Join-Path $testData 'Backups\ci-preservation-fixture.txt') -Value 'Synthetic CI fixture; not firmware or a device backup.'
$fixtureHashes = @{}
Get-ChildItem -LiteralPath $testData -File -Recurse | ForEach-Object {
    $fixtureHashes[$_.FullName] = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
}

Install-TestApp
Assert-FixtureUnchanged
Run-TestProcess $installedExe @('--ui-smoke-test') 60000
Run-TestProcess (Join-Path $testInstall 'Update.exe') @('--silent','uninstall')
Assert-FixtureUnchanged
if (Test-Path -LiteralPath $installedExe) { throw 'Application executable remains after uninstall.' }
Install-TestApp
Assert-FixtureUnchanged
Run-TestProcess $installedExe @('--ui-smoke-test') 60000
Write-Output 'PASS: install, actual UI, same-version reinstall, uninstall, reinstall; database and backup fixture SHA-256 unchanged.'
