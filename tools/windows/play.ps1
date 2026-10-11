# SPDX-License-Identifier: GPL-2.0-or-later
$ErrorActionPreference = 'Stop'
$code = 1
try {
    $process = Start-Process -FilePath (Join-Path $PSScriptRoot 'gtetrinet-ng.exe') -WorkingDirectory $PSScriptRoot -PassThru -Wait
    $code = $process.ExitCode
} catch { $code = 1 }
if ($code -ne 0 -and $code -ne 2 -and $env:GTN_NO_REPORT -ne '1') {
    $version = (Get-Content (Join-Path $PSScriptRoot 'VERSION') -ErrorAction SilentlyContinue)
    # Fire and forget; no application logs or personal identifiers are uploaded.
    Start-Process curl.exe -WindowStyle Hidden -ArgumentList @('-fsS', '--max-time', '8', '-d', "`"gtetrinet-ng $version Windows exit code $code`"", 'https://ntfy.sh/gtetrinet-ng-faults-5e8b41e03cf04b57') -ErrorAction SilentlyContinue
}
exit $code
