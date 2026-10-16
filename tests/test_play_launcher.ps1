# SPDX-License-Identifier: GPL-2.0-or-later
# Runs tools/windows/play.ps1 against a stand-in game that exits with code 3 (and against a
# missing game) and checks the fault dialog text follows the saved language. CI only.
param([Parameter(Mandatory)][string]$Fake)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$work = Join-Path ([IO.Path]::GetTempPath()) ('gtn-play-' + [guid]::NewGuid())
$app = Join-Path $work 'game'
$data = Join-Path $work 'appdata'
New-Item -ItemType Directory -Force (Join-Path $data 'gtetrinet-ng\gtetrinet-ng'), $app | Out-Null
Copy-Item (Join-Path $root 'tools\windows\play.ps1') $app
Set-Content (Join-Path $app 'VERSION') 'vtest'
$env:APPDATA = $data; $env:GTN_NO_DIALOG = '1'; $env:GTN_NO_REPORT = '1'
$failed = 0
function Check([string]$lang, [string]$want, [int]$wantCode) {
    Set-Content (Join-Path $data 'gtetrinet-ng\gtetrinet-ng\settings.ini') "host=x`nlang=$lang"
    $out = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $app 'play.ps1') | Out-String
    $code = $LASTEXITCODE
    if ($out -notlike "*$want*" -or $code -ne $wantCode) {
        Write-Output "FAIL lang=$lang code=$code (want $wantCode): $out"; $script:failed++
    } else { Write-Output "ok lang=${lang}: $($out.Trim())" }
}
Check 'de' 'GTetrinet NG konnte nicht gestartet werden' 1
Copy-Item $Fake (Join-Path $app 'gtetrinet-ng.exe')
Check 'de' 'GTetrinet NG wurde unerwartet beendet (Code 3).' 3
Check 'en' 'GTetrinet NG closed unexpectedly (code 3).' 3
Remove-Item -Recurse -Force $work
if ($failed) { exit 1 }
