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
