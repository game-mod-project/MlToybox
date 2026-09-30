param([Parameter(Mandatory)][string]$File, [string]$GameDir, [int]$TimeoutSec = 60, [hashtable]$Vars)
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\common.ps1"
$lab = Join-Path (Get-MLModsDir $GameDir) 'MLToyboxLab\lab'
if (-not (Test-Path $lab)) { throw 'MLToyboxLab not deployed. Run: deploy.ps1 -Mod MLToyboxLab (then restart the game)' }
$out = Join-Path $lab 'out.txt'
Remove-Item $out -ErrorAction SilentlyContinue
# -Vars @{ NAME = 'x' } 는 스크립트 안의 __NAME__ 을 x 로 바꿔 넣는다
$code = Get-Content -Raw -Encoding utf8 $File
if ($Vars) { foreach ($k in $Vars.Keys) { $code = $code.Replace("__${k}__", [string]$Vars[$k]) } }
# Lab 모드가 쓰다 만 파일을 읽지 않도록 임시 파일에 쓴 뒤 이름을 바꾼다
$tmp = Join-Path $lab 'run.lua.tmp'
Set-Content -Path $tmp -Value $code -Encoding utf8NoBOM -NoNewline
Move-Item -Force $tmp (Join-Path $lab 'run.lua')
$deadline = (Get-Date).AddSeconds($TimeoutSec)
while (-not (Test-Path $out)) {
    if ((Get-Date) -gt $deadline) { throw "Timed out; is the game running with MLToyboxLab loaded?" }
    Start-Sleep -Milliseconds 300
}
Get-Content $out
