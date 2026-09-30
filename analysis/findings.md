# MLToybox 분석 결과 (findings)

- 덤프: `analysis/dumps/20260928-143053`(메인 메뉴: 네이티브 클래스 정의 전체와 주요 DataTable), `analysis/dumps/20260928-144831`(인게임, 맵 Winding_River: 인스턴스 확인)
- UE4SS: v3.0.1 Beta #0 (Git SHA f6d5f942), 내장 Lua 5.4 / 게임 buildid: 24905706
- 근거 표기: `H:줄` = `CXXHeaderDump/ManorLords.hpp`의 줄 번호, `OD` = `UE4SS_ObjectDump.txt`
- 판정 기호: ✅ Lua 가능(UFunction 호출 또는 리플렉션 프로퍼티 쓰기) / 🧪 Lua 가능성 높음, Plan 2 첫 스파이크에서 실측 필요 / ❌ Lua 불가 추정(C++ 승격 후보)

## 공통 사항

- `InitGameState` 훅의 context는 **GameMode**다. 메뉴는 `MenuGameMode_ML_C /Game/NotStronghold/Maps/MainMenu.MainMenu:...`(events.txt). 인게임은 `RTSGame_C /Game/NotStronghold/Maps/NewMapSets/Winding_River.Winding_River:PersistentLevel.RTSGame_C_...`(2차 덤프 events.txt, 14:47:53).
  - 조치: `config.lua`의 `menuGameModePattern = "MenuGameMode_ML_C"`로 메뉴를 제외한다. 다른 맵도 `RTSGame_C`일 것으로 보이지만 확인한 맵이 하나뿐이라 양성 패턴은 쓰지 않는다.
- 게임 로직 중심 객체: `ARTSMultiEngineCPP`(건물·유닛·분대·용병 관리), `ARegion`(지역 재고·재화·승인도), `APawnCPP`(플레이어 영주). 인게임 인스턴스(2차 덤프로 확인): `MyRTSMultiEngineCPP_BP_C` 1개(`PersistentLevel.RTSGameManager`), `BP_Region_C` 7개, `MyPawnCPP_BP3_C` 1개, `MLCheatManager_C`(`MLPlayerController_C_....MLCheatManager_C_...`), 건물 `SMBuildingMaster` 167개, 건물 파츠 `SMBuilding` 6162개. 찾기: `FindFirstOf("MyRTSMultiEngineCPP_BP_C")`, `FindAllOf("BP_Region_C")`, `FindFirstOf("MyPawnCPP_BP3_C")`, `FindFirstOf("MLCheatManager_C")`. 로드 시점에는 공사 중 액터(`ConstructionBP_C`)가 0개였다.
- **개발용 치트 매니저 `UMLCheatManager`가 있다(H:5977~6011).** UE4SS의 `CheatManagerEnablerMod`가 이미 활성화해 두었다(UE4SS.log "Enabled CheatManager"). 모든 메서드가 UFunction이라 Lua에서 호출할 수 있다.
  - `GiveItemByName(FString,int32)`, `GiveItemByID(int32,int32)`, `GiveRandomAmountOfAllItems()`, `GiveFoodVariety()`, `AddItemToBuilding(FString,FString,float)`
  - `ChangeTreasury(int32 InNewTreasury)`(절대값 설정), `MaintainAllBuildings()`, `UnlockPerk(FName,int32)`, `SetSettlementLevel(uint32)`, `AddExpertiseToSelection(int32)`
- **Lua 훅의 한계:** `RegisterHook`은 ProcessEvent를 거치는 호출(BP→UFunction)에만 걸린다. 네이티브 C++끼리의 직접 호출(예: 틱 안의 `canUpgrade`)은 가로챌 수 없다. 그래서 1차 수단은 **데이터 쓰기**(DataTable 행, 설정 CDO, 인스턴스 프로퍼티)와 **UFunction 호출**로 잡는다.
- **주요 DataTable은 메뉴 시점에 이미 로드돼 있다(OD):** `/Game/NotStronghold/Data/DT_Upgrades`, `buildingStats`, `DT_Items`, `DT_UnitTemplates`, `DT_MercenaryCompanies`, `techList`, `/Game/UI/Development/DT_RegionDevelopmentTech`.
  - 🧪 UE4SS 3.0.1 Lua에서 DataTable **행 읽기·쓰기**가 가능한지는 실측이 필요하다(`UDataTableFunctionLibrary`의 `GetDataTableRowNames`/`GetDataTableRowFromName`, 또는 RowMap 직접 접근). **Plan 2 Task 1 스파이크로 가장 먼저 검증한다.** 불가능하면 DataTable을 쓰는 항목은 대체 수단이나 C++ 승격으로 넘어간다.

## ① 자원 목표값 유지

- 자원 식별: `enum class EItemType`(ManorLords_enums.hpp). 예: `Timber=16`, `planks=17`, `RoughStone=27`, `IronOre=14`, `Clay=146`, `Firewood=216`, `WheatBread=172`, `spears=133`, `gambesons=163`, `mail_armor=164`. 표시 이름 DataTable은 `DT_Items`(`UItemSettings.ItemTable`).
- 수량 저장: 건물별 `ASMBuildingMaster.Inventory : TArray<FGood>`(H:4507 클래스, 0x0438). `FGood{Type:int32, amt:int32, belongsTo, belongsToUnit}`(H:567).
- 지역 합계 읽기: `ARegion.getStockOfGood(int32 Type, bool minusReservation, bool bIncludePredictedCraftingOutput) -> int32`, `ARegion.getStock(bool) -> TArray<FGood>` ✅
- 추가: `ARegion.grantResources(TArray<FGood>, bool bAddLogPrompt)` ✅(UFunction), 치트 `AddItemToBuilding` / `GiveItemByID` ✅
- 지역 재화: `ARegion.regionalWealth : int32`(0x0648) ✅ 직접 쓰기, 보조로 `addRegionalWealth(int32)`
- 영주 금고: 필드는 리플렉션에 없다. `UMLCheatManager.ChangeTreasury(int32 InNewTreasury)`로 절대값을 설정한다 ✅. 현재값 읽기 수단은 없으므로 목표값을 매 주기 그대로 설정한다.
- 영향력(Influence): `APawnCPP.influence : int32`(0x0B98) ✅
- 구현안: 주기마다 지역별로 `getStockOfGood(type)`을 읽고 목표값과의 차이만큼 `grantResources`(부족분)로 채운다. 초과분은 건드리지 않는다. 재화와 금고는 직접 설정한다.
- 판정: ✅ (grantResources의 동작은 인게임 확인 필요. 실패하면 `AddItemToBuilding`으로 대체)

## ② 건물

### 배치 제한 무시
- `APawnCPP`에는 배치 관련 상태만 있다: `placeBuilding`(0x0608), `placebuilding_missingTech : TArray<int32>`(0x13B0), `resourcesInCollisionOfPlacebuilding`. 유효성 판정 UFunction(canPlace/isValid 류)은 네이티브·BP 헤더 전체에서 **발견되지 않았다**(검색어: `canPlace`, `placementValid`, `isValidPlace`, `obstruct`, `canBuildHere`, `placeable`).
- `ARTSMultiEngineCPP.isConstructionAllowed : bool`(0x0CBC)은 전역 허용 플래그로 보인다 ✅ 쓰기(효과는 실측 필요).
- `placebuilding_missingTech`를 비우면 **기술 미해금** 제한은 풀릴 가능성이 있다 🧪
- 판정: 지형·겹침 판정은 ❌ **C++ 승격 후보**(네이티브 틱 안에서 판정, UFunction 없음). 기술 제한은 🧪

### 즉시 완공
- `AConstruction.constructionProgress : float`(0x02F0), `AConstruction.updateConstructionLevel()` / `setupConstructionFast()` ✅. 공사 액터 클래스는 `AConstructionBP_C`(ConstructionBP.hpp).
- 건물 측: `ASMBuildingMaster.IsConstructed()`, `getConstructionProgress()`, `constructionGoods : TArray<FGood>`(0x03C0). 완공 콜백은 `APawnCPP.constructionComplete(ASMBuildingMaster*)` ✅(UFunction)
- 건물 스탯 DataTable `buildingStats`(`FStat`): `constructionGoods`, `constructionCost` → 0으로 만들면 자재가 필요 없어진다 🧪(DataTable 쓰기)
- 구현안: `NotifyOnNewObject("/Script/ManorLords.Construction")`로 새 공사 액터를 감지하고 → `constructionProgress = 1.0` → `updateConstructionLevel()`을 호출한다. 네이티브가 완공으로 처리하는지 실측한다.
- 판정: 🧪

### 즉시 수리
- `UMLCheatManager.MaintainAllBuildings()` ✅(유지보수 전체 충족), `FMaintenanceTrackerData.DurabilityLeft`
- 화재: `AConstruction.onFire : bool`(0x0312), `ARegion.bAnyBuildingOnFire`
- 구현안: 주기마다 `MaintainAllBuildings()`를 호출한다. 붕괴·화재 복구는 실측 후 결정한다.
- 판정: ✅(유지보수) / 🧪(화재·붕괴)

## ③ 업그레이드 — 조건 / 비용 / 해금

- 업그레이드 정의: DataTable `DT_Upgrades`(`UBuildingSettings.UpgradeDataTable`), 행 구조 `FUpgrade`(H:2688)
  - 비용: `cost : TArray<FGood>`, `regionalWealth : int32`, `treasury : int32`
  - 조건: `requiresBuilding : TArray<int32>`, `requiredPerks : TArray<FName>`(해금), `minimumSettlementLevel`, `minimumProsperity : uint8`, `minimumHouseLv : uint8`, `lockedInOutposts : bool`
- 주거 레벨업 충족 조건: `UResidentialRequirementSettings.UpgradeRequirementsPerLevel : TArray<FRequirementsPerLevel>`(설정 CDO) ✅ 쓰기 후보(빈 배열 또는 요구 0)
- 판정 함수: `ASMBuildingMaster.canUpgrade(int32 ID, TArray<FName>& reasons) -> bool`, `GetUpgradeResourceCost`, `GetRegionalWealthCostForUpgrade`, `AllResidentialRequirementsSatisfied()`. 모두 UFunction이지만 네이티브 내부 호출은 훅으로 가로챌 수 없다.
- 개발(발전) 해금: `DT_RegionDevelopmentTech`, `techList`, `UMLCheatManager.UnlockPerk(FName,int32)` ✅
- 구현안:
  - `DT_Upgrades` 모든 행을 cost=[], regionalWealth=0, treasury=0, requires*=[], minimum*=0으로 만든다.
  - `UpgradeRequirementsPerLevel`의 요구를 비운다.
  - 필요한 perk는 `UnlockPerk`로 해금한다.
  - 원본 값은 메모리에 보관해 기능을 끌 때 되돌린다(DataTable이 세이브에 저장되지 않으므로 안전).
- 판정: 🧪(DataTable 쓰기 스파이크에 의존), 설정 CDO 쓰기는 ✅

## ④ 군사

- 민병대 부대 수 상한: `APawnCPP.maxNumOfMilitiaToSpawn : int32`(0x0BC0) ✅ 쓰기, 판정 `APawnCPP.canAddNewMilitiaSquad()`
- 장비 요구: `DT_UnitTemplates` 행 `FUnitTemplate.requiredEquipment : TArray<FGood>`(H:2656), 기본 장비 `ARTSMultiEngineCPP.getDefaultMilitiaEquipmentForUnitTemplate(FName)` → `requiredEquipment`를 비운다 🧪(DataTable)
- 인구(징집 가능 인원) 제한:
  - `FUnitTemplate.minHouseLv`, `minMeleeTraining`, `minArcheryTraining` → 0으로 만든다 🧪
  - 가용 인원 계산은 `ARegion.getAvailableRecruits(...)`(네이티브)라서 주민 수 자체를 넘는 징집은 ❌ C++ 후보
  - 부대 정원: `FSquad.maxSize`(ARTSMultiEngineCPP.squads[], 0x05B0), `FUnitTemplate.maxSize`, `APawnCPP.targetSquadSize : uint8`
- 친위대·용병 유지비:
  - 분대 `FSquad.payDay : int32`(급여일), 용병 계약 `FMercenaryCompany.cost : int32`(DT_MercenaryCompanies, `ARTSMultiEngineCPP.hiredMercs : TMap<int32,FMercenaryCompany>`) ✅(인스턴스 쓰기)
  - 친위대 고용비 `ASMBuildingMaster.GetRetinueHireTreasuryCost()`, 모집비 `APawnCPP.recruitCost : float`(0x0B40) ✅
  - 구현안: 주기마다 `hiredMercs`의 cost=0, `recruitCost=0`. 급여 차감이 네이티브 월 정산이면 ①의 금고 유지로 상쇄한다(`ChangeTreasury`).
- 판정: 부대 수 상한 ✅ / 장비·훈련 요구 🧪 / 유지비 ✅(보정 방식) / 주민 수를 넘는 징집 ❌

## C++ 승격 후보

1. **배치 제한 무시(지형·겹침)**: 판정 UFunction이 없다(검색어 목록은 ②에 기재). 네이티브 시그니처 스캔과 후킹이 필요하다.
2. **주민 수를 넘는 민병대 징집**: `getAvailableRecruits`의 네이티브 계산을 우회해야 한다.
3. **즉시 완공**: 공사 진행도가 리플렉션되지 않은 건물 필드에 있다(스파이크 결과 참조).
4. ~~DataTable 쓰기 불가 시 ③·④~~ → 스파이크에서 쓰기 가능 확인. 해당 없음.

## 스파이크 결과 (2026-09-28, 테스트 세이브, 맵 Winding_River, 개발용 MLToyboxLab 모드로 실측)

아래 결과는 위 섹션의 판정보다 우선한다.

### UE4SS 3.0.1 Lua API 사용법 (확인됨)
- DataTable: `dt:GetRowNames()`, `dt:ForEachRow(function(name, row) ... end)`, `dt:FindRow("이름")`. **`FindRow`는 참조를 돌려주므로 필드 쓰기가 표에 그대로 남는다**(DT_Upgrades 행 "4" `regionalWealth` 25→0 유지 확인). 배열 필드는 `row.cost:Empty()`로 비워진다.
- UFunction이 돌려주는 `TArray`는 Lua 테이블이며, 요소는 `RemoteUnrealParam`이라 `x:get()`으로 꺼낸다. 프로퍼티 배열(`region.residents`)은 `#arr`, `arr[i]`로 바로 접근한다.
- out 파라미터 배열(`canUpgrade(id, reasons)`)은 빈 Lua 테이블을 넘기면 채워진다(요소는 `:get():ToString()`).
- `TMap`: `map:ForEach(function(k, v) ... k:get(), v:get() end)`
- 소유 판별: `region.ownerPawn:GetAddress() == pawn:GetAddress()`. 플레이어 지역은 7개 중 1개(Klainau).
- 설정 CDO: `StaticFindObject("/Script/ManorLords.Default__ResidentialRequirementSettings")`

### ① 자원 — ✅ 확정
- `region:getStockOfGood(16, false, false)` 읽기, `region:grantResources({ { Type = 16, amt = 10 } }, false)` → 목재 1147→1157 확인.
- `region.regionalWealth`, `pawn.influence` 직접 쓰기 반영 확인.
- **금고: `MLCheatManager_C:ChangeTreasury(n)`은 절대값이 아니라 증감(델타)이다**(66116 + 66123 → 132239 확인, 즉시 되돌림). 현재값은 HUD `W_HUD_LordPanel_V2_C` 중 `TreasuryNumeric`이 유효한 인스턴스의 `CurrentNumericValue`에서 읽는다(UI 의존, 금고 기능만의 한계). 목표 유지 = `ChangeTreasury(목표 - 현재)`.

### ② 건물 — 부분 확정
- 즉시 완공: ❌ Lua 불가.
  - `pawn:constructionComplete(b)`는 알림용이라 상태가 바뀌지 않는다.
  - `buildingStats` 행 `maxHp`를 낮춰도 `getConstructionProgress()`가 그대로다(창고 0.5005 유지).
  - 진행도는 건물 인스턴스의 리플렉션되지 않은 필드에 있다 → C++ 승격 후보.
  - `ConstructionBP_C` 액터는 건물 파츠(계단 등) 단위라 건물 완공과 무관했다.
- 대체안(Lua 가능): `buildingStats` 행의 `constructionGoods`를 비워 자재가 필요 없게 한다. 효과(새로 배치한 건물에 적용되는지)는 Plan 2에서 확인한다. 공사 인력은 여전히 필요하다.
- 즉시 수리: `MaintainAllBuildings()` 경로는 변경 없음(✅ 추정, Plan 2에서 확인).

### ③ 업그레이드 — ✅ 확정
- 레벨 2 주거지의 townhouse(ID 4)가 `residential_requirements_not_met`으로 막힌 상태였다. `ResidentialRequirementSettings.UpgradeRequirementsPerLevel[*].Requirements[*].VarietyRequired = 0`(33개)으로 만들자 **`canUpgrade(4)`가 false→true**, `AllResidentialRequirementsSatisfied()` → true가 됐다.
- 배열을 통째로 비우지 않고 값만 0으로 둔다(네이티브 인덱스 접근 보호).
- 비용·해금·정착지 레벨 조건은 `DT_Upgrades` 행 쓰기로 처리한다(쓰기 가능 확인). 막힌 이유 문자열에는 `trader_available_in+`(가축 주문 대기), `not_enough_space`(공간)도 있었는데, 이것은 데이터 조건이 아니라 상황 조건이라 범위 밖으로 둔다.

### ④ 군사 — 데이터 쓰기 확정, 효과는 Plan 2 인게임 테스트
- `pawn.maxNumOfMilitiaToSpawn` 쓰기 가능(기본 6). 테스트 시점에는 분대 5개라 상한 효과를 관찰하지 못했다.
- `engine.squads`(38개) 중 내 분대 5개: `maxSize`, `payDay`, `companyID` 읽기 가능.
- `engine.hiredMercs` 2개 계약 `cost` 50, 60 읽기 가능.
- `DT_UnitTemplates`(29행): 민병대 행의 `requiredEquipment`는 대부분 이미 비어 있고(`militiaFoot`만 2개), `minHouseLv` 1~2. 장비가 실제로 어디서 강제되는지(`weapons`/`armours` 목록 또는 무기고 재고)는 Plan 2에서 확인한다.

### 스파이크 부작용
- 테스트 세이브의 게임 실행 중 금고 증감은 즉시 원복했다. DataTable·설정 변경은 메모리에만 있고 세이브에 저장되지 않으며, 게임 재시작 시 원복된다.
- 목재 +10은 남아 있다. 지역 재화·영향력은 +1 후 원복했다. 태너리 1곳에 `constructionComplete` 알림을 한 번 호출했다(상태 변화 없음).

## Plan 2 인게임 검증 (2026-09-28, 테스트 세이브, Winding_River)

설정은 `control.json`을 직접 써서 적용했다(패널과 같은 스키마). 값은 `tools/lab.ps1`로 읽었다.

| 기능 | 결과 | 근거 | 후속 |
|---|---|---|---|
| ① 자원 | 통과 | 목재 1146→1246, 판자 1385→1485(목표값). 금고 66006→66506(목표값), 이중 가산 없음. 자원 ID 58개가 status에 보고됨 | 금고 HUD 반영이 적용 후 약 5~10초 늦다. 최종 리뷰 후 고정 쿨다운을 "같은 값 2회 연속일 때만 보충 + 보충 후 HUD 값이 바뀌거나 30초가 지날 때까지 대기"로 바꿨다(인게임 재검증은 다음 실행 때) |
| ③ 업그레이드 | 통과 | 가능 48→74, 막힘 57→31. 남은 31건은 모두 `not_enough_space`(물리 공간, 범위 밖) | — |
| ② 자재 불필요 | 통과 | `buildingStats` 창고 행 자재 1→0. 새로 배치한 건물 7개 모두 `constructionGoods` 0개, 상태 `constructing`, 진행도 약 0.5에서 시작(자재 단계 생략) | 공사 인력은 여전히 필요하다(설계대로) |
| ② 즉시 수리 | 부분 | `MaintainAllBuildings` 10초 주기 호출, 오류 없음 | 유지보수 부족 상태의 건물로 효과를 관찰하지 못했다 |
| ④ 군사 | 통과(데이터) | 민병대 상한 6→99, 고용 용병 4개 계약 비용→0(TMap 값 쓰기가 유지됨), `militiaFoot` 장비 요구 2→0·집 레벨 1→0, 용병 표 비용 90→0 | 월 정산 시 급여 차감 여부는 관찰하지 못했다. ①의 금고 유지가 보정한다 |
| ④ 친위대 유지비 | 미구현 | 친위대 급여 차감 경로(`FSquad.payDay`, `GetRetinueHireTreasuryCost`)는 분석하지 않았다. 패널 라벨에 "미지원"을 명시했다 | ① 금고 목표 유지로 보정. 필요하면 Plan 3에서 조사 |
| 공통 맵 재로드 | 통과 | 같은 세이브 재로드(16:42:52) 뒤 기능 4개 모두 `active=true`, `lastError` 없음, 자원 목표 유지 | — |

## Plan 3 네이티브 — 즉시 완공 (2026-09-28)
- 구현: `ASMBuildingMaster::getConstructionProgress`(구현 `0x144CBC7B0`, 패턴 `48 8B C4 48 89 58 08 48 89 70 10 57 48 83 EC 50 48 8B F1`) 후킹. 파츠 배열(`this+0x2F8`)의 각 파츠 `hp(+0x314)`를 `maxHp(+0x318)`로 채운다. 상세는 Plan 3 부록 A.1.
- 진행도 식: `(Σhp/ΣmaxHp + 자재비율) × 0.5`. 스파이크에서 `buildingStats.maxHp`를 낮춰도 효과가 없던 이유는 진행도가 파츠 인스턴스의 hp로 계산되기 때문이다.
- 완공 플래그 `this+0x3B1`은 인부 작업·게임 틱이 세운다. 인게임에서 배치 후 24초 안에 완공됐다.
- 분석 도구 주의: `exec <UFunction>`은 UFunction 생성 함수와 exec 썽크를 함께 돌려준다.

## Plan 3 네이티브 — 주민 수를 넘는 징집 (2026-09-28)
- 가용 징집 인원(`getAllAvailableRecruits` 구현 `0x144BE7D50`)은 `ARegion.residents`(+0x368)를 순회해 만든다. 민병대 병사는 실제 주민 유닛이다.
- 결론: 판정 후킹으로는 주민 수를 넘길 수 없다(없는 유닛을 만들 수 없음). 대안은 유닛 생성 UFunction(`spawnArmy`/`spawnCompleteUnit`)이며 사용자 결정 대기.

## Plan 3 네이티브 — 배치 제한 무시 (2026-09-28)
- 동적 분석(메모리 프로브)으로 숨은 배치 불가 플래그 `APawnCPP+0x60C`를 찾았다. 이 플래그를 쓰는 건물 배치 갱신 함수 `0x144AC61E0`을 후킹해, 원본 호출 뒤 `isInsideBorders`가 1이면 플래그를 0으로 만든다. 상세는 Plan 3 부록 A.3.
- 인게임(18:49~): 후킹 installed/active. 경계 안의 막힌 위치에 건물 3개를 배치했고, 공사가 진행 중이며 즉시 완공과 함께 동작했다. 모드 오류 0건, 크래시 없음.
- 한계: 영지 경계 밖은 의도적으로 허용하지 않는다. 도로·밭 배치 갱신(`0x144B03460`)은 대상이 아니다.

<<<<<<< HEAD
## 게임 버그 방어 — AI 민병대 평가 크래시 (2026-09-28)
- 증상: 전투 뒤 `EXCEPTION_ACCESS_VIOLATION reading 0xCA8`, RIP `exe+0x4B9133A`. 함수 `0x144B910C0`(APawnCPP 멤버, UFunction 없음)은 `0x144BB3A40`의 지역별 AI 틱 루프에서 호출된다.
- 동작: `region+0x2E8`이 켜져 있고 쿨다운 `region+0x7C8 <= 0`이면 `+0x7C8 = 1`로 설정한다. 그다음 `pawn.commandedSquads`(0xA10)의 분대 가운데 `squadType == 1`이고 `originRegion == region`인 분대를 골라, `assignedRecruits`(0x348)의 각 유닛에 대해 `unit.Home(0x340)->lv(0xCA8)`를 null 검사 없이 읽는다.
- 원인: AI 지역 민병대원의 가족이 집에서 몇 초간 빠졌다가 같은 집으로 돌아오는 일이 있다(Lab 기록: occupants 0 → 1, 약 4초). 그 사이에 이 함수가 호출되면 크래시한다. **MLToybox를 제거한 상태에서도 같은 세이브로 재현됐다**(게임 자체 버그). 생성 분대(type 0)는 이 경로를 타지 않는다.
- 조사 결과: `squads` 배열 인덱스와 분대 ID는 일치한다(58개 모두).
- 같은 AI 틱 루프(`0x144BB41F0`~)에서 바로 다음에 호출되는 형제 함수 `0x144B923D0`(쿨다운 `region+0x7CC`)도 같은 읽기(`mov rcx,[rax+340h]; mov eax,[rcx+0CA8h]`)를 한다. 이 바이트열은 exe 전체에서 두 곳뿐이다. 첫 수정 뒤 이 함수에서 크래시가 났다(RIP `exe+0x4B9292A`).
- 방어: 네이티브 `militia_guard`/`militia_guard_2`가 원본 호출 직전에 같은 조건의 분대원을 검사하고, Home이 null이면 그 호출만 건너뛴다. 쿨다운을 설정하기 전이므로 다음 AI 틱에 다시 실행된다. 항상 켜져 있다.
- 인게임 검증: 수정 후 같은 세이브로 전투해도 크래시가 나지 않았고, 상태는 `installed=True active=True`였다.

## 생성 분대 해제·재구성 (2026-09-29)
- 게임이 만든 민병대·친위대는 해제하면 병사가 집으로 돌아가고, 카드는 `assignedRecruits`를 기준으로 N/N을 유지한다. 집결 버튼은 그 모집병을 다시 불러온다(예: 순정 친위대 25번은 units 0, recruits 5, 모집병에게 Home이 있다).
- `spawnArmy`로 만든 분대는 `squadType=0`, `companyID=-1`이고 병사에게 Home이 없다. 해제하면 units와 recruits가 모두 0이 되고, 게임은 0/N 빈 카드를 남기며 집결·삭제 버튼을 숨긴다. 이 동작은 `squadType`을 용병(2)으로 바꿔도 같았다(실험).
- 해제·집결 버튼은 `PawnCPP:rallySquads`·`disbandSquad` 등 UFunction을 거치지 않는다(Lua 후킹으로 호출이 잡히지 않았다).
- `APawnCPP::removeSquad(id)`(exec `0x144A65550`, 구현 `0x144AFC390`)는 엔진 `+0xFC0` 제거 대기열(TArray<int32>)에 id를 AddUnique로 넣는다. 게임이 곧(3초 이내) 해당 분대를 배열에서 지우고, 뒤쪽 분대를 한 칸씩 당겨 ID를 인덱스에 맞게 다시 매긴다. `commandedSquads`도 함께 갱신된다(실측: 43번 제거 후 id≠index인 분대 0개).
- 재구성: 빈 카드(플레이어 소유, type 0, company -1, units 0, recruits 0)는 세이브에 `unitType`과 함께 남으므로 예비 기록으로 쓴다. 모드는 같은 병종을 `spawnArmy`로 다시 생성하고, 빈 카드는 높은 ID부터 한 틱에 하나씩 `removeSquad`로 지운다. 지우는 대상은 새 분대보다 앞 번호로 한정하고, 배열이 줄어든 것을 확인한 뒤 다음 것을 지운다.
=======
## 인구 기능 (2026-09-28)
- `ARegion.growPopulation()`은 이민이 아니라 주민 나이 증가 처리였다(구현 `0x144BED220`, 주민별 카운터 +0xD10이 30에 도달하면 처리). 호출해도 가족 수가 변하지 않는다.
- `UPerkSettings.IncreaseImmigrationRate`(+0x1FC)를 읽는 코드가 게임 모듈에 없고, 해당 perk도 없다(`GetPerkForPerkEffect(32)` = None). perk 방식 배율은 불가하다.
- 이민 경로 역추적: `AddNewFamily` 구현(`0x144BC2010`)의 호출자 `0x144CD1FA0`는 UFunction **`ASMBuildingMaster.spawnManorServantsInside(int32)`**의 구현이다. 건물 위치에 주민을 만들고 `AddNewFamily`로 등록하며, `occupantFamilyIDs`(+0xF98, Num +0xFA0)가 2 미만인 동안 반복한다.
- 인게임: 빈 주거지에 1회 호출 → 거주 가족 0→1, 곧 구성원 3명(정상 규모). 저장·재로드 후에도 유지된다.
- 구현: `features/population.lua`
  - 배율: 자연 증가분 × (배율−1)만큼 추가한다. 모드가 들인 가족에는 배율을 다시 걸지 않는다.
  - 목표 가족 수: 부족분만 채운다.
  - 명령 `addFamilies`: 1~20가족, 빈 집부터 채운다.
>>>>>>> feat/population

## 인구 — 추가 가족의 배치 상태 (2026-09-29)
- `spawnManorServantsInside(1)`로 들인 가족은 그 집에 일꾼으로 배치된다(`workerFamilies[id].assignedTo == familyHome`). 그래서 자연 이민 가족처럼 미배치 인력으로 잡히지 않는다.
- `ARegion::unassignFamily(familyID)`를 호출하면 미배치가 된다(실측: `getNumUnassignedFamilies` 4 → 5). familyID는 `region.workerFamilies`의 인덱스이고, 집의 `occupantFamilyIDs`에 들어 있다.
- 수정: 가족을 추가한 직후 집의 `occupantFamilyIDs`에서 새로 생긴 id를 찾아 `unassignFamily`를 호출한다.
- 이미 들어온 가족은 건드리지 않는다. 조사 시점에 일터가 자기 집인 가족이 hof 80, sel 17이었는데, 순정 장인(버게이지 작업장) 가족과 구분할 방법을 찾지 못했다.

## 영주 값 HUD 갱신 (2026-09-29)
- `pawn.influence`에 직접 쓰거나 `changeKingsFavour`를 호출해도 영주 HUD(`W_HUD_LordPanel_V2_C`의 `InfluenceNumeric`/`FavorNumeric`)는 갱신되지 않는다(실측: 내부 값 33333, HUD는 13초 뒤에도 20410). `ChangeTreasury`는 HUD를 갱신하므로 국고를 바꾸면 다른 값도 함께 갱신돼 보였다.
- 위젯의 `updatePlayerStats()`를 호출하면 즉시 갱신된다(영향력 20410→33333, 총애 50000→44444 실측).

## 패널이 상태를 못 읽던 문제 (2026-09-30)
- 모드의 JSON 인코더는 빈 Lua 테이블을 `[]`로 쓴다. `spawn.byUnit`이 비면 `"byUnit":[]`가 되는데, 패널은 이 필드를 `Dictionary<string,int>`로 읽다가 실패하고 상태 전체를 null로 처리했다(실측 오류: `$.spawn.byUnit`). 그 결과 패널은 "연결 안 됨"을 띄우고, "현재" 값은 "-"로, 영지 목록은 기본값만 보였다. 생성 분대 재구성 기능을 넣은 뒤 해제된 생성 분대가 없는 동안 항상 이 상태였다.
- 수정: 패널 JSON 옵션에, 문자열 키 사전 자리에 온 배열을 빈 사전으로 읽는 변환기를 추가했다.
