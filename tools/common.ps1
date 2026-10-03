$ErrorActionPreference = 'Stop'
$script:MLAppId = '1363080'

function Assert-Equal($actual, $expected, [string]$msg) {
    if ($actual -ne $expected) { throw "$msg : expected <$expected> got <$actual>" }
}
function Assert-True($cond, [string]$msg) {
    if (-not $cond) { throw "$msg : condition false" }
}

function Get-MLGameDir([string]$GameDir) {
    if ($GameDir) { return (Resolve-Path $GameDir).Path }
    if ($env:MLTOYBOX_GAMEDIR) { return (Resolve-Path $env:MLTOYBOX_GAMEDIR).Path }
    $steam = (Get-ItemProperty 'HKCU:\Software\Valve\Steam' -ErrorAction Stop).SteamPath
    $vdf = Get-Content -Raw (Join-Path $steam 'steamapps\libraryfolders.vdf')
    $pattern = '"path"\s*"(?<p>[^"]+)"(?<body>.*?)(?="path"|\z)'
    foreach ($m in [regex]::Matches($vdf, $pattern, 'Singleline')) {
        if ($m.Groups['body'].Value -match "`"$script:MLAppId`"\s*`"") {
            $lib = $m.Groups['p'].Value -replace '\\\\', '\'
            $dir = Join-Path $lib 'steamapps\common\Manor Lords'
            if (Test-Path $dir) { return (Resolve-Path $dir).Path }
        }
    }
    throw 'Manor Lords install not found. Pass -GameDir or set MLTOYBOX_GAMEDIR.'
}

function Get-MLWin64Dir([string]$GameDir) { Join-Path (Get-MLGameDir $GameDir) 'ManorLords\Binaries\Win64' }
function Get-MLModsDir([string]$GameDir) { Join-Path (Get-MLWin64Dir $GameDir) 'ue4ss\Mods' }

# 폴더 안에서 이름이 점으로 시작하는 폴더와 파일을 지운다. 도구가 모드 소스 폴더에 남긴 것(.omc 등)이
# 모드와 함께 게임 폴더나 릴리스 묶음으로 나가지 않게 한다. 깊은 것부터 지운다
function Remove-MLDotEntries([string]$Dir) {
    Get-ChildItem -LiteralPath $Dir -Recurse -Force | Where-Object { $_.Name -like '.*' } |
        Sort-Object { $_.FullName.Length } -Descending |
        ForEach-Object { Remove-Item -LiteralPath $_.FullName -Recurse -Force }
}
