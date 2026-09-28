# MLToybox Plan 1 — 기반(도구·Lua 코어·브리지·패널·분석) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 기능 모듈을 꽂기만 하면 되는 UE4SS Lua 모드 골격, 게임 밖 제어 패널, 배포·백업·덤프 도구, 그리고 기능 구현 대상을 확정한 `analysis/findings.md`를 만든다.

**Architecture:** UE4SS Lua 모드(`mod/MLToybox`)가 1초 루프로 `bridge/control.json`을 읽어 기능 레지스트리에 적용하고 `status.json`을 기록한다. .NET 8 WinForms 패널이 이 두 파일로 모드를 제어한다. 게임 밖에서 테스트할 수 있는 Lua 코드는 xUnit + NLua(Lua 5.4)로 검증한다.

**Tech Stack:** UE4SS Lua (Lua 5.4 내장), rxi/json.lua v0.1.2, .NET 8 (WinForms, xUnit, NLua), PowerShell 7

**Spec:** `docs/superpowers/specs/2026-09-28-mltoybox-mod-design.md`

**범위:** 이 계획은 spec의 §4(아키텍처), §5(패널·브리지), §7(분석), §8·§9 중 기반 부분을 구현한다. §6의 기능 4개(`features/*.lua`)는 `findings.md` 완성 후 **Plan 2**에서 구현한다.

## Global Constraints

- 게임 경로: `E:\SteamLibrary\steamapps\common\Manor Lords` (Steam appid `1363080`). 스크립트는 `-GameDir` 인자, 환경변수 `MLTOYBOX_GAMEDIR`, Steam `libraryfolders.vdf` 순서로 탐지한다.
- UE4SS 모드 폴더: `<GameDir>\ManorLords\Binaries\Win64\ue4ss\Mods\`
- 브리지 폴더: `<GameDir>\ManorLords\Binaries\Win64\ue4ss\Mods\MLToybox\bridge\`
- 브리지 프로토콜 `version = 1`. heartbeat 타임아웃 5초. 폴링 1초.
- 연속 실패 임계치 기본 5 (`config.lua`의 `failureThreshold`).
- UE4SS Lua 로그 접두어 `[MLToybox] `, `print`에는 끝에 `\n`을 붙인다.
- UObject 조작과 레지스트리 호출은 반드시 게임 스레드(`ExecuteInGameThread` 또는 훅 콜백 안)에서 한다. `LoopAsync` 콜백은 비동기 스레드다.
- .NET: `net8.0-windows`, SDK 8.0.425. 외부 의존성은 NuGet `NLua`, `xunit`, `xunit.runner.visualstudio`, `Microsoft.NET.Test.Sdk`와 GitHub `rxi/json.lua` 태그 `v0.1.2`뿐이다. **이 계획을 승인하면 이 다운로드들을 승인한 것으로 본다.** 그 밖의 다운로드(FModel 등)는 별도로 승인받는다.
- git: 작업 브랜치 `feat/plan1-foundation`(`develop`에서 분기). 태스크마다 커밋하고 마지막에 `develop`으로 `--no-ff` 머지한다. `main`은 건드리지 않는다. 커밋 메시지 끝에 `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`을 붙인다.
- 인게임 테스트 전에는 반드시 `tools\backup-saves.ps1`을 실행한다.
- spec과 다른 점(의도된 결정):
  - 핫키가 없으므로 `ticker.lua`는 `registry:tick` + `main.lua` 루프로 대체한다.
  - `finder.lua`는 계약이 분석 결과에 달려 있으므로 Plan 2로 미룬다.
  - 기능 모듈 계약에서 `init`은 `enable`에 합친다(맵마다 enable/disable). 설정 변경 반영용 `configure`를 추가한다.
  - `status.json`에 선택 필드 `bridgeError`를 추가한다.
  - Lua에는 파일 mtime API가 없으므로, `control.json` 변경은 mtime 대신 `seq` 비교로 감지한다.

## Review Focus

1. **패널을 먼저 켜고 게임은 나중에 실행** → 패널은 "연결 안 됨"을 표시하고 크래시하지 않아야 한다. 게임이 켜지면 5초 안에 연결로 바뀌어야 한다. → Task 8 `Evaluate_NullStatus_IsDisconnected`, `Evaluate_StaleHeartbeat_IsDisconnected`
2. **`control.json`이 깨졌거나(수동 편집, 반쯤 쓰임) 버전이 다름** → 모드는 직전 상태를 유지하고 `bridgeError`를 보고해야 한다. 패널은 기본값으로 시작해야 한다. → Task 5 `poll_bad_json_keeps_last_seq`, `poll_version_mismatch`; Task 8 `LoadControl_CorruptFile_ReturnsDefault`
3. **기능이 계속 예외를 던짐**(게임 업데이트로 대상이 사라짐) → 임계치에서 해당 기능만 꺼지고 다른 기능은 계속 동작해야 한다. 사용자가 다시 적용하면 재시도해야 한다. → Task 6 `trip_disables_only_that_feature`, `apply_clears_trip`
4. **맵 전환(게임 → 메인 메뉴 → 다른 세이브)** → 전환 시 모든 기능이 disable되고, 새 게임에서 다시 enable되어야 한다. 죽은 오브젝트 참조가 남으면 안 된다. → Task 6 `leaving_game_disables_all`, `reentering_game_reenables`
5. **`deploy.ps1`을 여러 번 실행** → `mods.txt`/`mods.json` 항목이 중복되지 않아야 하고, 기존 `bridge/control.json`(사용자 설정)이 지워지지 않아야 한다. → Task 2 테스트의 "2회 배포" 및 "bridge 보존" 단언

---

## 파일 구조

```
E:\MLToybox\
├─ tools\
│   ├─ common.ps1                 Get-MLGameDir, Get-MLWin64Dir, Assert-* 헬퍼
│   ├─ backup-saves.ps1           SaveGames → backups\<yyyyMMdd-HHmmss>\
│   ├─ deploy.ps1                 모드 설치/제거 + mods.txt/json 등록 + (선택) 패널 publish
│   ├─ dump.ps1                   덤프 요청 플래그 생성 → 완료 대기 → analysis\dumps 수집
│   └─ tests\
│       ├─ fixtures\mods.txt, mods.json
│       └─ Tools.Tests.ps1        PowerShell 단언 기반 테스트
├─ mod\
│   ├─ MLToybox\
│   │   ├─ Scripts\
│   │   │   ├─ main.lua
│   │   │   ├─ config.lua
│   │   │   ├─ lib\json.lua       (rxi v0.1.2 원본)
│   │   │   └─ core\log.lua, safe.lua, paths.lua, fileio.lua, bridge.lua, registry.lua
│   │   └─ tests\t.lua, *_spec.lua   (배포하지 않음)
│   └─ MLToyboxDump\Scripts\main.lua
├─ panel\
│   ├─ MLToybox.sln
│   ├─ MLToybox.Panel.Core\       모델·경로 탐지·브리지 클라이언트·설정 (UI 없음)
│   ├─ MLToybox.Panel\            WinForms 앱
│   └─ MLToybox.Tests\            xUnit: Core 테스트 + Lua spec 러너(NLua)
├─ analysis\findings.md
└─ README.md
```

---

### Task 1: PowerShell 공통 모듈 + 세이브 백업

**Files:**
- Create: `tools/common.ps1`
- Create: `tools/backup-saves.ps1`
- Create: `tools/tests/Tools.Tests.ps1`

**Interfaces:**
- Produces: `Get-MLGameDir([string]$GameDir) -> string`, `Get-MLWin64Dir([string]$GameDir) -> string`, `Get-MLModsDir([string]$GameDir) -> string`, `Assert-Equal($actual,$expected,$msg)`, `Assert-True($cond,$msg)`. 모든 스크립트는 `. "$PSScriptRoot\common.ps1"`로 불러온다.

- [ ] **Step 1: 작업 브랜치 생성**

```powershell
git -C E:\MLToybox switch -c feat/plan1-foundation develop
```

- [ ] **Step 2: 실패하는 테스트 작성** — `tools/tests/Tools.Tests.ps1`

```powershell
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
```

- [ ] **Step 3: 실행해서 실패 확인**

Run: `pwsh -NoProfile -File E:\MLToybox\tools\tests\Tools.Tests.ps1`
Expected: `common.ps1` 파일이 없어 오류로 FAIL

- [ ] **Step 4: 구현** — `tools/common.ps1`

```powershell
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
```

`tools/backup-saves.ps1`

```powershell
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
```

- [ ] **Step 5: 테스트 통과 확인**

Run: `pwsh -NoProfile -File E:\MLToybox\tools\tests\Tools.Tests.ps1`
Expected: 3개 PASS, `ALL PASS`

- [ ] **Step 6: 커밋**

```powershell
git -C E:\MLToybox add tools
git -C E:\MLToybox commit -m "feat(tools): add game dir detection and save backup script`n`nCo-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 2: deploy.ps1 — 모드 설치·제거·등록

**Files:**
- Create: `tools/deploy.ps1`
- Create: `tools/tests/fixtures/mods.txt`, `tools/tests/fixtures/mods.json` (실제 게임 파일 사본)
- Modify: `tools/tests/Tools.Tests.ps1` (마지막 `if ($script:failed ...` 앞에 케이스 추가)

**Interfaces:**
- Consumes: Task 1의 `Get-MLModsDir`, `Assert-*`
- Produces: `deploy.ps1 -Mod <MLToybox|MLToyboxDump> [-GameDir <path>] [-Remove] [-Panel]`. 설치 시 `mod\<Mod>\Scripts` → `<Mods>\<Mod>\Scripts`로 미러링하고, `<Mods>\<Mod>\bridge\`를 만든다(MLToybox만). 기존 bridge 내용은 보존한다. `mods.txt`의 `; Built-in keybinds` 줄 앞에 `<Mod> : 1`, `mods.json`에 `{mod_name, mod_enabled:true}`를 추가하며 중복은 만들지 않는다. `-Remove`는 등록을 해제하고 폴더를 삭제한다. `-Panel`은 Task 9에서 추가한다.

- [ ] **Step 1: fixture 복사**

```powershell
$m = 'E:\SteamLibrary\steamapps\common\Manor Lords\ManorLords\Binaries\Win64\ue4ss\Mods'
New-Item -ItemType Directory -Force E:\MLToybox\tools\tests\fixtures | Out-Null
Copy-Item "$m\mods.txt","$m\mods.json" E:\MLToybox\tools\tests\fixtures\
```

- [ ] **Step 2: 실패하는 테스트 추가** — `Tools.Tests.ps1`의 `if ($script:failed -gt 0)` 줄 바로 위에 삽입

```powershell
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
```

- [ ] **Step 3: 실행해서 실패 확인**

Run: `pwsh -NoProfile -File E:\MLToybox\tools\tests\Tools.Tests.ps1`
Expected: 새 2개 케이스 FAIL (`deploy.ps1` 없음)

- [ ] **Step 4: 구현** — `tools/deploy.ps1`

```powershell
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
```

- [ ] **Step 5: 테스트 통과 확인**

Run: `pwsh -NoProfile -File E:\MLToybox\tools\tests\Tools.Tests.ps1`
Expected: 5개 PASS, `ALL PASS`

- [ ] **Step 6: 테스트용 placeholder가 생겼다면 삭제**

Run: `Get-Content E:\MLToybox\mod\MLToybox\Scripts\main.lua` — 내용이 `-- placeholder created by test`이면 `Remove-Item -Recurse E:\MLToybox\mod`로 지운다(Task 7에서 실제 파일을 만든다).

- [ ] **Step 7: 커밋**

```powershell
git -C E:\MLToybox add tools
git -C E:\MLToybox commit -m "feat(tools): add deploy script with idempotent mod registration`n`nCo-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 3: .NET 솔루션 + Lua 테스트 하네스 + json.lua

**Files:**
- Create: `panel/MLToybox.sln`, `panel/MLToybox.Panel.Core/MLToybox.Panel.Core.csproj`, `panel/MLToybox.Tests/MLToybox.Tests.csproj`
- Create: `panel/MLToybox.Tests/RepoPaths.cs`, `panel/MLToybox.Tests/LuaSpecTests.cs`
- Create: `mod/MLToybox/Scripts/lib/json.lua` (rxi v0.1.2 원본 그대로)
- Create: `mod/MLToybox/tests/t.lua`, `mod/MLToybox/tests/json_spec.lua`

**Interfaces:**
- Produces:
  - Lua 테스트 규약: `mod/MLToybox/tests/*_spec.lua`는 `local T = require("t")` 후 `T.run({ name = function() ... end, ... })`을 호출한다. 전역 `TEST_TMP`(쓰기 가능한 임시 폴더, 끝에 `\` 없음)가 주어진다. `package.path`에는 `Scripts\?.lua`와 `tests\?.lua`가 들어 있다.
  - `T.eq(actual, expected, msg)`, `T.truthy(v, msg)`, `T.run(cases) -> count`
  - C# `RepoPaths.Root`, `RepoPaths.Scripts`, `RepoPaths.LuaTests`

- [ ] **Step 1: 솔루션·프로젝트 생성**

```powershell
cd E:\MLToybox\panel
dotnet new sln -n MLToybox
dotnet new classlib -n MLToybox.Panel.Core -f net8.0
dotnet new xunit -n MLToybox.Tests -f net8.0
dotnet sln add MLToybox.Panel.Core MLToybox.Tests
dotnet add MLToybox.Tests reference MLToybox.Panel.Core
dotnet add MLToybox.Tests package NLua
Remove-Item MLToybox.Panel.Core\Class1.cs, MLToybox.Tests\UnitTest1.cs
```

두 csproj의 `<TargetFramework>`를 `net8.0-windows`로 바꾼다. `MLToybox.Panel.Core.csproj`의 PropertyGroup에 `<Nullable>enable</Nullable>`이 있는지 확인한다(템플릿 기본값).

- [ ] **Step 2: json.lua 벤더링**

```powershell
New-Item -ItemType Directory -Force E:\MLToybox\mod\MLToybox\Scripts\lib | Out-Null
Invoke-WebRequest https://raw.githubusercontent.com/rxi/json.lua/v0.1.2/json.lua -OutFile E:\MLToybox\mod\MLToybox\Scripts\lib\json.lua
Select-String -Path E:\MLToybox\mod\MLToybox\Scripts\lib\json.lua -Pattern 'json.lua','MIT' | Select -First 3
```
Expected: 헤더에 `json.lua`와 MIT 라이선스 문구가 보인다.

- [ ] **Step 3: Lua 테스트 도우미와 첫 spec 작성**

`mod/MLToybox/tests/t.lua`

```lua
local T = {}

function T.eq(actual, expected, msg)
  if actual ~= expected then
    error(string.format("%s: expected <%s> got <%s>", msg or "eq", tostring(expected), tostring(actual)), 2)
  end
end

function T.truthy(v, msg)
  if not v then error(string.format("%s: got <%s>", msg or "truthy", tostring(v)), 2) end
end

function T.run(cases)
  local names = {}
  for name in pairs(cases) do names[#names + 1] = name end
  table.sort(names)
  for _, name in ipairs(names) do
    local ok, err = pcall(cases[name])
    if not ok then error(name .. " FAILED: " .. tostring(err), 0) end
  end
  return #names
end

return T
```

`mod/MLToybox/tests/json_spec.lua`

```lua
local T = require("t")
local json = require("lib.json")

T.run({
  roundtrip_object = function()
    local s = json.encode({ a = 1, b = "x" })
    local d = json.decode(s)
    T.eq(d.a, 1, "a")
    T.eq(d.b, "x", "b")
  end,
  temp_dir_is_writable = function()
    local p = TEST_TMP .. "\\probe.txt"
    local f = assert(io.open(p, "wb")); f:write("ok"); f:close()
    T.truthy(io.open(p, "rb"), "readable")
  end,
})
```

- [ ] **Step 4: C# 러너 작성**

`panel/MLToybox.Tests/RepoPaths.cs`

```csharp
namespace MLToybox.Tests;

public static class RepoPaths
{
    public static string Root { get; } = FindRoot();
    public static string Scripts => Path.Combine(Root, "mod", "MLToybox", "Scripts");
    public static string LuaTests => Path.Combine(Root, "mod", "MLToybox", "tests");

    private static string FindRoot()
    {
        var dir = new DirectoryInfo(AppContext.BaseDirectory);
        while (dir is not null && !File.Exists(Path.Combine(dir.FullName, "CLAUDE.md")))
            dir = dir.Parent;
        return dir?.FullName ?? throw new InvalidOperationException("repo root (CLAUDE.md) not found");
    }
}
```

`panel/MLToybox.Tests/LuaSpecTests.cs`

```csharp
using NLua;
using NLua.Exceptions;
using Xunit;

namespace MLToybox.Tests;

public class LuaSpecTests
{
    public static IEnumerable<object[]> Specs() =>
        Directory.GetFiles(RepoPaths.LuaTests, "*_spec.lua")
            .Select(p => new object[] { Path.GetFileName(p) });

    [Theory]
    [MemberData(nameof(Specs))]
    public void Spec(string file)
    {
        var tmp = Directory.CreateTempSubdirectory("mltb-lua-").FullName;
        using var lua = new Lua();
        lua.State.Encoding = System.Text.Encoding.UTF8;
        lua["TEST_TMP"] = tmp;
        lua.DoString($"package.path = [[{RepoPaths.Scripts}\\?.lua;{RepoPaths.LuaTests}\\?.lua;]] .. package.path");
        lua.DoString("require('core.log').sink = function() end", "silence");
        try
        {
            lua.DoFile(Path.Combine(RepoPaths.LuaTests, file));
        }
        catch (LuaScriptException ex)
        {
            Assert.Fail($"{file}: {ex.Message}");
        }
    }
}
```

이 시점에는 `core.log`가 없어서 `silence` 줄에서 실패한다. Step 5에서 확인한 뒤 Step 6에서 최소한의 log를 만든다.

- [ ] **Step 5: 실행해서 실패 확인**

Run: `dotnet test E:\MLToybox\panel\MLToybox.sln`
Expected: `Spec(file: "json_spec.lua")` FAIL, 메시지에 `module 'core.log' not found`

- [ ] **Step 6: 최소 log 모듈 작성** — `mod/MLToybox/Scripts/core/log.lua`

```lua
local M = {}
local PREFIX = "[MLToybox] "

M.sink = print

function M.info(fmt, ...)
  M.sink(PREFIX .. string.format(fmt, ...) .. "\n")
end

function M.error(fmt, ...)
  M.sink(PREFIX .. "ERROR " .. string.format(fmt, ...) .. "\n")
end

return M
```

- [ ] **Step 7: 테스트 통과 확인**

Run: `dotnet test E:\MLToybox\panel\MLToybox.sln`
Expected: `json_spec.lua` PASS (1 passed)

- [ ] **Step 8: 커밋**

```powershell
git -C E:\MLToybox add panel mod
git -C E:\MLToybox commit -m "build: add .NET solution with NLua-based Lua spec runner and vendored json.lua`n`nCo-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 4: Lua 코어 유틸 — safe, paths, fileio

**Files:**
- Create: `mod/MLToybox/Scripts/core/safe.lua`, `core/paths.lua`, `core/fileio.lua`
- Create: `mod/MLToybox/tests/safe_spec.lua`, `tests/paths_spec.lua`, `tests/fileio_spec.lua`

**Interfaces:**
- Consumes: `core.log` (Task 3)
- Produces:
  - `safe.threshold` (number, 기본 5), `safe.setThreshold(n)`, `safe.onTrip(name, fn(err))`, `safe.reset(name)`, `safe.failures(name) -> number`, `safe.call(name, fn, ...) -> ok:boolean, ...results|err:string`, `safe.valid(obj) -> boolean` (`obj:IsValid()`를 pcall로 호출)
  - `paths.parentDir(path) -> string|nil`, `paths.scriptsDirFromSource(source) -> string` (`"@C:\\x\\Scripts\\main.lua"` → `"C:\\x\\Scripts"`)
  - `fileio.read(path) -> string|nil`, `fileio.writeAtomic(path, content) -> true | false, err`

- [ ] **Step 1: 실패하는 테스트 작성**

`mod/MLToybox/tests/safe_spec.lua`

```lua
local T = require("t")
local safe = require("core.safe")

T.run({
  success_returns_results_and_resets = function()
    safe.reset("a")
    local ok, x, y = safe.call("a", function(p) return p + 1, "y" end, 1)
    T.eq(ok, true, "ok"); T.eq(x, 2, "x"); T.eq(y, "y", "y")
    T.eq(safe.failures("a"), 0, "failures")
  end,
  failure_counts_and_returns_error = function()
    safe.reset("b")
    local ok, err = safe.call("b", function() error("boom") end)
    T.eq(ok, false, "ok")
    T.truthy(tostring(err):find("boom"), "err text")
    T.eq(safe.failures("b"), 1, "failures")
  end,
  trip_fires_once_at_threshold_then_resets = function()
    safe.setThreshold(3)
    safe.reset("c")
    local trips = 0
    safe.onTrip("c", function() trips = trips + 1 end)
    for _ = 1, 3 do safe.call("c", function() error("x") end) end
    T.eq(trips, 1, "tripped once")
    T.eq(safe.failures("c"), 0, "reset after trip")
    safe.setThreshold(5)
  end,
  success_between_failures_resets_counter = function()
    safe.setThreshold(2)
    safe.reset("d")
    local trips = 0
    safe.onTrip("d", function() trips = trips + 1 end)
    safe.call("d", function() error("x") end)
    safe.call("d", function() end)
    safe.call("d", function() error("x") end)
    T.eq(trips, 0, "not consecutive")
    safe.setThreshold(5)
  end,
  valid_handles_nil_and_throwing_objects = function()
    T.eq(safe.valid(nil), false, "nil")
    T.eq(safe.valid({ IsValid = function() return true end }), true, "valid")
    T.eq(safe.valid({ IsValid = function() error("dead") end }), false, "throws")
    T.eq(safe.valid({}), false, "no method")
  end,
})
```

`mod/MLToybox/tests/paths_spec.lua`

```lua
local T = require("t")
local paths = require("core.paths")

T.run({
  scripts_dir_from_source = function()
    T.eq(paths.scriptsDirFromSource("@E:\\G\\Mods\\MLToybox\\Scripts\\main.lua"), "E:\\G\\Mods\\MLToybox\\Scripts", "backslash")
    T.eq(paths.scriptsDirFromSource("@E:/G/Mods/MLToybox/Scripts/main.lua"), "E:/G/Mods/MLToybox/Scripts", "slash")
  end,
  parent_dir = function()
    T.eq(paths.parentDir("E:\\G\\Mods\\MLToybox\\Scripts"), "E:\\G\\Mods\\MLToybox", "parent")
    T.eq(paths.parentDir("nodir"), nil, "no separator")
  end,
})
```

`mod/MLToybox/tests/fileio_spec.lua`

```lua
local T = require("t")
local fileio = require("core.fileio")

T.run({
  read_missing_returns_nil = function()
    T.eq(fileio.read(TEST_TMP .. "\\nope.json"), nil, "missing")
  end,
  write_atomic_creates_and_overwrites = function()
    local p = TEST_TMP .. "\\s.json"
    T.eq(fileio.writeAtomic(p, "one"), true, "first")
    T.eq(fileio.writeAtomic(p, "two"), true, "overwrite")
    T.eq(fileio.read(p), "two", "content")
    T.eq(fileio.read(p .. ".tmp"), nil, "tmp removed")
  end,
  write_atomic_into_missing_dir_fails_cleanly = function()
    local ok, err = fileio.writeAtomic(TEST_TMP .. "\\no\\such\\dir\\x.json", "x")
    T.eq(ok, false, "fails")
    T.truthy(err, "has error")
  end,
})
```

- [ ] **Step 2: 실행해서 실패 확인**

Run: `dotnet test E:\MLToybox\panel\MLToybox.sln`
Expected: safe/paths/fileio spec 3개 FAIL (`module ... not found`)

- [ ] **Step 3: 구현**

`mod/MLToybox/Scripts/core/safe.lua`

```lua
local log = require("core.log")

local M = { threshold = 5 }
local failures = {}
local tripHandlers = {}

function M.setThreshold(n) M.threshold = n end
function M.onTrip(name, fn) tripHandlers[name] = fn end
function M.reset(name) failures[name] = 0 end
function M.failures(name) return failures[name] or 0 end

function M.call(name, fn, ...)
  local results = table.pack(pcall(fn, ...))
  if results[1] then
    failures[name] = 0
    return true, table.unpack(results, 2, results.n)
  end
  local err = tostring(results[2])
  failures[name] = (failures[name] or 0) + 1
  log.error("%s: %s", name, err)
  if failures[name] >= M.threshold then
    failures[name] = 0
    local handler = tripHandlers[name]
    if handler then
      local ok, herr = pcall(handler, err)
      if not ok then log.error("%s trip handler: %s", name, tostring(herr)) end
    end
  end
  return false, err
end

function M.valid(obj)
  if obj == nil then return false end
  local ok, v = pcall(function() return obj:IsValid() end)
  return ok and v == true
end

return M
```

`mod/MLToybox/Scripts/core/paths.lua`

```lua
local M = {}

function M.parentDir(path)
  return path:match("^(.*)[\\/][^\\/]+$")
end

function M.scriptsDirFromSource(source)
  return M.parentDir((source:gsub("^@", "")))
end

return M
```

`mod/MLToybox/Scripts/core/fileio.lua`

```lua
local M = {}

function M.read(path)
  local f = io.open(path, "rb")
  if not f then return nil end
  local content = f:read("a")
  f:close()
  return content
end

-- Windows os.rename 은 대상이 있으면 실패하므로 삭제 후 rename 한다.
-- 그 사이 읽는 쪽은 파일이 잠깐 없을 수 있으며, 읽는 쪽이 다음 폴링에서 재시도한다.
function M.writeAtomic(path, content)
  local tmp = path .. ".tmp"
  local f, err = io.open(tmp, "wb")
  if not f then return false, err end
  f:write(content)
  f:close()
  os.remove(path)
  local ok, rerr = os.rename(tmp, path)
  if not ok then
    os.remove(tmp)
    return false, rerr
  end
  return true
end

return M
```

- [ ] **Step 4: 테스트 통과 확인**

Run: `dotnet test E:\MLToybox\panel\MLToybox.sln`
Expected: spec 4개(json, safe, paths, fileio) 모두 PASS

- [ ] **Step 5: 커밋**

```powershell
git -C E:\MLToybox add mod
git -C E:\MLToybox commit -m "feat(lua): add safe call wrapper, path and atomic file helpers`n`nCo-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 5: bridge.lua — control 폴링과 status 기록

**Files:**
- Create: `mod/MLToybox/Scripts/core/bridge.lua`
- Create: `mod/MLToybox/tests/bridge_spec.lua`

**Interfaces:**
- Consumes: `lib.json`, `core.fileio`
- Produces:
  - `bridge.VERSION == 1`
  - `bridge.parseControl(text) -> control | nil, err` (version·seq·features 검증)
  - `bridge.new(dir) -> b`, 필드 `b.controlPath`, `b.statusPath`, `b.lastSeq`(number|nil), `b.lastError`(string|nil)
  - `b:poll() -> control | nil` (새 seq일 때만 반환. 파일이 없으면 nil이고 lastError는 바꾸지 않는다. 파싱에 실패하면 nil을 반환하고 lastError를 설정하며 lastSeq는 유지한다.)
  - `b:writeStatus(tbl) -> true | false, err`

- [ ] **Step 1: 실패하는 테스트 작성** — `mod/MLToybox/tests/bridge_spec.lua`

```lua
local T = require("t")
local bridge = require("core.bridge")
local json = require("lib.json")

local n = 0
local function fresh()
  n = n + 1
  local dir = TEST_TMP .. "\\b" .. n
  os.execute('mkdir "' .. dir .. '"')
  return bridge.new(dir)
end
local function writeControl(b, text)
  local f = assert(io.open(b.controlPath, "wb")); f:write(text); f:close()
end
local function ctl(seq, extra)
  local t = { version = 1, seq = seq, features = extra or { build = { enabled = true } } }
  return json.encode(t)
end

T.run({
  poll_missing_file_returns_nil = function()
    local b = fresh()
    T.eq(b:poll(), nil, "nil")
    T.eq(b.lastError, nil, "no error")
  end,
  poll_returns_new_seq_once = function()
    local b = fresh()
    writeControl(b, ctl(1))
    local c = b:poll()
    T.eq(c.seq, 1, "seq")
    T.eq(c.features.build.enabled, true, "payload")
    T.eq(b:poll(), nil, "same seq again")
    writeControl(b, ctl(2))
    T.eq(b:poll().seq, 2, "next seq")
  end,
  poll_bad_json_keeps_last_seq = function()
    local b = fresh()
    writeControl(b, ctl(5)); b:poll()
    writeControl(b, "{not json")
    T.eq(b:poll(), nil, "nil on bad json")
    T.truthy(b.lastError and b.lastError:find("parse"), "parse error reported")
    T.eq(b.lastSeq, 5, "last seq kept")
    writeControl(b, ctl(5))
    T.eq(b:poll(), nil, "same seq still ignored")
    T.eq(b.lastError, nil, "error cleared on good file")
  end,
  poll_version_mismatch = function()
    local b = fresh()
    writeControl(b, '{"version":2,"seq":1,"features":{}}')
    T.eq(b:poll(), nil, "rejected")
    T.truthy(b.lastError:find("version"), "version error")
  end,
  parse_rejects_missing_fields = function()
    T.eq((bridge.parseControl('{"version":1,"features":{}}')), nil, "no seq")
    T.eq((bridge.parseControl('{"version":1,"seq":1}')), nil, "no features")
    T.eq((bridge.parseControl('[]')), nil, "array")
    T.eq((bridge.parseControl('')), nil, "empty")
  end,
  write_status_roundtrip = function()
    local b = fresh()
    T.eq(b:writeStatus({ version = 1, heartbeat = 123, inGame = false }), true, "written")
    local f = assert(io.open(b.statusPath, "rb")); local s = f:read("a"); f:close()
    local d = json.decode(s)
    T.eq(d.heartbeat, 123, "heartbeat")
    T.eq(d.inGame, false, "inGame")
  end,
})
```

- [ ] **Step 2: 실행해서 실패 확인**

Run: `dotnet test E:\MLToybox\panel\MLToybox.sln`
Expected: `bridge_spec.lua` FAIL (`module 'core.bridge' not found`)

- [ ] **Step 3: 구현** — `mod/MLToybox/Scripts/core/bridge.lua`

```lua
local json = require("lib.json")
local fileio = require("core.fileio")

local M = { VERSION = 1 }

function M.parseControl(text)
  if text == nil or text == "" then return nil, "empty control" end
  local ok, data = pcall(json.decode, text)
  if not ok then return nil, "parse error: " .. tostring(data) end
  if type(data) ~= "table" then return nil, "control is not an object" end
  if data.version ~= M.VERSION then return nil, "version mismatch: " .. tostring(data.version) end
  if type(data.seq) ~= "number" then return nil, "missing seq" end
  if type(data.features) ~= "table" then return nil, "missing features" end
  return data
end

function M.new(dir)
  local b = {
    dir = dir,
    controlPath = dir .. "\\control.json",
    statusPath = dir .. "\\status.json",
    lastSeq = nil,
    lastError = nil,
  }

  function b:poll()
    local text = fileio.read(self.controlPath)
    if text == nil then return nil end
    local control, err = M.parseControl(text)
    if not control then
      self.lastError = err
      return nil
    end
    self.lastError = nil
    if control.seq == self.lastSeq then return nil end
    self.lastSeq = control.seq
    return control
  end

  function b:writeStatus(status)
    return fileio.writeAtomic(self.statusPath, json.encode(status))
  end

  return b
end

return M
```

- [ ] **Step 4: 테스트 통과 확인**

Run: `dotnet test E:\MLToybox\panel\MLToybox.sln`
Expected: 모든 spec PASS

- [ ] **Step 5: 커밋**

```powershell
git -C E:\MLToybox add mod
git -C E:\MLToybox commit -m "feat(lua): add control/status bridge with seq-based change detection`n`nCo-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 6: registry.lua — 기능 수명주기

**Files:**
- Create: `mod/MLToybox/Scripts/core/registry.lua`
- Create: `mod/MLToybox/tests/registry_spec.lua`

**Interfaces:**
- Consumes: `core.safe`
- Produces (Plan 2의 기능 모듈이 따를 계약):
  - 기능 모듈: `{ name:string, intervalSec?:number, enable?(state, settings), disable?(state, settings), configure?(state, settings), tick?(state, settings) }`. `settings`는 `control.json`의 `features[name]` 테이블이다. `state`는 레지스트리가 공유하는 테이블로, `state.inGame`, 그리고 기능이 채우는 `state.resourceIds`(배열)와 `state.resources`(id→number)를 담는다.
  - `registry.new() -> r`
  - `r:add(feature)`, `r:apply(featuresCfg)`, `r:setInGame(bool)`, `r:tick(nowSec)`, `r:status(nowSec, appliedSeq, bridgeError) -> table`
  - 필드 `r.active[name]`, `r.errors[name]`, `r.tripped[name]`, `r.state`
  - 규칙:
    - 기능이 켜지는 조건은 `state.inGame`이고, `settings.enabled == true`이며, trip 상태가 아닐 때다.
    - `apply`는 전달된 기능의 trip을 해제하고, 이미 켜져 있으면 `configure`를 호출한다.
    - `setInGame(false)`는 켜진 기능을 모두 disable한다.
    - `tick`의 간격은 `settings.intervalSec`, `feature.intervalSec`, 2초 순서로 정한다.

- [ ] **Step 1: 실패하는 테스트 작성** — `mod/MLToybox/tests/registry_spec.lua`

```lua
local T = require("t")
local registry = require("core.registry")
local safe = require("core.safe")

local function fake(name, opts)
  opts = opts or {}
  local f = { name = name, calls = {}, intervalSec = opts.intervalSec }
  local function rec(m) return function(state, s) f.calls[#f.calls + 1] = m; if opts.fail == m then error(m .. " failed") end end end
  f.enable, f.disable, f.configure, f.tick = rec("enable"), rec("disable"), rec("configure"), rec("tick")
  return f
end
local function count(f, m) local c = 0 for _, x in ipairs(f.calls) do if x == m then c = c + 1 end end return c end

T.run({
  not_enabled_until_in_game = function()
    local r = registry.new(); local a = fake("a"); r:add(a)
    r:apply({ a = { enabled = true } })
    T.eq(count(a, "enable"), 0, "menu")
    r:setInGame(true)
    T.eq(count(a, "enable"), 1, "in game")
    T.eq(r.active.a, true, "active")
  end,
  apply_toggles_and_configures = function()
    local r = registry.new(); local a = fake("a"); r:add(a); r:setInGame(true)
    r:apply({ a = { enabled = true } })
    r:apply({ a = { enabled = true, x = 1 } })
    T.eq(count(a, "configure"), 1, "configure on change while active")
    r:apply({ a = { enabled = false } })
    T.eq(count(a, "disable"), 1, "disabled")
    T.eq(r.active.a, false, "inactive")
  end,
  unknown_feature_in_control_is_ignored = function()
    local r = registry.new(); r:add(fake("a")); r:setInGame(true)
    r:apply({ zzz = { enabled = true } })
    T.eq(r.active.zzz, nil, "ignored")
  end,
  leaving_game_disables_all = function()
    local r = registry.new(); local a, b = fake("a"), fake("b"); r:add(a); r:add(b)
    r:setInGame(true); r:apply({ a = { enabled = true }, b = { enabled = true } })
    r:setInGame(false)
    T.eq(count(a, "disable") + count(b, "disable"), 2, "both disabled")
    T.eq(r.active.a or r.active.b, false, "none active")
  end,
  reentering_game_reenables = function()
    local r = registry.new(); local a = fake("a"); r:add(a)
    r:apply({ a = { enabled = true } }); r:setInGame(true); r:setInGame(false); r:setInGame(true)
    T.eq(count(a, "enable"), 2, "enabled twice")
  end,
  tick_respects_interval = function()
    local r = registry.new(); local a = fake("a"); r:add(a); r:setInGame(true)
    r:apply({ a = { enabled = true, intervalSec = 3 } })
    r:tick(100); r:tick(101); r:tick(102); r:tick(103)
    T.eq(count(a, "tick"), 2, "t=100 and t=103")
  end,
  tick_default_interval_is_2 = function()
    local r = registry.new(); local a = fake("a"); r:add(a); r:setInGame(true)
    r:apply({ a = { enabled = true } })
    r:tick(10); r:tick(11); r:tick(12)
    T.eq(count(a, "tick"), 2, "t=10 and t=12")
  end,
  trip_disables_only_that_feature = function()
    safe.setThreshold(3)
    local r = registry.new(); local bad, good = fake("bad", { fail = "tick" }), fake("good"); r:add(bad); r:add(good)
    r:setInGame(true); r:apply({ bad = { enabled = true, intervalSec = 1 }, good = { enabled = true, intervalSec = 1 } })
    for t = 1, 5 do r:tick(t) end
    T.eq(r.active.bad, false, "bad off")
    T.eq(r.tripped.bad, true, "tripped")
    T.truthy(r.errors.bad:find("auto%-disabled"), "reason kept")
    T.eq(r.active.good, true, "good on")
    T.eq(count(good, "tick"), 5, "good kept ticking")
    safe.setThreshold(5)
  end,
  apply_clears_trip = function()
    safe.setThreshold(1)
    local r = registry.new(); local bad = fake("bad", { fail = "tick" }); r:add(bad)
    r:setInGame(true); r:apply({ bad = { enabled = true } }); r:tick(1)
    T.eq(r.tripped.bad, true, "tripped")
    r:setInGame(false); r:setInGame(true)
    T.eq(r.active.bad, false, "stays off across maps")
    r:apply({ bad = { enabled = true } })
    T.eq(r.active.bad, true, "retry after apply")
    safe.setThreshold(5)
  end,
  enable_failure_reports_error_and_stays_off = function()
    local r = registry.new(); local a = fake("a", { fail = "enable" }); r:add(a); r:setInGame(true)
    r:apply({ a = { enabled = true } })
    T.eq(r.active.a, false, "off")
    T.truthy(r.errors.a:find("enable failed"), "error")
  end,
  status_shape = function()
    local r = registry.new(); r:add(fake("a")); r:setInGame(true); r:apply({ a = { enabled = true } })
    local s = r:status(42, 7, "oops")
    T.eq(s.version, 1, "version"); T.eq(s.heartbeat, 42, "hb"); T.eq(s.inGame, true, "inGame")
    T.eq(s.appliedSeq, 7, "seq"); T.eq(s.bridgeError, "oops", "bridgeError")
    T.eq(s.features.a.active, true, "feature active")
    T.eq(s.resources, nil, "empty resources omitted")
    T.eq(s.resourceIds, nil, "empty ids omitted")
  end,
  status_without_features_omits_map = function()
    local s = registry.new():status(1, nil, nil)
    T.eq(s.features, nil, "no features -> nil (avoids [] encoding)")
  end,
})
```

- [ ] **Step 2: 실행해서 실패 확인**

Run: `dotnet test E:\MLToybox\panel\MLToybox.sln`
Expected: `registry_spec.lua` FAIL (`module 'core.registry' not found`)

- [ ] **Step 3: 구현** — `mod/MLToybox/Scripts/core/registry.lua`

```lua
local safe = require("core.safe")

local M = { DEFAULT_INTERVAL = 2 }

function M.new()
  local r = {
    features = {}, order = {}, active = {}, settings = {},
    errors = {}, tripped = {}, lastTick = {},
    state = { inGame = false },
  }

  function r:add(feature)
    assert(type(feature) == "table" and type(feature.name) == "string", "feature needs a name")
    local name = feature.name
    self.features[name] = feature
    self.order[#self.order + 1] = name
    self.active[name] = false
    safe.reset(name)
    safe.onTrip(name, function(err)
      self.tripped[name] = true
      self:_deactivate(name, "auto-disabled: " .. tostring(err))
    end)
  end

  function r:_invoke(name, method)
    local fn = self.features[name][method]
    if fn == nil then return true end
    local ok, err = safe.call(name, fn, self.state, self.settings[name])
    if not ok and not self.tripped[name] then self.errors[name] = tostring(err) end
    return ok
  end

  function r:_activate(name)
    if self:_invoke(name, "enable") then
      self.active[name] = true
      self.errors[name] = nil
      self.lastTick[name] = nil
    end
  end

  function r:_deactivate(name, reason)
    if self.active[name] then
      self.active[name] = false
      local fn = self.features[name].disable
      if fn then pcall(fn, self.state, self.settings[name]) end
    end
    if reason then self.errors[name] = reason end
  end

  function r:_wants(name)
    local s = self.settings[name]
    return self.state.inGame and not self.tripped[name] and type(s) == "table" and s.enabled == true
  end

  function r:_sync()
    for _, name in ipairs(self.order) do
      local want = self:_wants(name)
      if want and not self.active[name] then
        self:_activate(name)
      elseif not want and self.active[name] then
        self:_deactivate(name)
      end
    end
  end

  function r:apply(featuresCfg)
    if type(featuresCfg) ~= "table" then return end
    for _, name in ipairs(self.order) do
      local cfg = featuresCfg[name]
      if type(cfg) == "table" then
        self.settings[name] = cfg
        self.tripped[name] = nil
        if self.active[name] and cfg.enabled == true then self:_invoke(name, "configure") end
      end
    end
    self:_sync()
  end

  function r:setInGame(inGame)
    if not inGame then
      for _, name in ipairs(self.order) do self:_deactivate(name) end
    end
    self.state.inGame = inGame
    self:_sync()
  end

  function r:tick(now)
    for _, name in ipairs(self.order) do
      local f = self.features[name]
      if self.active[name] and f.tick then
        local s = self.settings[name]
        local interval = (type(s) == "table" and tonumber(s.intervalSec)) or f.intervalSec or M.DEFAULT_INTERVAL
        local last = self.lastTick[name]
        if last == nil or now - last >= interval then
          self.lastTick[name] = now
          self:_invoke(name, "tick")
        end
      end
    end
  end

  function r:status(now, appliedSeq, bridgeError)
    local features = {}
    for _, name in ipairs(self.order) do
      features[name] = { active = self.active[name], lastError = self.errors[name] }
    end
    local st = self.state
    return {
      version = 1,
      heartbeat = now,
      inGame = st.inGame,
      appliedSeq = appliedSeq,
      bridgeError = bridgeError,
      features = next(features) and features or nil,
      resourceIds = (type(st.resourceIds) == "table" and #st.resourceIds > 0) and st.resourceIds or nil,
      resources = (type(st.resources) == "table" and next(st.resources)) and st.resources or nil,
    }
  end

  return r
end

return M
```

- [ ] **Step 4: 테스트 통과 확인**

Run: `dotnet test E:\MLToybox\panel\MLToybox.sln`
Expected: 모든 spec PASS

- [ ] **Step 5: 커밋**

```powershell
git -C E:\MLToybox add mod
git -C E:\MLToybox commit -m "feat(lua): add feature registry with lifecycle, interval ticks and auto-trip`n`nCo-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 7: main.lua·config.lua + 인게임 스모크 (UE4SS 기동 확인)

**Files:**
- Create: `mod/MLToybox/Scripts/config.lua`, `mod/MLToybox/Scripts/main.lua`

**Interfaces:**
- Consumes: `core.*` 전부 (Task 3~6), `deploy.ps1` (Task 2), `backup-saves.ps1` (Task 1)
- Produces:
  - `config.lua` 필드: `failureThreshold`, `pollIntervalMs`, `gameStateClassPattern`(string|nil, nil이면 모든 GameState를 인게임으로 간주), `featureModules`(require할 `features.<name>` 이름 배열, Plan 1에서는 빈 배열), `defaults`(control.json이 없을 때 적용할 features 테이블)
  - 로그 줄 `[MLToybox] loaded (scripts=..., bridge=...)`, `[MLToybox] GameState: <FullName> inGame=<bool>` (Plan 2 분석에서 메뉴/인게임 판별에 사용)

- [ ] **Step 1: config.lua 작성**

```lua
return {
  failureThreshold = 5,
  pollIntervalMs = 1000,
  -- GameState 전체 이름에 이 Lua 패턴이 들어 있을 때만 인게임으로 본다. nil 이면 모든 GameState를 인게임으로 본다.
  -- Task 11 분석에서 메뉴/인게임 GameState 이름을 확인해 설정한다.
  gameStateClassPattern = nil,
  -- 로드할 기능 모듈 이름 (Scripts/features/<name>.lua). Plan 2에서 추가한다.
  featureModules = {},
  -- control.json 이 아직 없을 때 쓰는 기본 설정 (features 테이블과 같은 모양)
  defaults = {},
}
```

- [ ] **Step 2: main.lua 작성**

```lua
local source = debug.getinfo(1, "S").source
local scriptsDir = source:gsub("^@", ""):match("^(.*)[\\/][^\\/]+$")
package.path = scriptsDir .. "\\?.lua;" .. package.path

local paths = require("core.paths")
local log = require("core.log")
local safe = require("core.safe")
local bridgeLib = require("core.bridge")
local registryLib = require("core.registry")
local config = require("config")

safe.setThreshold(config.failureThreshold)

local bridgeDir = paths.parentDir(scriptsDir) .. "\\bridge"
local bridge = bridgeLib.new(bridgeDir)
local registry = registryLib.new()
local appliedSeq = nil

for _, name in ipairs(config.featureModules) do
  local ok, mod = pcall(require, "features." .. name)
  if ok then registry:add(mod) else log.error("load feature %s: %s", name, tostring(mod)) end
end
registry:apply(config.defaults)

local function isGameplayState(fullName)
  if config.gameStateClassPattern == nil then return true end
  return fullName:find(config.gameStateClassPattern) ~= nil
end

RegisterLoadMapPreHook(function()
  safe.call("core", function() registry:setInGame(false) end)
end)

RegisterInitGameStatePostHook(function(context)
  safe.call("core", function()
    local gameState = context:get()
    local fullName = gameState:GetFullName()
    local inGame = isGameplayState(fullName)
    log.info("GameState: %s inGame=%s", fullName, tostring(inGame))
    registry:setInGame(inGame)
  end)
end)

LoopAsync(config.pollIntervalMs, function()
  local ok, err = pcall(function()
    local control = bridge:poll()
    ExecuteInGameThread(function()
      safe.call("core", function()
        if control then
          registry:apply(control.features)
          appliedSeq = control.seq
        end
        local now = os.time()
        registry:tick(now)
        local wrote, werr = bridge:writeStatus(registry:status(now, appliedSeq, bridge.lastError))
        if not wrote then log.error("status write: %s", tostring(werr)) end
      end)
    end)
  end)
  if not ok then log.error("loop: %s", tostring(err)) end
  return false
end)

log.info("loaded (scripts=%s, bridge=%s)", scriptsDir, bridgeDir)
```

- [ ] **Step 3: 단위 테스트 회귀 확인**

Run: `dotnet test E:\MLToybox\panel\MLToybox.sln`
Expected: 모든 spec PASS (main.lua는 UE4SS 전역이 필요해 단위 테스트하지 않는다)

- [ ] **Step 4: 세이브 백업 → 배포**

```powershell
pwsh -NoProfile -File E:\MLToybox\tools\backup-saves.ps1
pwsh -NoProfile -File E:\MLToybox\tools\deploy.ps1 -Mod MLToybox
```
Expected: `Backed up ...`, `Deployed MLToybox -> ...\ue4ss\Mods\MLToybox`

- [ ] **Step 5: 게임 실행 후 UE4SS·모드 로드 확인**

```powershell
Start-Process 'steam://rungameid/1363080'
```
90초 뒤에 확인한다(Monitor 도구로 로그 파일에 `[MLToybox] loaded`가 나타날 때까지 기다리는 방식을 권장한다).

```powershell
$w = 'E:\SteamLibrary\steamapps\common\Manor Lords\ManorLords\Binaries\Win64\ue4ss'
Select-String -Path "$w\UE4SS.log" -Pattern 'UE4SS - v','Unreal Engine','\[MLToybox\]' | Select -First 20
Get-Content "$w\Mods\MLToybox\bridge\status.json"
```
Expected: UE4SS 버전 줄, `[MLToybox] loaded (...)`, 그리고 `status.json`의 `heartbeat`가 현재 epoch 초에 가깝다(`[DateTimeOffset]::UtcNow.ToUnixTimeSeconds()`와 비교해 5초 이내).

**실패 시:**
- `UE4SS.log`가 0바이트로 남으면 UE4SS 자체가 로드되지 않은 것이다. superpowers:systematic-debugging으로 조사한다(`dwmapi.dll` 프록시 로드, 안티치트, `UE4SS-settings.ini`). 원인을 문서화하고 수정한 뒤 이 단계를 다시 한다.
- `[MLToybox]` 줄 대신 Lua 오류가 보이면 해당 줄을 고친다.

- [ ] **Step 6: 인게임 GameState 로그 확인**

세이브를 불러온다. AI가 computer-use로 먼저 시도하고, 불가능하면 사용자에게 "메인 메뉴 → Load Game → 아무 슬롯"을 요청한다. 그 뒤:

```powershell
Select-String -Path "$w\UE4SS.log" -Pattern '\[MLToybox\] GameState' | Select -Last 5
```
Expected: 메뉴와 인게임 두 종류의 `GameState: ...` 줄. 두 이름을 Task 11의 `findings.md`에 기록할 수 있게 메모해 둔다.

- [ ] **Step 7: 게임 종료 후 커밋**

```powershell
git -C E:\MLToybox add mod
git -C E:\MLToybox commit -m "feat(lua): add mod entry point wiring bridge loop, map hooks and registry`n`nCo-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 8: 패널 Core — 모델·경로 탐지·브리지 클라이언트·설정

**Files:**
- Create: `panel/MLToybox.Panel.Core/BridgeJson.cs`, `ControlDocument.cs`, `StatusDocument.cs`, `GameLocator.cs`, `BridgeClient.cs`, `PanelSettings.cs`, `ResourceRows.cs`
- Create: `panel/MLToybox.Tests/GameLocatorTests.cs`, `BridgeClientTests.cs`, `PanelSettingsTests.cs`, `ResourceRowsTests.cs`

**Interfaces:**
- Produces (Task 9 UI가 사용):
  - `BridgeJson.Options` (`JsonSerializerDefaults.Web`, `WriteIndented = true`)
  - `ControlDocument { int Version=1; long Seq; FeaturesControl Features }`. `FeaturesControl { ResourcesControl Resources; BuildControl Build; UpgradeControl Upgrade; MilitaryControl Military }`
    - `ResourcesControl { bool Enabled; int IntervalSec=2; Dictionary<string,int> Targets }`
    - `BuildControl { bool Enabled; bool IgnorePlacement=true; bool InstantBuild=true; bool InstantRepair=true }`
    - `UpgradeControl { bool Enabled }`
    - `MilitaryControl { bool Enabled; bool IgnoreEquipment=true; bool IgnorePopulation=true; bool ZeroUpkeep=true; bool UnlimitedSquads=true }`
  - `StatusDocument { int Version; long Heartbeat; bool InGame; long? AppliedSeq; string? BridgeError; Dictionary<string,FeatureStatus>? Features; List<string>? ResourceIds; Dictionary<string,double>? Resources }`, `FeatureStatus { bool Active; string? LastError }`
  - `GameLocator.AppId`, `FindLibraryWithApp(string vdf) -> string?`, `GameDirFromLibrary(string) -> string`, `BridgeDir(string gameDir) -> string`, `LooksLikeGameDir(string) -> bool`, `Detect(string? steamPath) -> string?`, `SteamPathFromRegistry() -> string?`
  - `enum BridgeState { Disconnected, MainMenu, Pending, Applied }`
  - `BridgeClient(string dir, Func<DateTimeOffset>? now = null)`: `ControlPath`, `StatusPath`, `LoadControl() -> ControlDocument`, `SaveControl(ControlDocument) -> long`(새 seq), `ReadStatus() -> StatusDocument?`, `Evaluate(StatusDocument?, long lastSentSeq) -> BridgeState`, `static TimeSpan HeartbeatTimeout = 5s`
  - `PanelSettings { string? GameDir }`, `static string DefaultPath`, `static PanelSettings Load(string path)`, `void Save(string path)`
  - `record ResourceRow(string Id, double? Current, int? Target)`, `ResourceRows.Build(IEnumerable<string>? ids, IReadOnlyDictionary<string,double>? current, IReadOnlyDictionary<string,int> targets) -> List<ResourceRow>` (id 합집합을 정렬해 반환)

- [ ] **Step 1: 실패하는 테스트 작성**

`panel/MLToybox.Tests/GameLocatorTests.cs`

```csharp
using MLToybox.Panel.Core;
using Xunit;

namespace MLToybox.Tests;

public class GameLocatorTests
{
    private const string Vdf = """
        "libraryfolders"
        {
            "0"
            {
                "path"		"C:\\Program Files (x86)\\Steam"
                "apps"
                {
                    "228980"		"364999368"
                }
            }
            "1"
            {
                "path"		"E:\\SteamLibrary"
                "apps"
                {
                    "1363080"		"15360715472"
                }
            }
        }
        """;

    [Fact]
    public void FindLibraryWithApp_ReturnsUnescapedPath() =>
        Assert.Equal(@"E:\SteamLibrary", GameLocator.FindLibraryWithApp(Vdf));

    [Fact]
    public void FindLibraryWithApp_NotInstalled_ReturnsNull() =>
        Assert.Null(GameLocator.FindLibraryWithApp(Vdf.Replace("1363080", "999")));

    [Fact]
    public void FindLibraryWithApp_IgnoresSizeValueMatchingAppId() =>
        Assert.Null(GameLocator.FindLibraryWithApp(Vdf.Replace("\"1363080\"\t\t\"15360715472\"", "\"5\"\t\t\"1363080\"")));

    [Fact]
    public void BridgeDir_IsUnderUe4ssMods() =>
        Assert.Equal(@"G:\ML\ManorLords\Binaries\Win64\ue4ss\Mods\MLToybox\bridge", GameLocator.BridgeDir(@"G:\ML"));

    [Fact]
    public void Detect_RealMachine_FindsInstall()
    {
        var dir = GameLocator.Detect(GameLocator.SteamPathFromRegistry());
        Assert.NotNull(dir);
        Assert.True(GameLocator.LooksLikeGameDir(dir!));
    }
}
```

`panel/MLToybox.Tests/BridgeClientTests.cs`

```csharp
using System.Text.Json;
using MLToybox.Panel.Core;
using Xunit;

namespace MLToybox.Tests;

public class BridgeClientTests
{
    private static readonly DateTimeOffset Now = DateTimeOffset.FromUnixTimeSeconds(1_800_000_000);
    private static BridgeClient NewClient(out string dir)
    {
        dir = Path.Combine(Directory.CreateTempSubdirectory("mltb-bridge-").FullName, "bridge");
        return new BridgeClient(dir, () => Now);
    }

    [Fact]
    public void SaveControl_CreatesDirAndIncrementsSeq()
    {
        var c = NewClient(out _);
        var doc = c.LoadControl();
        Assert.Equal(1, c.SaveControl(doc));
        Assert.Equal(2, c.SaveControl(c.LoadControl()));
        Assert.False(File.Exists(c.ControlPath + ".tmp"));
    }

    [Fact]
    public void SaveControl_WritesCamelCaseProtocol()
    {
        var c = NewClient(out _);
        var doc = new ControlDocument();
        doc.Features.Resources.Enabled = true;
        doc.Features.Resources.Targets["Timber"] = 500;
        c.SaveControl(doc);
        using var json = JsonDocument.Parse(File.ReadAllText(c.ControlPath));
        var root = json.RootElement;
        Assert.Equal(1, root.GetProperty("version").GetInt32());
        Assert.Equal(1, root.GetProperty("seq").GetInt64());
        var res = root.GetProperty("features").GetProperty("resources");
        Assert.True(res.GetProperty("enabled").GetBoolean());
        Assert.Equal(2, res.GetProperty("intervalSec").GetInt32());
        Assert.Equal(500, res.GetProperty("targets").GetProperty("Timber").GetInt32());
        Assert.True(root.GetProperty("features").GetProperty("military").GetProperty("unlimitedSquads").GetBoolean());
    }

    [Fact]
    public void LoadControl_CorruptFile_ReturnsDefault()
    {
        var c = NewClient(out var dir);
        Directory.CreateDirectory(dir);
        File.WriteAllText(c.ControlPath, "{broken");
        var doc = c.LoadControl();
        Assert.Equal(0, doc.Seq);
        Assert.False(doc.Features.Build.Enabled);
    }

    [Fact]
    public void ReadStatus_MissingOrCorrupt_ReturnsNull()
    {
        var c = NewClient(out var dir);
        Assert.Null(c.ReadStatus());
        Directory.CreateDirectory(dir);
        File.WriteAllText(c.StatusPath, "{broken");
        Assert.Null(c.ReadStatus());
    }

    [Fact]
    public void ReadStatus_ParsesLuaOutput()
    {
        var c = NewClient(out var dir);
        Directory.CreateDirectory(dir);
        File.WriteAllText(c.StatusPath,
            """{"version":1,"heartbeat":1800000000,"inGame":true,"appliedSeq":3,"features":{"build":{"active":true}},"resourceIds":["Timber"],"resources":{"Timber":12.0}}""");
        var s = c.ReadStatus()!;
        Assert.True(s.InGame);
        Assert.Equal(3, s.AppliedSeq);
        Assert.True(s.Features!["build"].Active);
        Assert.Null(s.Features["build"].LastError);
        Assert.Equal(12.0, s.Resources!["Timber"]);
    }

    [Fact]
    public void Evaluate_NullStatus_IsDisconnected() =>
        Assert.Equal(BridgeState.Disconnected, NewClient(out _).Evaluate(null, 0));

    [Fact]
    public void Evaluate_StaleHeartbeat_IsDisconnected() =>
        Assert.Equal(BridgeState.Disconnected,
            NewClient(out _).Evaluate(new StatusDocument { Heartbeat = Now.ToUnixTimeSeconds() - 6, InGame = true }, 0));

    [Fact]
    public void Evaluate_States()
    {
        var c = NewClient(out _);
        var hb = Now.ToUnixTimeSeconds() - 1;
        Assert.Equal(BridgeState.MainMenu, c.Evaluate(new StatusDocument { Heartbeat = hb, InGame = false }, 0));
        Assert.Equal(BridgeState.Pending, c.Evaluate(new StatusDocument { Heartbeat = hb, InGame = true, AppliedSeq = 1 }, 2));
        Assert.Equal(BridgeState.Pending, c.Evaluate(new StatusDocument { Heartbeat = hb, InGame = true, AppliedSeq = null }, 1));
        Assert.Equal(BridgeState.Applied, c.Evaluate(new StatusDocument { Heartbeat = hb, InGame = true, AppliedSeq = 2 }, 2));
    }
}
```

`panel/MLToybox.Tests/PanelSettingsTests.cs`

```csharp
using MLToybox.Panel.Core;
using Xunit;

namespace MLToybox.Tests;

public class PanelSettingsTests
{
    [Fact]
    public void SaveThenLoad_RoundTrips()
    {
        var path = Path.Combine(Directory.CreateTempSubdirectory("mltb-set-").FullName, "sub", "panel.json");
        new PanelSettings { GameDir = @"E:\Game" }.Save(path);
        Assert.Equal(@"E:\Game", PanelSettings.Load(path).GameDir);
    }

    [Fact]
    public void Load_MissingOrCorrupt_ReturnsEmpty()
    {
        var dir = Directory.CreateTempSubdirectory("mltb-set-").FullName;
        Assert.Null(PanelSettings.Load(Path.Combine(dir, "none.json")).GameDir);
        var bad = Path.Combine(dir, "bad.json");
        File.WriteAllText(bad, "{x");
        Assert.Null(PanelSettings.Load(bad).GameDir);
    }
}
```

`panel/MLToybox.Tests/ResourceRowsTests.cs`

```csharp
using MLToybox.Panel.Core;
using Xunit;

namespace MLToybox.Tests;

public class ResourceRowsTests
{
    [Fact]
    public void Build_UnionsIdsSortedWithCurrentAndTarget()
    {
        var rows = ResourceRows.Build(
            new[] { "Timber", "Stone" },
            new Dictionary<string, double> { ["Timber"] = 10 },
            new Dictionary<string, int> { ["Iron"] = 50, ["Timber"] = 500 });
        Assert.Equal(new[] { "Iron", "Stone", "Timber" }, rows.Select(r => r.Id));
        Assert.Equal(new ResourceRow("Iron", null, 50), rows[0]);
        Assert.Equal(new ResourceRow("Stone", null, null), rows[1]);
        Assert.Equal(new ResourceRow("Timber", 10, 500), rows[2]);
    }

    [Fact]
    public void Build_AllNullInputs_ReturnsEmpty() =>
        Assert.Empty(ResourceRows.Build(null, null, new Dictionary<string, int>()));
}
```

- [ ] **Step 2: 실행해서 실패 확인**

Run: `dotnet test E:\MLToybox\panel\MLToybox.sln`
Expected: 컴파일 오류(`MLToybox.Panel.Core` 타입 없음)

- [ ] **Step 3: 구현**

`panel/MLToybox.Panel.Core/BridgeJson.cs`

```csharp
using System.Text.Json;

namespace MLToybox.Panel.Core;

public static class BridgeJson
{
    public static readonly JsonSerializerOptions Options = new(JsonSerializerDefaults.Web) { WriteIndented = true };
}
```

`panel/MLToybox.Panel.Core/ControlDocument.cs`

```csharp
namespace MLToybox.Panel.Core;

public sealed class ControlDocument
{
    public int Version { get; set; } = 1;
    public long Seq { get; set; }
    public FeaturesControl Features { get; set; } = new();
}

public sealed class FeaturesControl
{
    public ResourcesControl Resources { get; set; } = new();
    public BuildControl Build { get; set; } = new();
    public UpgradeControl Upgrade { get; set; } = new();
    public MilitaryControl Military { get; set; } = new();
}

public sealed class ResourcesControl
{
    public bool Enabled { get; set; }
    public int IntervalSec { get; set; } = 2;
    public Dictionary<string, int> Targets { get; set; } = new();
}

public sealed class BuildControl
{
    public bool Enabled { get; set; }
    public bool IgnorePlacement { get; set; } = true;
    public bool InstantBuild { get; set; } = true;
    public bool InstantRepair { get; set; } = true;
}

public sealed class UpgradeControl
{
    public bool Enabled { get; set; }
}

public sealed class MilitaryControl
{
    public bool Enabled { get; set; }
    public bool IgnoreEquipment { get; set; } = true;
    public bool IgnorePopulation { get; set; } = true;
    public bool ZeroUpkeep { get; set; } = true;
    public bool UnlimitedSquads { get; set; } = true;
}
```

`panel/MLToybox.Panel.Core/StatusDocument.cs`

```csharp
namespace MLToybox.Panel.Core;

public sealed class StatusDocument
{
    public int Version { get; set; }
    public long Heartbeat { get; set; }
    public bool InGame { get; set; }
    public long? AppliedSeq { get; set; }
    public string? BridgeError { get; set; }
    public Dictionary<string, FeatureStatus>? Features { get; set; }
    public List<string>? ResourceIds { get; set; }
    public Dictionary<string, double>? Resources { get; set; }
}

public sealed class FeatureStatus
{
    public bool Active { get; set; }
    public string? LastError { get; set; }
}
```

`panel/MLToybox.Panel.Core/GameLocator.cs`

```csharp
using System.Text.RegularExpressions;

namespace MLToybox.Panel.Core;

public static class GameLocator
{
    public const string AppId = "1363080";
    public const string InstallDirName = "Manor Lords";

    private static readonly Regex LibraryEntry = new(
        "\"path\"\\s*\"(?<path>[^\"]+)\"(?<body>.*?)(?=\"path\"|\\z)", RegexOptions.Singleline);
    private static readonly Regex AppKey = new($"\"{AppId}\"\\s*\"");

    public static string? FindLibraryWithApp(string vdfText)
    {
        foreach (Match m in LibraryEntry.Matches(vdfText))
        {
            if (AppKey.IsMatch(m.Groups["body"].Value))
                return m.Groups["path"].Value.Replace(@"\\", @"\");
        }
        return null;
    }

    public static string GameDirFromLibrary(string library) =>
        Path.Combine(library, "steamapps", "common", InstallDirName);

    public static string BridgeDir(string gameDir) =>
        Path.Combine(gameDir, "ManorLords", "Binaries", "Win64", "ue4ss", "Mods", "MLToybox", "bridge");

    public static bool LooksLikeGameDir(string dir) =>
        File.Exists(Path.Combine(dir, "ManorLords", "Binaries", "Win64", "ManorLords-Win64-Shipping.exe"));

    public static string? Detect(string? steamPath)
    {
        if (string.IsNullOrEmpty(steamPath)) return null;
        var vdf = Path.Combine(steamPath, "steamapps", "libraryfolders.vdf");
        if (!File.Exists(vdf)) return null;
        var library = FindLibraryWithApp(File.ReadAllText(vdf));
        if (library is null) return null;
        var dir = GameDirFromLibrary(library);
        return LooksLikeGameDir(dir) ? dir : null;
    }

    public static string? SteamPathFromRegistry() =>
        Microsoft.Win32.Registry.GetValue(@"HKEY_CURRENT_USER\Software\Valve\Steam", "SteamPath", null) as string;
}
```

`panel/MLToybox.Panel.Core/BridgeClient.cs`

```csharp
using System.Text.Json;

namespace MLToybox.Panel.Core;

public enum BridgeState { Disconnected, MainMenu, Pending, Applied }

public sealed class BridgeClient
{
    public static readonly TimeSpan HeartbeatTimeout = TimeSpan.FromSeconds(5);
    private readonly Func<DateTimeOffset> _now;

    public BridgeClient(string dir, Func<DateTimeOffset>? now = null)
    {
        Dir = dir;
        _now = now ?? (() => DateTimeOffset.UtcNow);
    }

    public string Dir { get; }
    public string ControlPath => Path.Combine(Dir, "control.json");
    public string StatusPath => Path.Combine(Dir, "status.json");

    public ControlDocument LoadControl()
    {
        try
        {
            if (File.Exists(ControlPath))
                return JsonSerializer.Deserialize<ControlDocument>(File.ReadAllText(ControlPath), BridgeJson.Options) ?? new();
        }
        catch (JsonException) { }
        catch (IOException) { }
        return new ControlDocument();
    }

    public long SaveControl(ControlDocument doc)
    {
        Directory.CreateDirectory(Dir);
        doc.Version = 1;
        doc.Seq = Math.Max(LoadControl().Seq, doc.Seq) + 1;
        var tmp = ControlPath + ".tmp";
        File.WriteAllText(tmp, JsonSerializer.Serialize(doc, BridgeJson.Options));
        for (var attempt = 1; ; attempt++)
        {
            try
            {
                File.Move(tmp, ControlPath, overwrite: true);
                return doc.Seq;
            }
            catch (IOException) when (attempt < 5)
            {
                Thread.Sleep(50);
            }
        }
    }

    public StatusDocument? ReadStatus()
    {
        try
        {
            using var fs = new FileStream(StatusPath, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete);
            return JsonSerializer.Deserialize<StatusDocument>(fs, BridgeJson.Options);
        }
        catch (IOException) { return null; }
        catch (UnauthorizedAccessException) { return null; }
        catch (JsonException) { return null; }
    }

    public BridgeState Evaluate(StatusDocument? status, long lastSentSeq)
    {
        if (status is null) return BridgeState.Disconnected;
        var age = _now() - DateTimeOffset.FromUnixTimeSeconds(status.Heartbeat);
        if (age > HeartbeatTimeout) return BridgeState.Disconnected;
        if (!status.InGame) return BridgeState.MainMenu;
        return (status.AppliedSeq ?? -1) >= lastSentSeq ? BridgeState.Applied : BridgeState.Pending;
    }
}
```

`panel/MLToybox.Panel.Core/PanelSettings.cs`

```csharp
using System.Text.Json;

namespace MLToybox.Panel.Core;

public sealed class PanelSettings
{
    public string? GameDir { get; set; }

    public static string DefaultPath =>
        Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "MLToybox", "panel.json");

    public static PanelSettings Load(string path)
    {
        try
        {
            if (File.Exists(path))
                return JsonSerializer.Deserialize<PanelSettings>(File.ReadAllText(path), BridgeJson.Options) ?? new();
        }
        catch (JsonException) { }
        catch (IOException) { }
        return new PanelSettings();
    }

    public void Save(string path)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
        File.WriteAllText(path, JsonSerializer.Serialize(this, BridgeJson.Options));
    }
}
```

`panel/MLToybox.Panel.Core/ResourceRows.cs`

```csharp
namespace MLToybox.Panel.Core;

public sealed record ResourceRow(string Id, double? Current, int? Target);

public static class ResourceRows
{
    public static List<ResourceRow> Build(
        IEnumerable<string>? ids,
        IReadOnlyDictionary<string, double>? current,
        IReadOnlyDictionary<string, int> targets)
    {
        var all = new SortedSet<string>(StringComparer.Ordinal);
        if (ids is not null) all.UnionWith(ids);
        if (current is not null) all.UnionWith(current.Keys);
        all.UnionWith(targets.Keys);
        return all.Select(id => new ResourceRow(
            id,
            current is not null && current.TryGetValue(id, out var c) ? c : null,
            targets.TryGetValue(id, out var t) ? t : null)).ToList();
    }
}
```

- [ ] **Step 4: 테스트 통과 확인**

Run: `dotnet test E:\MLToybox\panel\MLToybox.sln`
Expected: C# 테스트 전부와 Lua spec 전부 PASS

- [ ] **Step 5: 커밋**

```powershell
git -C E:\MLToybox add panel
git -C E:\MLToybox commit -m "feat(panel): add core bridge client, protocol models, game locator and settings`n`nCo-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 9: 패널 UI (WinForms) + 배포 + E2E 브리지 확인

**Files:**
- Create: `panel/MLToybox.Panel/MLToybox.Panel.csproj` (템플릿), `panel/MLToybox.Panel/Program.cs`, `panel/MLToybox.Panel/MainForm.cs`
- Modify: `tools/deploy.ps1` (`-Panel` 처리 추가)

**Interfaces:**
- Consumes: Task 8의 Core 전체
- Produces: `dist\panel\MLToybox.Panel.exe`

- [ ] **Step 1: 프로젝트 생성**

```powershell
cd E:\MLToybox\panel
dotnet new winforms -n MLToybox.Panel -f net8.0
dotnet sln add MLToybox.Panel
dotnet add MLToybox.Panel reference MLToybox.Panel.Core
Remove-Item MLToybox.Panel\Form1.cs, MLToybox.Panel\Form1.Designer.cs
```

- [ ] **Step 2: Program.cs 작성** (템플릿 파일 덮어쓰기)

```csharp
namespace MLToybox.Panel;

internal static class Program
{
    [STAThread]
    private static void Main()
    {
        ApplicationConfiguration.Initialize();
        Application.Run(new MainForm());
    }
}
```

- [ ] **Step 3: MainForm.cs 작성**

```csharp
using MLToybox.Panel.Core;

namespace MLToybox.Panel;

public sealed class MainForm : Form
{
    private readonly string _settingsPath = PanelSettings.DefaultPath;
    private readonly PanelSettings _settings;
    private BridgeClient? _bridge;
    private ControlDocument _control = new();
    private long _lastSentSeq;
    private string _resourceIdsKey = "";

    private readonly Label _gameDirLabel = new() { AutoSize = true };
    private readonly Label _stateLabel = new() { AutoSize = true, Font = new Font(SystemFonts.DefaultFont, FontStyle.Bold) };

    private readonly CheckBox _resEnabled = new() { Text = "자원 목표값 유지", AutoSize = true };
    private readonly NumericUpDown _resInterval = new() { Minimum = 1, Maximum = 60, Value = 2, Width = 60 };
    private readonly NumericUpDown _fillValue = new() { Minimum = 0, Maximum = 1_000_000, Value = 500, Width = 90 };
    private readonly DataGridView _resGrid = new()
    {
        Dock = DockStyle.Fill, AllowUserToAddRows = false, AllowUserToDeleteRows = false,
        RowHeadersVisible = false, AutoSizeColumnsMode = DataGridViewAutoSizeColumnsMode.Fill,
    };

    private readonly CheckBox _buildEnabled = new() { Text = "건설 기능 사용", AutoSize = true };
    private readonly CheckBox _ignorePlacement = new() { Text = "배치 제한 무시", AutoSize = true };
    private readonly CheckBox _instantBuild = new() { Text = "즉시 완공", AutoSize = true };
    private readonly CheckBox _instantRepair = new() { Text = "즉시 수리", AutoSize = true };

    private readonly CheckBox _upgradeEnabled = new() { Text = "업그레이드 조건·비용·해금 무시", AutoSize = true };

    private readonly CheckBox _milEnabled = new() { Text = "군사 기능 사용", AutoSize = true };
    private readonly CheckBox _ignoreEquipment = new() { Text = "민병대 장비 요구 무시", AutoSize = true };
    private readonly CheckBox _ignorePopulation = new() { Text = "징집 인구 제한 무시", AutoSize = true };
    private readonly CheckBox _zeroUpkeep = new() { Text = "친위대·용병 유지비 0", AutoSize = true };
    private readonly CheckBox _unlimitedSquads = new() { Text = "부대 수 상한 해제", AutoSize = true };

    private readonly TextBox _statusText = new()
    {
        Multiline = true, ReadOnly = true, Dock = DockStyle.Fill, ScrollBars = ScrollBars.Vertical,
        Font = new Font(FontFamily.GenericMonospace, 9),
    };
    private readonly System.Windows.Forms.Timer _timer = new() { Interval = 1000 };

    public MainForm()
    {
        Text = "MLToybox Panel";
        MinimumSize = new Size(560, 480);
        _settings = PanelSettings.Load(_settingsPath);

        _resGrid.Columns.Add(new DataGridViewTextBoxColumn { Name = "Id", HeaderText = "자원", ReadOnly = true });
        _resGrid.Columns.Add(new DataGridViewTextBoxColumn { Name = "Current", HeaderText = "현재", ReadOnly = true });
        _resGrid.Columns.Add(new DataGridViewTextBoxColumn { Name = "Target", HeaderText = "목표 (빈칸=관리 안 함)" });

        var tabs = new TabControl { Dock = DockStyle.Fill };
        tabs.TabPages.Add(BuildResourcesTab());
        tabs.TabPages.Add(Page("건설", _buildEnabled, _ignorePlacement, _instantBuild, _instantRepair));
        tabs.TabPages.Add(Page("업그레이드", _upgradeEnabled));
        tabs.TabPages.Add(Page("군사", _milEnabled, _ignoreEquipment, _ignorePopulation, _zeroUpkeep, _unlimitedSquads));
        var statusPage = new TabPage("상태");
        statusPage.Controls.Add(_statusText);
        tabs.TabPages.Add(statusPage);

        var changeDir = new Button { Text = "경로 변경", AutoSize = true };
        changeDir.Click += (_, _) => ChooseGameDir();
        var apply = new Button { Text = "적용", AutoSize = true };
        apply.Click += (_, _) => Apply();
        var reload = new Button { Text = "되돌리기", AutoSize = true };
        reload.Click += (_, _) => LoadControlIntoUi();

        var top = new FlowLayoutPanel { Dock = DockStyle.Top, AutoSize = true, Padding = new Padding(6) };
        top.Controls.AddRange(new Control[] { _gameDirLabel, changeDir });
        var bottom = new FlowLayoutPanel { Dock = DockStyle.Bottom, AutoSize = true, Padding = new Padding(6), FlowDirection = FlowDirection.RightToLeft };
        bottom.Controls.AddRange(new Control[] { apply, reload, _stateLabel });

        Controls.Add(tabs);
        Controls.Add(top);
        Controls.Add(bottom);

        InitGameDir();
        _timer.Tick += (_, _) => RefreshStatus();
        _timer.Start();
    }

    private TabPage BuildResourcesTab()
    {
        var fill = new Button { Text = "모두 이 값으로", AutoSize = true };
        fill.Click += (_, _) =>
        {
            foreach (DataGridViewRow row in _resGrid.Rows) row.Cells["Target"].Value = (int)_fillValue.Value;
        };
        var header = new FlowLayoutPanel { Dock = DockStyle.Top, AutoSize = true };
        header.Controls.AddRange(new Control[]
        {
            _resEnabled, new Label { Text = "주기(초)", AutoSize = true, Padding = new Padding(8, 6, 0, 0) }, _resInterval,
            _fillValue, fill,
        });
        var page = new TabPage("자원");
        page.Controls.Add(_resGrid);
        page.Controls.Add(header);
        return page;
    }

    private static TabPage Page(string title, params Control[] controls)
    {
        var flow = new FlowLayoutPanel { Dock = DockStyle.Fill, FlowDirection = FlowDirection.TopDown, Padding = new Padding(10) };
        flow.Controls.AddRange(controls);
        var page = new TabPage(title);
        page.Controls.Add(flow);
        return page;
    }

    private void InitGameDir()
    {
        var dir = _settings.GameDir is { } saved && GameLocator.LooksLikeGameDir(saved)
            ? saved
            : GameLocator.Detect(GameLocator.SteamPathFromRegistry());
        SetGameDir(dir);
    }

    private void ChooseGameDir()
    {
        using var dlg = new FolderBrowserDialog { Description = "Manor Lords 설치 폴더 선택" };
        if (dlg.ShowDialog(this) != DialogResult.OK) return;
        if (!GameLocator.LooksLikeGameDir(dlg.SelectedPath))
        {
            MessageBox.Show(this, "ManorLords-Win64-Shipping.exe 를 찾을 수 없습니다.", "MLToybox");
            return;
        }
        _settings.GameDir = dlg.SelectedPath;
        _settings.Save(_settingsPath);
        SetGameDir(dlg.SelectedPath);
    }

    private void SetGameDir(string? dir)
    {
        _bridge = dir is null ? null : new BridgeClient(GameLocator.BridgeDir(dir));
        _gameDirLabel.Text = dir is null ? "게임 경로: (찾지 못함 — 경로 변경을 누르세요)" : $"게임 경로: {dir}";
        LoadControlIntoUi();
    }

    private void LoadControlIntoUi()
    {
        _control = _bridge?.LoadControl() ?? new ControlDocument();
        _lastSentSeq = _control.Seq;
        var f = _control.Features;
        _resEnabled.Checked = f.Resources.Enabled;
        _resInterval.Value = Math.Clamp(f.Resources.IntervalSec, 1, 60);
        _buildEnabled.Checked = f.Build.Enabled;
        _ignorePlacement.Checked = f.Build.IgnorePlacement;
        _instantBuild.Checked = f.Build.InstantBuild;
        _instantRepair.Checked = f.Build.InstantRepair;
        _upgradeEnabled.Checked = f.Upgrade.Enabled;
        _milEnabled.Checked = f.Military.Enabled;
        _ignoreEquipment.Checked = f.Military.IgnoreEquipment;
        _ignorePopulation.Checked = f.Military.IgnorePopulation;
        _zeroUpkeep.Checked = f.Military.ZeroUpkeep;
        _unlimitedSquads.Checked = f.Military.UnlimitedSquads;
        _resourceIdsKey = "";
        RebuildResourceRows(null, null);
    }

    private void RebuildResourceRows(List<string>? ids, Dictionary<string, double>? current)
    {
        _resGrid.Rows.Clear();
        foreach (var row in ResourceRows.Build(ids, current, _control.Features.Resources.Targets))
            _resGrid.Rows.Add(row.Id, row.Current?.ToString("0"), row.Target);
    }

    private void Apply()
    {
        if (_bridge is null)
        {
            MessageBox.Show(this, "게임 경로를 먼저 지정하세요.", "MLToybox");
            return;
        }
        var f = _control.Features;
        f.Resources.Enabled = _resEnabled.Checked;
        f.Resources.IntervalSec = (int)_resInterval.Value;
        f.Resources.Targets = ReadTargets();
        f.Build.Enabled = _buildEnabled.Checked;
        f.Build.IgnorePlacement = _ignorePlacement.Checked;
        f.Build.InstantBuild = _instantBuild.Checked;
        f.Build.InstantRepair = _instantRepair.Checked;
        f.Upgrade.Enabled = _upgradeEnabled.Checked;
        f.Military.Enabled = _milEnabled.Checked;
        f.Military.IgnoreEquipment = _ignoreEquipment.Checked;
        f.Military.IgnorePopulation = _ignorePopulation.Checked;
        f.Military.ZeroUpkeep = _zeroUpkeep.Checked;
        f.Military.UnlimitedSquads = _unlimitedSquads.Checked;
        try
        {
            _lastSentSeq = _bridge.SaveControl(_control);
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException)
        {
            MessageBox.Show(this, $"control.json 저장 실패: {ex.Message}", "MLToybox");
        }
        RefreshStatus();
    }

    private Dictionary<string, int> ReadTargets()
    {
        var targets = new Dictionary<string, int>();
        foreach (DataGridViewRow row in _resGrid.Rows)
        {
            var id = row.Cells["Id"].Value as string;
            var raw = row.Cells["Target"].Value?.ToString();
            if (id is not null && int.TryParse(raw, out var v) && v >= 0) targets[id] = v;
        }
        return targets;
    }

    private void RefreshStatus()
    {
        var status = _bridge?.ReadStatus();
        var state = _bridge?.Evaluate(status, _lastSentSeq) ?? BridgeState.Disconnected;
        _stateLabel.Text = state switch
        {
            BridgeState.Disconnected => "● 연결 안 됨 (게임 미실행 또는 모드 미로드)",
            BridgeState.MainMenu => "● 메인 메뉴",
            BridgeState.Pending => "● 적용 대기 중",
            _ => "● 적용됨",
        };
        _stateLabel.ForeColor = state switch
        {
            BridgeState.Applied => Color.SeaGreen,
            BridgeState.Pending => Color.DarkOrange,
            BridgeState.MainMenu => Color.SteelBlue,
            _ => Color.Firebrick,
        };
        if (status is null)
        {
            _statusText.Text = "status.json 없음";
            return;
        }

        var key = string.Join("|", status.ResourceIds ?? new List<string>());
        if (key != _resourceIdsKey && !_resGrid.IsCurrentCellInEditMode)
        {
            _resourceIdsKey = key;
            var edited = ReadTargets();   // 재구성 전에 사용자가 입력 중이던 목표값을 보존
            if (edited.Count > 0) _control.Features.Resources.Targets = edited;
            RebuildResourceRows(status.ResourceIds, status.Resources);
        }
        else if (status.Resources is not null)
        {
            foreach (DataGridViewRow row in _resGrid.Rows)
                if (row.Cells["Id"].Value is string id && status.Resources.TryGetValue(id, out var cur))
                    row.Cells["Current"].Value = cur.ToString("0");
        }

        var lines = new List<string>
        {
            $"상태: {state}",
            $"heartbeat: {DateTimeOffset.FromUnixTimeSeconds(status.Heartbeat).ToLocalTime():HH:mm:ss}",
            $"inGame: {status.InGame}",
            $"appliedSeq: {status.AppliedSeq?.ToString() ?? "-"} / sent: {_lastSentSeq}",
            $"bridgeError: {status.BridgeError ?? "-"}",
            "",
            "[기능]",
        };
        if (status.Features is null) lines.Add("(모드에 등록된 기능 없음)");
        else
            foreach (var (name, fs) in status.Features.OrderBy(p => p.Key))
                lines.Add($"{name,-10} active={fs.Active,-5} error={fs.LastError ?? "-"}");
        _statusText.Text = string.Join(Environment.NewLine, lines);
    }
}
```

- [ ] **Step 4: 빌드 확인**

Run: `dotnet build E:\MLToybox\panel\MLToybox.sln -warnaserror:nullable`
Expected: `Build succeeded`, 오류 0

- [ ] **Step 5: deploy.ps1에 -Panel 추가** — `if ($Remove) {` 블록 바로 위에 삽입

```powershell
if ($Panel) {
    $out = Join-Path $repo 'dist\panel'
    dotnet publish (Join-Path $repo 'panel\MLToybox.Panel\MLToybox.Panel.csproj') -c Release -o $out --nologo -v q
    if ($LASTEXITCODE -ne 0) { throw 'panel publish failed' }
    Write-Host "Panel published -> $out\MLToybox.Panel.exe"
    return
}
```

Run: `pwsh -NoProfile -File E:\MLToybox\tools\deploy.ps1 -Mod MLToybox -Panel`
Expected: `Panel published -> E:\MLToybox\dist\panel\MLToybox.Panel.exe`

- [ ] **Step 6: 게임 없이 스모크**

`dist\panel\MLToybox.Panel.exe`를 실행한다. 게임 경로가 자동으로 표시되고 상태가 "연결 안 됨"인지 스크린샷(computer-use)으로 확인한다. "적용"을 누른 뒤 control.json의 seq를 확인한다.

```powershell
Get-Content 'E:\SteamLibrary\steamapps\common\Manor Lords\ManorLords\Binaries\Win64\ue4ss\Mods\MLToybox\bridge\control.json'
```
Expected: `"seq": 1` 이상, `"features"` 아래 네 기능 모두 존재

- [ ] **Step 7: 게임과 E2E**

`backup-saves.ps1`를 실행하고 게임을 켠 뒤 세이브를 로드한다(Task 7 Step 5·6 절차). 패널에서 "적용"을 누른다.
Expected: 5초 안에 "● 적용됨" 표시. `status.json`의 `appliedSeq`가 control의 seq와 같다. 게임을 끄면 5초 뒤 "● 연결 안 됨"으로 바뀐다(Review Focus 1). "● 메인 메뉴" 표시는 `gameStateClassPattern`이 설정되는 Task 11 Step 3에서 확인한다. 그 전에는 메뉴도 인게임으로 간주한다.

- [ ] **Step 8: 테스트 회귀 확인 후 커밋**

Run: `dotnet test E:\MLToybox\panel\MLToybox.sln` 와 `pwsh -NoProfile -File E:\MLToybox\tools\tests\Tools.Tests.ps1`
Expected: 모두 PASS

```powershell
git -C E:\MLToybox add panel tools
git -C E:\MLToybox commit -m "feat(panel): add WinForms control panel and publish step`n`nCo-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 10: 덤프 모드 + dump.ps1 + 덤프 수행

**Files:**
- Create: `mod/MLToyboxDump/Scripts/main.lua`
- Create: `tools/dump.ps1`

**Interfaces:**
- Consumes: `deploy.ps1 -Mod MLToyboxDump`
- Produces:
  - 덤프 모드 프로토콜: `<Mods>\MLToyboxDump\dump_request.txt`가 생기면 2초 안에 게임 스레드에서 `DumpAllObjects()`, `GenerateSDK()`, `GenerateLuaTypes()`를 순서대로 실행한다. 끝나면 요청 파일을 지우고 `dump_done.txt`(각 단계 결과)를 쓴다. GameState·맵 로드 이력은 `events.txt`에 추가 기록한다.
  - `dump.ps1 [-GameDir] [-TimeoutSec 600]`: 요청 → 완료 대기 → `analysis\dumps\<timestamp>\`로 수집(`UE4SS_ObjectDump.txt`, `CXXHeaderDump\`, `Mods\shared\types\`, `UE4SS.log`, `events.txt`, `dump_done.txt`)

- [ ] **Step 1: 덤프 모드 작성** — `mod/MLToyboxDump/Scripts/main.lua`

```lua
local source = debug.getinfo(1, "S").source:gsub("^@", "")
local scriptsDir = source:match("^(.*)[\\/][^\\/]+$")
local modDir = scriptsDir:match("^(.*)[\\/][^\\/]+$")
local requestPath = modDir .. "\\dump_request.txt"
local donePath = modDir .. "\\dump_done.txt"
local eventsPath = modDir .. "\\events.txt"
local busy = false

local function log(msg) print("[MLToyboxDump] " .. msg .. "\n") end

local function append(path, line)
  local f = io.open(path, "ab")
  if f then f:write(os.date("!%Y-%m-%dT%H:%M:%SZ") .. " " .. line .. "\n"); f:close() end
end

local function exists(path)
  local f = io.open(path, "rb")
  if f then f:close() return true end
  return false
end

RegisterLoadMapPreHook(function() append(eventsPath, "LoadMapPre") end)
RegisterInitGameStatePostHook(function(context)
  local ok, name = pcall(function() return context:get():GetFullName() end)
  append(eventsPath, "InitGameState " .. (ok and name or ("<error " .. tostring(name) .. ">")))
end)

local function runDump()
  local results = {}
  for _, step in ipairs({
    { "DumpAllObjects", DumpAllObjects },
    { "GenerateSDK", GenerateSDK },
    { "GenerateLuaTypes", GenerateLuaTypes },
  }) do
    local started = os.time()
    local ok, err = pcall(step[2])
    results[#results + 1] = string.format("%s ok=%s sec=%d err=%s", step[1], tostring(ok), os.time() - started, tostring(err))
    log(results[#results])
  end
  os.remove(requestPath)
  local f = io.open(donePath, "wb")
  if f then f:write(table.concat(results, "\n") .. "\n"); f:close() end
end

LoopAsync(2000, function()
  if not busy and exists(requestPath) then
    busy = true
    os.remove(donePath)
    ExecuteInGameThread(function()
      local ok, err = pcall(runDump)
      if not ok then log("dump failed: " .. tostring(err)) end
      busy = false
    end)
  end
  return false
end)

log("loaded, waiting for " .. requestPath)
```

- [ ] **Step 2: dump.ps1 작성**

```powershell
param([string]$GameDir, [int]$TimeoutSec = 600)
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\common.ps1"
$repo = Split-Path $PSScriptRoot -Parent
$win64 = Get-MLWin64Dir $GameDir
$ue4ss = Join-Path $win64 'ue4ss'
$modDir = Join-Path $ue4ss 'Mods\MLToyboxDump'
if (-not (Test-Path $modDir)) { throw 'MLToyboxDump not deployed. Run: deploy.ps1 -Mod MLToyboxDump' }

$done = Join-Path $modDir 'dump_done.txt'
Remove-Item $done -ErrorAction SilentlyContinue
Set-Content (Join-Path $modDir 'dump_request.txt') (Get-Date -Format o)
Write-Host 'Dump requested; waiting (game must be running with a save loaded)...'
$deadline = (Get-Date).AddSeconds($TimeoutSec)
while (-not (Test-Path $done)) {
    if ((Get-Date) -gt $deadline) { throw "Timed out after $TimeoutSec s waiting for $done" }
    Start-Sleep -Seconds 3
}

$dest = Join-Path $repo "analysis\dumps\$(Get-Date -Format 'yyyyMMdd-HHmmss')"
New-Item -ItemType Directory -Force $dest | Out-Null
$candidates = @(
    'UE4SS_ObjectDump.txt', 'CXXHeaderDump', 'UE4SS.log',
    'Mods\shared\types', 'Mods\MLToyboxDump\events.txt', 'Mods\MLToyboxDump\dump_done.txt'
)
foreach ($rel in $candidates) {
    $found = @((Join-Path $ue4ss $rel), (Join-Path $win64 $rel)) | Where-Object { Test-Path $_ } | Select-Object -First 1
    if ($found) {
        Copy-Item $found -Destination $dest -Recurse -Force
        Write-Host "collected $rel"
    } else {
        Write-Warning "missing $rel"
    }
}
Get-Content $done
return $dest
```

- [ ] **Step 3: 배포와 덤프 수행**

```powershell
pwsh -NoProfile -File E:\MLToybox\tools\backup-saves.ps1
pwsh -NoProfile -File E:\MLToybox\tools\deploy.ps1 -Mod MLToyboxDump
Start-Process 'steam://rungameid/1363080'
```
세이브를 로드한다(Task 7 Step 6과 같은 방식). 인게임 화면이 뜨면:

```powershell
pwsh -NoProfile -File E:\MLToybox\tools\dump.ps1
```
Expected: `collected UE4SS_ObjectDump.txt`, `collected CXXHeaderDump`, `collected types`, 그리고 `dump_done.txt`의 세 단계 모두 `ok=true`. `missing` 경고가 있으면 해당 파일을 `Get-ChildItem -Recurse $win64 -Filter <이름>`으로 찾는다. 찾으면 `dump.ps1`의 `$candidates`를 고치고 다시 수집한다.

- [ ] **Step 4: 덤프 모드 해제, 게임 종료, 커밋**

```powershell
pwsh -NoProfile -File E:\MLToybox\tools\deploy.ps1 -Mod MLToyboxDump -Remove
git -C E:\MLToybox add mod tools
git -C E:\MLToybox commit -m "feat(tools): add file-triggered dump mod and dump collection script`n`nCo-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 11: 분석 — findings.md + README + develop 머지

**Files:**
- Create: `analysis/findings.md`
- Create: `README.md`
- Modify: `mod/MLToybox/Scripts/config.lua` (`gameStateClassPattern` 설정)

**Interfaces:**
- Consumes: `analysis/dumps/<latest>/` (Task 10), Task 7 Step 6의 GameState 로그
- Produces: Plan 2 작성의 유일한 입력인 `findings.md`. 형식은 아래와 같다.

- [ ] **Step 1: 덤프 탐색**

`analysis\dumps\<latest>\`에서 기능별 키워드로 검색한다. 대량 검색은 Explore 서브에이전트에 맡겨도 된다. 이때 지시문에 "읽기 전용, `E:\MLToybox\analysis\dumps\<latest>` 절대경로만 사용, 결과는 파일:줄 인용으로 보고"를 명시한다.

| 기능 | 검색 키워드 (대소문자 무시) |
|---|---|
| ① 자원 | `Resource`, `Storage`, `Stockpile`, `Inventory`, `Goods`, `Wealth`, `Treasury`, `Money`, `Amount` |
| ② 배치 | `Placement`, `CanPlace`, `CanBuild`, `Valid`, `Ghost`, `Preview`, `Blueprint` |
| ② 완공·수리 | `Construction`, `Progress`, `Complete`, `Finish`, `Durability`, `Damage`, `Repair`, `Health` |
| ③ 업그레이드 | `Upgrade`, `Requirement`, `Level`, `Burgage`, `Development`, `Perk`, `Unlock` |
| ④ 군사 | `Militia`, `Squad`, `Unit`, `Retinue`, `Mercenar`, `Upkeep`, `Equipment`, `Conscript`, `Recruit`, `MaxSquad` |

```powershell
$d = (Get-ChildItem E:\MLToybox\analysis\dumps | Sort-Object Name | Select-Object -Last 1).FullName
Select-String -Path "$d\CXXHeaderDump\*.hpp" -Pattern 'Upkeep' | Select-Object -First 40
```

- [ ] **Step 2: findings.md 작성** — 아래 골격을 모두 채운다. 빈칸이 남으면 안 된다. 찾지 못한 항목은 "미발견 + 시도한 검색어"를 적는다.

```markdown
# MLToybox 분석 결과 (findings)

- 덤프: analysis/dumps/<timestamp>, UE4SS 버전: <UE4SS.log에서>, 게임 buildid: 24905706
- GameState: 메뉴=<FullName>, 인게임=<FullName> → config.gameStateClassPattern = "<패턴>"

## ① 자원 목표값 유지
- 자원 식별 방식: <enum 이름/값 목록 또는 DataTable 행 이름 목록>
- 수량 저장 위치: <클래스.프로퍼티 (타입)> — 근거: <파일:줄>
- 지역 재화 / 영주 금고: <클래스.프로퍼티> — 근거
- 인스턴스 찾는 법: <FindAllOf("<클래스>") 등>
- 판정: Lua 가능 / 불가 (+이유)

## ② 건물 — 배치 제한 / 즉시 완공 / 즉시 수리
(항목마다 대상 UFunction·프로퍼티, 근거, 판정)

## ③ 업그레이드 — 조건 / 비용 / 해금
(동일 형식)

## ④ 군사 — 장비 / 인구 / 유지비 / 부대 수 상한
(동일 형식)

## C++ 승격 후보
<Lua로 불가능하다고 판정한 항목과 근거. 없으면 "없음">
```

- [ ] **Step 3: gameStateClassPattern 반영과 확인**

findings에 적은 패턴을 `config.lua`에 넣는다(예: `gameStateClassPattern = "<인게임 GameState 클래스 이름 일부>"`). 배포한 뒤 게임에서 메뉴는 `inGame=false`, 세이브 로드 후는 `inGame=true`로 로그가 찍히는지 확인한다.

```powershell
pwsh -NoProfile -File E:\MLToybox\tools\deploy.ps1 -Mod MLToybox
```

- [ ] **Step 4: README.md 작성**

```markdown
# MLToybox

Manor Lords(Steam)용 UE4SS Lua 치트 모드와 외부 제어 패널.

## 구성
- `mod/MLToybox` — UE4SS Lua 모드 (게임 폴더 `ue4ss/Mods/MLToybox`로 배포)
- `panel/` — .NET 8 WinForms 제어 패널 (`dist/panel/MLToybox.Panel.exe`)
- `tools/` — 배포·백업·덤프 스크립트
- `analysis/findings.md` — 게임 내부 대상 매핑

## 설치
    pwsh tools/backup-saves.ps1
    pwsh tools/deploy.ps1 -Mod MLToybox
    pwsh tools/deploy.ps1 -Mod MLToybox -Panel
게임을 실행한 뒤 `dist/panel/MLToybox.Panel.exe`를 켜고, 탭에서 기능을 설정하고 "적용"을 누른다.

## 제거
    pwsh tools/deploy.ps1 -Mod MLToybox -Remove

## 테스트
    dotnet test panel/MLToybox.sln
    pwsh tools/tests/Tools.Tests.ps1

## 게임 업데이트 후 복구 절차
1. `pwsh tools/deploy.ps1 -Mod MLToyboxDump`, 게임 실행 후 세이브 로드
2. `pwsh tools/dump.ps1` → `analysis/dumps/<시각>/` 생성
3. 패널 [상태] 탭에서 "비활성(대상 없음)"인 기능을 확인하고, `findings.md`의 해당 항목을 새 덤프로 다시 확인해 식별자를 수정
4. `pwsh tools/deploy.ps1 -Mod MLToyboxDump -Remove` 후 `pwsh tools/deploy.ps1 -Mod MLToybox`

## 주의
모드가 기록한 값은 세이브에 저장된다. 테스트 전에 항상 `backup-saves.ps1`을 실행한다.
```

- [ ] **Step 5: 전체 테스트 후 커밋과 develop 머지**

Run: `dotnet test E:\MLToybox\panel\MLToybox.sln` 와 `pwsh -NoProfile -File E:\MLToybox\tools\tests\Tools.Tests.ps1`
Expected: 모두 PASS

```powershell
git -C E:\MLToybox add analysis/findings.md README.md mod
git -C E:\MLToybox commit -m "docs: add analysis findings, README and gameplay GameState detection`n`nCo-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
git -C E:\MLToybox switch develop
git -C E:\MLToybox merge --no-ff feat/plan1-foundation -m "Merge branch 'feat/plan1-foundation' into develop`n`nCo-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
git -C E:\MLToybox log --oneline --graph -15
```

- [ ] **Step 6: 사용자에게 보고하고 정지**

`findings.md` 요약을 보고한다: 기능별 Lua 가능·불가 판정과, C++ 승격 후보가 있으면 그 결정 요청. 그 다음 Plan 2(기능 구현) 작성으로 넘어갈지 묻는다.
