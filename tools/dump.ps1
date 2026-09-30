param([string]$GameDir, [int]$TimeoutSec = 600)
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\common.ps1"
$repo = Split-Path $PSScriptRoot -Parent
$win64 = Get-MLWin64Dir $GameDir
$ue4ss = Join-Path $win64 'ue4ss'
$modDir = Join-Path $ue4ss 'Mods\MLToyboxDump'
if (-not (Test-Path $modDir)) { throw 'MLToyboxDump not deployed. Run: deploy.ps1 -Mod MLToyboxDump' }

$done = Join-Path $modDir 'dump_done.txt'
Remove-Item $done -ErrorAction SilentlyContinue
Set-Content (Join-Path $modDir 'dump_request.txt') (Get-Date -Format o)
Write-Host 'Dump requested; waiting (game must be running with a save loaded)...'
$deadline = (Get-Date).AddSeconds($TimeoutSec)
while (-not (Test-Path $done)) {
    if ((Get-Date) -gt $deadline) { throw "Timed out after $TimeoutSec s waiting for $done" }
    Start-Sleep -Seconds 3
}

$dest = Join-Path $repo "analysis\dumps\$(Get-Date -Format 'yyyyMMdd-HHmmss')"
New-Item -ItemType Directory -Force $dest | Out-Null
$candidates = @(
    'UE4SS_ObjectDump.txt', 'CXXHeaderDump', 'UE4SS.log',
    'Mods\shared\types', 'Mods\MLToyboxDump\events.txt', 'Mods\MLToyboxDump\dump_done.txt'
)
foreach ($rel in $candidates) {
    $found = @((Join-Path $ue4ss $rel), (Join-Path $win64 $rel)) | Where-Object { Test-Path $_ } | Select-Object -First 1
    if ($found) {
        Copy-Item $found -Destination $dest -Recurse -Force
        Write-Host "collected $rel"
    } else {
        Write-Warning "missing $rel"
    }
}
Get-Content $done
return $dest
