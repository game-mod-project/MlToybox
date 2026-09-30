# MLToybox

Manor Lords UE4SS Lua 모드 + .NET 8 제어 패널. 설계: `docs/superpowers/specs/2026-09-28-mltoybox-mod-design.md`

## Git 브랜치 전략
- `main`은 보호 브랜치: 직접 커밋·푸시 금지.
- `develop`이 통합 브랜치. 작업은 `develop`에서 `feat/*`·`fix/*`·`chore/*`·`docs/*`로 분기한다.
- 작업 브랜치 → `develop`, 릴리스 시 `develop` → `main`은 `git merge --no-ff`로 병합한다(원격: https://github.com/game-mod-project/MlToybox — 공개 레포. 병합 후 `develop`·`main`을 푸시).
- 머지 전 테스트 통과를 확인한다.

## 작업 규칙 (이 레포)
- 원인은 실측(게임 값·로그·덤프·상태 파일)으로 확인한 뒤 보고한다. 추정을 결론처럼 쓰지 않는다.
- 게임 확인은 가능한 한 직접 한다: Lab 모드(`tools/lab.ps1`)로 값 읽기, `bridge/control.json`에 명령을 써서 결과를 `status.json`으로 확인.
- 패널을 고치면 `dotnet test`와 별도로 `dotnet build panel/MLToybox.sln`을 실행한다(테스트가 WinForms 컴파일 오류를 잡지 못한 적이 있음).
- Lua 모드 변경은 게임 재시작 후 적용, 패널은 실행 중이면 배포 불가(파일 잠김), 네이티브 DLL은 게임 실행 중 잠김.