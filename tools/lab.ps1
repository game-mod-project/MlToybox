param([Parameter(Mandatory)][string]$File, [string]$GameDir, [int]$TimeoutSec = 60)
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\common.ps1"
$lab = Join-Path (Get-MLModsDir $GameDir) 'MLToyboxLab\lab'
if (-not (Test-Path $lab)) { throw 'MLToyboxLab not deployed. Run: deploy.ps1 -Mod MLToyboxLab (then restart the game)' }
$out = Join-Path $lab 'out.txt'
Remove-Item $out -ErrorAction SilentlyContinue
Copy-Item $File (Join-Path $lab 'run.lua')
$deadline = (Get-Date).AddSeconds($TimeoutSec)
while (-not (Test-Path $out)) {
    if ((Get-Date) -gt $deadline) { throw "Timed out; is the game running with MLToyboxLab loaded?" }
    Start-Sleep -Milliseconds 300
}
Get-Content $out
