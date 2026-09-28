$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\..\common.ps1"
$script:failed = 0
function Test-Case([string]$name, [scriptblock]$body) {
    try { & $body; Write-Host "PASS $name" }
    catch { $script:failed++; Write-Host "FAIL $name : $_" -ForegroundColor Red }
}

Test-Case 'GameDir: explicit param wins' {
    $d = New-Item -ItemType Directory -Force (Join-Path $env:TEMP "mltb-gd-$(Get-Random)")
    Assert-Equal (Get-MLGameDir -GameDir $d.FullName) $d.FullName 'explicit'
}

Test-Case 'GameDir: detects real install from libraryfolders.vdf' {
    $env:MLTOYBOX_GAMEDIR = $null
    $g = Get-MLGameDir
    Assert-True (Test-Path (Join-Path $g 'ManorLords\Binaries\Win64\ManorLords-Win64-Shipping.exe')) "detected $g"
}

Test-Case 'backup-saves copies SaveGames into timestamped folder' {
    $src = New-Item -ItemType Directory -Force (Join-Path $env:TEMP "mltb-saves-$(Get-Random)")
    Set-Content (Join-Path $src 'a.sav') 'x'
    $dst = Join-Path $env:TEMP "mltb-bk-$(Get-Random)"
    $out = & "$PSScriptRoot\..\backup-saves.ps1" -SaveDir $src.FullName -BackupRoot $dst
    Assert-True (Test-Path (Join-Path $out 'a.sav')) "backup at $out"
    Assert-True ((Split-Path $out -Leaf) -match '^\d{8}-\d{6}$') 'timestamp name'
}

if ($script:failed -gt 0) { throw "$script:failed test(s) failed" }
Write-Host 'ALL PASS'
