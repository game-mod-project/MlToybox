# 릴리스용 압축 파일을 만든다: <OutDir>\MLToybox-<Version>.zip (기본 OutDir 은 dist).
# 빌드 도구 없이 설치할 수 있는 묶음이다: 모드 폴더(Lua, 네이티브 DLL, 오버레이 DLL), 비상용 패널, 서드파티 라이선스, 설치 안내.
# 설정·상태 파일(bridge 의 control.json 등), 테스트, 개발용 모드는 넣지 않는다.
#   -Version <글>   파일 이름과 설치 안내에 적을 판 이름(예: v1.0.0)
#   -OutDir <폴더>  만들 곳
#   -NoPanel        패널을 빼고 만든다(테스트용)
# 네이티브 DLL 은 먼저 빌드돼 있어야 한다: pwsh tools/build-native.ps1 -Test
param([Parameter(Mandatory)][string]$Version, [string]$OutDir, [switch]$NoPanel)
$ErrorActionPreference = 'Stop'
if ($Version -notmatch '^[0-9A-Za-z][0-9A-Za-z.\-]*$') { throw "bad version: $Version" }
$repo = Split-Path $PSScriptRoot -Parent
if (-not $OutDir) { $OutDir = Join-Path $repo 'dist' }
New-Item -ItemType Directory -Force $OutDir | Out-Null
$stage = Join-Path $env:TEMP "mltb-package-$(Get-Random)"
$mod = (New-Item -ItemType Directory -Force (Join-Path $stage 'MLToybox')).FullName

# 1. Lua 모드
$scripts = Join-Path $repo 'mod\MLToybox\Scripts'
if (-not (Test-Path (Join-Path $scripts 'main.lua'))) { throw "Missing $scripts\main.lua" }
Copy-Item $scripts (Join-Path $mod 'Scripts') -Recurse

# 2. 네이티브 DLL 과 오버레이 DLL
$native = (New-Item -ItemType Directory -Force (Join-Path $mod 'native')).FullName
foreach ($name in 'mltoybox_native.dll', 'mltoybox_overlay.dll') {
    $dll = Join-Path $repo "native\build\$name"
    if (-not (Test-Path $dll)) { throw "Missing $dll. Run: pwsh tools/build-native.ps1 -Test" }
    Copy-Item $dll $native
}

# 3. 모드와 오버레이가 설정·상태 파일을 두는 폴더(비워 둔다)
$bridge = (New-Item -ItemType Directory -Force (Join-Path $mod 'bridge')).FullName
Set-Content (Join-Path $bridge 'README.txt') -Encoding utf8NoBOM -Value @'
MLToybox 가 설정과 상태 파일을 두는 폴더입니다. 지우지 마세요.
  control.json         설정(오버레이나 패널이 씁니다)
  status.json          모드가 알리는 상태
  overlay.json         게임 안 창의 여닫는 키, 글자 배율, 창 위치
  overlay_status.json  게임 안 창의 상태
'@

# 4. 비상용 패널(.NET 8 Desktop Runtime 이 있어야 실행된다)
if (-not $NoPanel) {
    $panel = Join-Path $stage 'panel'
    dotnet publish (Join-Path $repo 'panel\MLToybox.Panel\MLToybox.Panel.csproj') -c Release -o $panel --nologo -v q | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'panel publish failed' }
    Get-ChildItem $panel -Filter '*.pdb' | Remove-Item
}

# 5. 함께 들어가는 라이브러리의 라이선스
$licenses = (New-Item -ItemType Directory -Force (Join-Path $stage 'licenses')).FullName
Copy-Item (Join-Path $repo 'native\third_party\imgui\LICENSE.txt') (Join-Path $licenses 'imgui-LICENSE.txt')
Copy-Item (Join-Path $repo 'native\third_party\minhook\LICENSE.txt') (Join-Path $licenses 'minhook-LICENSE.txt')
Copy-Item (Join-Path $repo 'native\third_party\nlohmann\LICENSE.MIT') (Join-Path $licenses 'nlohmann-json-LICENSE.MIT')

# 6. 설치 안내
$install = @'
MLToybox __VERSION__ - Manor Lords 치트 모드

필요한 것
  - Manor Lords (Steam). 이 판은 Steam buildid 24905706 에서 확인했습니다. 게임이 업데이트되면 일부 기능이 꺼질 수 있습니다.
  - UE4SS v3.0.1 이 게임 폴더(ManorLords\Binaries\Win64)에 설치돼 있어야 합니다(dwmapi.dll 프록시).

설치 (게임을 끈 상태에서)
  1. 이 압축 파일의 MLToybox 폴더를 아래 폴더에 통째로 복사합니다.
       <게임 폴더>\ManorLords\Binaries\Win64\ue4ss\Mods\
  2. 같은 Mods 폴더의 mods.txt 를 열어, "; Built-in keybinds" 줄보다 위에 다음 한 줄을 넣습니다.
       MLToybox : 1
     Mods 폴더에 mods.json 이 있으면 Keybinds 항목 앞에 다음 항목도 넣습니다.
       { "mod_name": "MLToybox", "mod_enabled": true },
  3. 게임을 켜고, 게임 안에서 Insert 를 누르면 MLToybox 창이 열립니다. 값을 바꾸면 바로 반영됩니다.

판올림 (이미 설치돼 있을 때)
  게임을 끄고 MLToybox 폴더의 Scripts 와 native 를 새것으로 덮어씁니다. bridge 폴더는 그대로 두면 설정이 남습니다.

창이 뜨지 않을 때
  - MLToybox\bridge\overlay_status.json 의 state 와 reason 을 봅니다.
  - 창 없이 설정하려면 panel\MLToybox.Panel.exe 를 실행합니다(.NET 8 Desktop Runtime 필요). 패널은 비상용입니다.
  - 게임 안 창만 끄려면 MLToybox\Scripts\config.lua 의 overlay = true 를 false 로 바꿉니다.

제거
  게임을 끄고 MLToybox 폴더를 지운 뒤 mods.txt(와 mods.json)에서 MLToybox 줄을 지웁니다.

주의
  - 세이브를 먼저 백업하세요. 치트로 바꾼 값은 세이브에 남습니다.
  - 기능 설명과 변경 이력: https://github.com/game-mod-project/MlToybox

함께 들어 있는 라이브러리(Dear ImGui, MinHook, nlohmann/json)의 라이선스는 licenses 폴더에 있습니다.
'@
Set-Content (Join-Path $stage 'INSTALL.txt') -Encoding utf8BOM -Value $install.Replace('__VERSION__', $Version)

$zip = Join-Path $OutDir "MLToybox-$Version.zip"
if (Test-Path $zip) { Remove-Item $zip }
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip
Remove-Item $stage -Recurse -Force
Write-Host "Packaged $zip"
$zip
