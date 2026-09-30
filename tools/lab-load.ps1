# 개발용: 메인 메뉴에서 세이브 슬롯을 불러온다(화면 조작 불필요). MLToyboxLab 이 배포돼 있어야 한다.
param([Parameter(Mandatory)][string]$Slot, [string]$GameDir, [int]$TimeoutSec = 180, [switch]$Start)
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\common.ps1"
if ($Slot -notmatch '^(autosave|quicksave|saveGame_[0-9]+)$') { throw "bad slot name: $Slot" }
$lab = Join-Path $PSScriptRoot 'lab.ps1'
$status = Join-Path (Get-MLModsDir $GameDir) 'MLToybox\bridge\status.json'

function Read-Status {
    try { return Get-Content -Raw $status | ConvertFrom-Json } catch { return $null }
}
function Test-Alive($s) {
    return $s -and (([DateTimeOffset]::UtcNow.ToUnixTimeSeconds() - $s.heartbeat) -le 3)
}

if ($Start -and -not (Get-Process -Name 'ManorLords-Win64-Shipping' -ErrorAction SilentlyContinue)) {
    & (Get-ItemProperty 'HKCU:\Software\Valve\Steam').SteamExe -applaunch $script:MLAppId
    $deadline = (Get-Date).AddSeconds(240)
    while (-not (Test-Alive (Read-Status))) {
        if ((Get-Date) -gt $deadline) { throw 'game did not start (no mod heartbeat)' }
        Start-Sleep -Seconds 3
    }
    Start-Sleep -Seconds 15   # 스플래시가 끝나고 메인 메뉴가 뜰 때까지
}
if (-not (Test-Alive (Read-Status))) { throw 'game is not running with MLToybox loaded (use -Start)' }

$tmp = Join-Path ([IO.Path]::GetTempPath()) "mltb-lab-load-$PID"
New-Item -ItemType Directory -Force $tmp | Out-Null
$loadLua = Join-Path $tmp 'load.lua'
Set-Content $loadLua -Encoding utf8NoBOM -Value @'
local SLOT = "__SLOT__"
local w
for _, x in ipairs(FindAllOf("mainMenu_widget_C") or {}) do
  if x:IsValid() and x:IsInViewport() then w = x end
end
if not w then print("NO_MENU") return end
-- 슬롯 번호: autosave 0, quicksave 1, saveGame_N 은 N + 2 (게임의 SlotNameFromIndex 와 같다)
local index = ({ autosave = 0, quicksave = 1 })[SLOT]
if not index then index = tonumber(SLOT:match("^saveGame_(%d+)$")) + 2 end
local gs = StaticFindObject("/Script/Engine.Default__GameplayStatics")
local gi = FindFirstOf("BP_MLGameInstance_C")
if not gs:IsValid() or not gi:IsValid() or not gs:DoesSaveGameExist(SLOT .. "_descr", 0) then print("NO_SLOT") return end
local descr = gs:LoadGameFromSlot(SLOT .. "_descr", 0)
if not descr:IsValid() then print("NO_SLOT") return end
-- LoadGame 은 화면 전환만 한다. 읽을 세이브는 게임 인스턴스의 savefileToLoad 가 정하고, 비어 있으면 새 게임이 시작된다.
gi.savefileToLoad = SLOT
w:LoadGame({ SlotIndex = index, Descriptor = descr })
print("LOADING")
'@

$loaded = & $lab -File $loadLua -GameDir $GameDir -Vars @{ SLOT = $Slot }
if ($loaded -contains 'NO_MENU') { throw 'main menu is not showing (already in a game?)' }
if ($loaded -notcontains 'LOADING') { throw "slot not loaded: $($loaded -join ' ')" }
$deadline = (Get-Date).AddSeconds($TimeoutSec)
while (-not (Read-Status).inGame) {
    if ((Get-Date) -gt $deadline) { throw 'timed out waiting for the save to load' }
    Start-Sleep -Seconds 3
}
Write-Host "Loaded $Slot"
