param(
    [string]$SaveDir = (Join-Path $env:LOCALAPPDATA 'ManorLords\Saved\SaveGames'),
    [string]$BackupRoot = (Join-Path (Split-Path $PSScriptRoot -Parent) 'backups')
)
$ErrorActionPreference = 'Stop'
if (-not (Test-Path $SaveDir)) { throw "SaveDir not found: $SaveDir" }
$dest = Join-Path $BackupRoot (Get-Date -Format 'yyyyMMdd-HHmmss')
New-Item -ItemType Directory -Force $dest | Out-Null
Copy-Item -Path (Join-Path $SaveDir '*') -Destination $dest -Recurse -Force
Write-Host "Backed up $SaveDir -> $dest"
return $dest
