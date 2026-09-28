# MLToybox

Manor Lords UE4SS Lua 모드 + .NET 8 제어 패널. 설계: `docs/superpowers/specs/2026-09-28-mltoybox-mod-design.md`

## Git 브랜치 전략
- `main`은 보호 브랜치: 직접 커밋·푸시 금지.
- `develop`이 통합 브랜치. 작업은 `develop`에서 `feat/*`·`fix/*`·`chore/*`·`docs/*`로 분기한다.
- 원격 레포가 없으므로 PR 대신 로컬 `git merge --no-ff`로 작업 브랜치 → `develop`, 릴리스 시 `develop` → `main`.
- 머지 전 테스트 통과를 확인한다.
