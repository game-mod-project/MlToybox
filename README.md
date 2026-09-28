# MLToybox

Manor Lords(Steam)용 UE4SS Lua 치트 모드와 외부 제어 패널.

## 구성
- `mod/MLToybox` — UE4SS Lua 모드 (게임 폴더 `ue4ss/Mods/MLToybox`로 배포)
- `mod/MLToyboxDump` — 분석용 덤프 모드 (필요할 때만 배포)
- `panel/` — .NET 8 WinForms 제어 패널 (`dist/panel/MLToybox.Panel.exe`)
- `tools/` — 배포·백업·덤프 스크립트
- `analysis/findings.md` — 게임 내부 대상 매핑

전제: 게임 폴더에 UE4SS v3.0.1(`dwmapi.dll` 프록시)이 설치돼 있어야 한다.

## 설치
    pwsh tools/backup-saves.ps1
    pwsh tools/deploy.ps1 -Mod MLToybox
    pwsh tools/deploy.ps1 -Mod MLToybox -Panel
게임을 실행한 뒤 `dist/panel/MLToybox.Panel.exe`를 켜고, 탭에서 기능을 설정하고 "적용"을 누른다.
모드는 게임 시작 시에 로드되므로, 배포는 게임을 끈 상태에서 하거나 배포 후 게임을 재시작한다.

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
