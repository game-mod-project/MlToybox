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

function New-FakeGame {
    $g = Join-Path $env:TEMP "mltb-game-$(Get-Random)"
    $mods = Join-Path $g 'ManorLords\Binaries\Win64\ue4ss\Mods'
    New-Item -ItemType Directory -Force $mods | Out-Null
    Copy-Item "$PSScriptRoot\fixtures\mods.txt", "$PSScriptRoot\fixtures\mods.json" $mods
    return $g
}
function Set-FakeModSource([string]$name) {
    # deploy.ps1 은 레포의 mod\<name>\Scripts 를 원본으로 쓴다. 테스트용 원본이 없으면 만든다.
    $src = Join-Path (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent) "mod\$name\Scripts"
    if (-not (Test-Path (Join-Path $src 'main.lua'))) {
        New-Item -ItemType Directory -Force $src | Out-Null
        Set-Content (Join-Path $src 'main.lua') '-- placeholder created by test'
    }
}

Test-Case 'deploy installs, registers once, preserves bridge' {
    Set-FakeModSource 'MLToybox'
    $g = New-FakeGame
    $mods = Join-Path $g 'ManorLords\Binaries\Win64\ue4ss\Mods'
    & "$PSScriptRoot\..\deploy.ps1" -Mod MLToybox -GameDir $g | Out-Null
    Assert-True (Test-Path "$mods\MLToybox\Scripts\main.lua") 'scripts copied'
    Assert-True (Test-Path "$mods\MLToybox\bridge") 'bridge dir'
    Set-Content "$mods\MLToybox\bridge\control.json" '{"keep":1}'
    & "$PSScriptRoot\..\deploy.ps1" -Mod MLToybox -GameDir $g | Out-Null
    Assert-Equal (Get-Content -Raw "$mods\MLToybox\bridge\control.json").Trim() '{"keep":1}' 'bridge preserved'
    $txt = Get-Content "$mods\mods.txt"
    Assert-Equal (@($txt | Where-Object { $_ -match '^\s*MLToybox\s*:\s*1\s*$' }).Count) 1 'mods.txt once'
    $kb = [array]::IndexOf($txt, ($txt | Where-Object { $_ -match '^; Built-in keybinds' } | Select-Object -First 1))
    $ml = [array]::IndexOf($txt, ($txt | Where-Object { $_ -match '^\s*MLToybox\s*:' } | Select-Object -First 1))
    Assert-True ($ml -lt $kb) 'registered before keybinds comment'
    $json = Get-Content -Raw "$mods\mods.json" | ConvertFrom-Json
    Assert-Equal (@($json | Where-Object mod_name -eq 'MLToybox').Count) 1 'mods.json once'
}

Test-Case 'deploy -Remove unregisters and deletes' {
    Set-FakeModSource 'MLToybox'
    $g = New-FakeGame
    $mods = Join-Path $g 'ManorLords\Binaries\Win64\ue4ss\Mods'
    & "$PSScriptRoot\..\deploy.ps1" -Mod MLToybox -GameDir $g | Out-Null
    & "$PSScriptRoot\..\deploy.ps1" -Mod MLToybox -GameDir $g -Remove | Out-Null
    Assert-True (-not (Test-Path "$mods\MLToybox")) 'folder removed'
    Assert-Equal (@(Get-Content "$mods\mods.txt" | Where-Object { $_ -match '^\s*MLToybox\s*:' }).Count) 0 'mods.txt clean'
    $json = Get-Content -Raw "$mods\mods.json" | ConvertFrom-Json
    Assert-Equal (@($json | Where-Object mod_name -eq 'MLToybox').Count) 0 'mods.json clean'
    Assert-Equal (@($json).Count) 8 'other entries intact'
}

Test-Case 'deploy MLToyboxLab creates lab folder and registers' {
    $g = New-FakeGame
    $mods = Join-Path $g 'ManorLords\Binaries\Win64\ue4ss\Mods'
    & "$PSScriptRoot\..\deploy.ps1" -Mod MLToyboxLab -GameDir $g | Out-Null
    Assert-True (Test-Path "$mods\MLToyboxLab\Scripts\main.lua") 'lab scripts'
    Assert-True (Test-Path "$mods\MLToyboxLab\lab") 'lab dir'
    Assert-Equal (@(Get-Content "$mods\mods.txt" | Where-Object { $_ -match '^\s*MLToyboxLab\s*:\s*1' }).Count) 1 'registered'
}

Test-Case 'deploy copies native dll when built and tolerates locked target' {
    $repo = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
    $built = Join-Path $repo 'native\build\mltoybox_native.dll'
    $created = $false
    if (-not (Test-Path $built)) { New-Item -ItemType Directory -Force (Split-Path $built) | Out-Null; Set-Content $built 'fake'; $created = $true }
    try {
        $g = New-FakeGame
        $mods = Join-Path $g 'ManorLords\Binaries\Win64\ue4ss\Mods'
        & "$PSScriptRoot\..\deploy.ps1" -Mod MLToybox -GameDir $g | Out-Null
        $target = "$mods\MLToybox\native\mltoybox_native.dll"
        Assert-True (Test-Path $target) 'dll copied'
        $lock = [System.IO.File]::Open($target, 'Open', 'Read', 'None')
        try { & "$PSScriptRoot\..\deploy.ps1" -Mod MLToybox -GameDir $g 3>$null | Out-Null } finally { $lock.Dispose() }
    } finally { if ($created) { Remove-Item $built } }
}

if ($script:failed -gt 0) { throw "$script:failed test(s) failed" }
Write-Host 'ALL PASS'
