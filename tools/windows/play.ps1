# SPDX-License-Identifier: GPL-2.0-or-later
# Kept ASCII-only: Windows PowerShell 5.1 reads a BOM-less script in the ANSI code page,
# so German letters are built from code points below.
$ErrorActionPreference = 'Stop'

function Test-German {
    # The game's own saved choice (Options -> Language) wins, then the Windows UI language.
    $saved = Join-Path $env:APPDATA 'gtetrinet-ng\gtetrinet-ng\settings.ini'
    $choice = Get-Content $saved -ErrorAction SilentlyContinue | Where-Object { $_ -like 'lang=*' } | Select-Object -Last 1
    if ($choice -eq 'lang=de') { return $true }
    if ($choice -eq 'lang=en') { return $false }
    return (Get-UICulture).TwoLetterISOLanguageName -eq 'de'
}

function Show-Fault([string]$text) {
    if ($env:GTN_NO_DIALOG -eq '1') { Write-Output $text; return }
    try {
        Add-Type -AssemblyName System.Windows.Forms
        [void][System.Windows.Forms.MessageBox]::Show($text, 'GTetrinet NG', 'OK', 'Warning')
    } catch { }
}

$code = 1
$started = $true
try {
    $process = Start-Process -FilePath (Join-Path $PSScriptRoot 'gtetrinet-ng.exe') -WorkingDirectory $PSScriptRoot -PassThru -Wait
    $code = $process.ExitCode
} catch { $code = 1; $started = $false }
if ($code -ne 0 -and $code -ne 2) {
    $reported = $env:GTN_NO_REPORT -ne '1'
    if ($reported) {
        $version = (Get-Content (Join-Path $PSScriptRoot 'VERSION') -ErrorAction SilentlyContinue)
        # Fire and forget; no application logs or personal identifiers are uploaded.
        Start-Process curl.exe -WindowStyle Hidden -ArgumentList @('-fsS', '--max-time', '8', '-d', "`"gtetrinet-ng $version Windows exit code $code`"", 'https://ntfy.sh/gtetrinet-ng-faults-5e8b41e03cf04b57') -ErrorAction SilentlyContinue
    }
    if (Test-German) {
        $ae = [char]0xE4; $oe = [char]0xF6; $ue = [char]0xFC
        if ($started) { $text = "GTetrinet NG wurde unerwartet beendet (Code $code)." }
        else { $text = "GTetrinet NG konnte nicht gestartet werden. Bitte entpacke das ZIP vollst${ae}ndig und starte Play.cmd erneut." }
        if ($reported) { $text += " Ein anonymer Fehlerbericht ohne pers${oe}nliche Daten wurde gesendet; Updates beheben solche Fehler meist." }
        else { $text += " Bitte versuche es noch einmal oder pr${ue}fe, ob es eine neue Version gibt." }
    } else {
        if ($started) { $text = "GTetrinet NG closed unexpectedly (code $code)." }
        else { $text = 'GTetrinet NG could not be started. Please extract the whole ZIP and run Play.cmd again.' }
        if ($reported) { $text += ' An anonymous fault report without personal data was sent; updates usually fix such faults.' }
        else { $text += ' Please try again or check for a newer version.' }
    }
    Show-Fault $text
}
exit $code
