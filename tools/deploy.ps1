param(
    [Parameter(Mandatory)][ValidateSet('MLToybox', 'MLToyboxDump')][string]$Mod,
    [string]$GameDir,
    [switch]$Remove,
    [switch]$Panel
)
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\common.ps1"
$repo = Split-Path $PSScriptRoot -Parent
$modsDir = Get-MLModsDir $GameDir
$target = Join-Path $modsDir $Mod
$modsTxt = Join-Path $modsDir 'mods.txt'
$modsJson = Join-Path $modsDir 'mods.json'

function Update-ModsTxt([bool]$register) {
    $lines = [System.Collections.Generic.List[string]](Get-Content $modsTxt)
    $lines.RemoveAll([Predicate[string]] { param($l) $l -match "^\s*$Mod\s*:" }) | Out-Null
    if ($register) {
        $kb = $lines.FindIndex([Predicate[string]] { param($l) $l -match '^; Built-in keybinds' })
        if ($kb -lt 0) { $lines.Add("$Mod : 1") } else { $lines.Insert($kb, "$Mod : 1") }
    }
    Set-Content -Path $modsTxt -Value $lines -Encoding utf8NoBOM
}

function Update-ModsJson([bool]$register) {
    if (-not (Test-Path $modsJson)) { return }
    $list = [System.Collections.Generic.List[object]]@(Get-Content -Raw $modsJson | ConvertFrom-Json)
    $list.RemoveAll([Predicate[object]] { param($e) $e.mod_name -eq $Mod }) | Out-Null
    if ($register) {
        $entry = [pscustomobject]@{ mod_name = $Mod; mod_enabled = $true }
        $kb = $list.FindIndex([Predicate[object]] { param($e) $e.mod_name -eq 'Keybinds' })
        if ($kb -lt 0) { $list.Add($entry) } else { $list.Insert($kb, $entry) }
    }
    ConvertTo-Json -InputObject @($list) -Depth 5 | Set-Content -Path $modsJson -Encoding utf8NoBOM
}

if ($Remove) {
    Update-ModsTxt $false
    Update-ModsJson $false
    if (Test-Path $target) { Remove-Item -Recurse -Force $target }
    Write-Host "Removed $Mod"
    return
}

$src = Join-Path $repo "mod\$Mod\Scripts"
if (-not (Test-Path (Join-Path $src 'main.lua'))) { throw "Missing $src\main.lua" }
$dstScripts = Join-Path $target 'Scripts'
New-Item -ItemType Directory -Force $dstScripts | Out-Null
robocopy $src $dstScripts /MIR /NJH /NJS /NP /NFL /NDL | Out-Null
if ($LASTEXITCODE -ge 8) { throw "robocopy failed ($LASTEXITCODE)" }
$global:LASTEXITCODE = 0
if ($Mod -eq 'MLToybox') { New-Item -ItemType Directory -Force (Join-Path $target 'bridge') | Out-Null }
Update-ModsTxt $true
Update-ModsJson $true
Write-Host "Deployed $Mod -> $target"
