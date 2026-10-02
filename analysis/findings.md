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

## 인구 기능 (2026-09-28)
- `ARegion.growPopulation()`은 이민이 아니라 주민 나이 증가 처리였다(구현 `0x144BED220`, 주민별 카운터 +0xD10이 30에 도달하면 처리). 호출해도 가족 수가 변하지 않는다.
- `UPerkSettings.IncreaseImmigrationRate`(+0x1FC)를 읽는 코드가 게임 모듈에 없고, 해당 perk도 없다(`GetPerkForPerkEffect(32)` = None). perk 방식 배율은 불가하다.
- 이민 경로 역추적: `AddNewFamily` 구현(`0x144BC2010`)의 호출자 `0x144CD1FA0`는 UFunction **`ASMBuildingMaster.spawnManorServantsInside(int32)`**의 구현이다. 건물 위치에 주민을 만들고 `AddNewFamily`로 등록하며, `occupantFamilyIDs`(+0xF98, Num +0xFA0)가 2 미만인 동안 반복한다.
- 인게임: 빈 주거지에 1회 호출 → 거주 가족 0→1, 곧 구성원 3명(정상 규모). 저장·재로드 후에도 유지된다.
- 구현: `features/population.lua`
  - 배율: 자연 증가분 × (배율−1)만큼 추가한다. 모드가 들인 가족에는 배율을 다시 걸지 않는다.
  - 목표 가족 수: 부족분만 채운다.
  - 명령 `addFamilies`: 1~20가족, 빈 집부터 채운다.

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

## 지역당 건물 개수 제한 (2026-09-30)
- `buildingStats` 표의 `FStat.maxInRegion`(0x2D8)이 지역당 최대 개수다. 실측 102개 행 가운데 9개가 1이고 나머지 93개는 0이다(0 = 제한 없음, 주택 등).
- 1인 행: 57 `manor_keep_lv1`(수비용 탑, garrison 12), 58 Tax Collector, 81 `manor_palace_lv3`, 83 `manor_keep_lv3`, 98 `manor_palace_lv1`, 102 `firewood_cart`, 103 `food_cart`, 474 `manor_palace_lv2`, 475 `manor_keep_lv2`.
- 툴팁의 "최대 수행원 규모가 12만큼 증가"는 `garrisonLimit` 12와 일치한다.
- 옵션 `build.noRegionLimit`은 이 값을 0으로 만든다. 데이터 표 변경은 메모리에만 남는다.

## 성벽 배치 크래시 — 배치 제한 무시와 충돌 (2026-09-30)
- 증상: `EXCEPTION_ACCESS_VIOLATION reading 0x738`, RIP `exe+0x4B047E5`(함수 `0x144B03460`, APawnCPP 도로/성벽 배치 처리. 함수 첫머리에서 `roadmode` +0x7B0을 검사).
- 이 함수는 성벽 지점이 속한 영지를 `0x144C4F230`(엔진 영지 배열 +0x550에서 점을 포함하는 영지 검색, 없으면 null)으로 찾는다. 없으면 폰의 배치 불가 플래그 `+0x60C`를 1로 세우고 뒤 처리를 건너뛴다. 플래그가 0이면 null 영지의 `+0x738`(`ARegion::CityPlanningComponent`)을 읽는다.
- 배치 제한 무시(네이티브 `placement`)가 같은 `+0x60C`를 0으로 지우고 있었다. 사용자 재현 결과, 이 기능을 끄면 튕기지 않고 켜면 튕겼다.
- 수정: 폰이 도로 모드(`roadmode`)이면 플래그를 건드리지 않는다.

## 용병 고용 — 목록 보충과 커스텀 용병단 (2026-09-30, 스파이크)
exe 정적 분석과 Lab 실측(맵 LargeLake, 진행된 세이브와 `saveGame_8`)을 함께 썼다. 제품 코드는 바꾸지 않았다.

### 함수 (exec 썽크 → 구현)
- `refillMercenaries`: `0x1449F6FC0` → `0x1410A0950`. 구현이 `ret`뿐인 빈 함수다.
- `rerollMercenaries`: `0x144A8BDC0` → `0x144C60460`.
  - `availableMercs`(엔진 +0xC00)를 비우고 `DT_MercenaryCompanies`(+0xBF8)에서 최대 3개를 중복 없이 무작위로 뽑는다.
  - 후보에서 빠지는 행: 특정 특성 2종 가운데 하나를 가진 행, 그리고 `hiredMercs`(+0xC10)에 같은 `Name`이 있는 행.
  - 뽑은 항목에 `arrivalRegion`(엔진 영지 배열 +0x550에서 무작위)과 `arrivesIn`(10~40)을 채운다. 결과가 1개 이상이면 알림을 띄운다.
  - 네이티브 호출처는 두 곳뿐이다: 엔진 시작 처리 `0x144C08CCA`(`engine+0x2F1`이 0일 때만), 새 게임 설정 `0x144D145AA`. 세이브를 불러올 때는 `0x144C1D040`이 저장된 목록을 복원한다.
- `hireMercs`: `0x144A8A240` → `0x144C50D00`. `arrivalRegion`과 폰이 null이 아니면 고용한다. 국고·부대 수·계약 수 검사는 없다. 빈 ID를 찾아 `hiredMercs`에 넣고, `availableMercs`에서 빼고, 분대를 만들어 `squadType`을 2로 두고, `cost`가 0보다 크면 국고에서 뺀다.
- `getCompanyCostForPawn`: `0x144A5EF20` → `0x144AE1310`. `company.cost`를 그대로 돌려준다.
- `canAddNewMilitiaSquad`(구현 `0x144ACAAE0`)는 `commandedSquads` 가운데 `squadType == 1`만 세어 `maxNumOfMilitiaToSpawn`과 비교한다. 용병 고용 화면 BP는 이 함수를 부르지 않는다(덤프의 BP 지역 변수 `CallFunc_*`로 확인. 호출하는 BP는 `W_HUD_ArmyRecruitCardV2_C`, `addSquadCard_C`).
- AI 영주도 같은 목록에서 고용한다. `0x144BACED0`이 `availableMercs`를 돌며 `pawn+0xA40 >= cost`이면 후보로 삼고 `hireMercs`를 부른다(호출 `0x144BADA15`, `0x144BB773B`, `0x144BB7B24`).

### 인게임 실측
- 용병 표는 11행이다. `questOnly` 특성의 2행(`hildebolts_army`, `hildebolts_army_large`)은 reroll 100회에 한 번도 나오지 않았다. 나머지 9행은 고용 중이 아니면 모두 나왔다.
- 진행된 세이브의 상태: `availableMercs` 0개, `hiredMercs` 4개. 후보가 5개 남아 있는데도 목록이 비어 있었다. 목록을 주기적으로 다시 채우는 동작은 없다.
  - `hiredMercs` 4개 가운데 2개(`battle_brothers`, `brigands`)는 분대가 하나도 없는데 항목이 남아 있었다. 이런 항목도 reroll 후보에서 그 이름을 뺀다.
- Lua에서 `engine:rerollMercenaries()`를 부르면 목록이 0 → 3개가 된다.
- **비용이 0이면 AI가 가져간다.** `zeroUpkeep`으로 비용이 0인 상태에서 reroll한 3개 가운데 2개(`brigands_small`, `huntsmen`)를 3분 안에 AI가 고용했다(분대 소유자가 플레이어가 아님). 남은 1개(`wayward_sons`)는 `hiredMercs`에 없이 목록에서 사라졌다(원인 미확인). 비용을 5001 이상으로 올린 목록은 약 2분 동안 변화가 없었다.
- 고용 버튼(`mercenaryCompanyCard_C.Button_76`)은 `cost`가 국고보다 클 때만 꺼진다(국고 148,500에서 cost 99,999,999 → 꺼짐, 77 → 켜짐).
- 목록 항목은 Lua에서 제자리 쓰기가 된다: `cost`, `arrivesIn`, `Name`(Lua 문자열, 한글 포함), `arrivalRegion`(영지 객체), `units`(`c.units = { FName(...), ... }` 테이블 대입. 3 → 5 → 1개로 바꾸면 배열이 다시 할당된다).
  - 고용 창(`mercenaryScreen_C:updateCompanies()`)은 바뀐 값을 그대로 보여 준다. 번역 키가 없는 이름은 문자열 그대로 나온다.
- UE4SS Lua의 `TArray`에는 추가 함수가 없다(`Empty`, `ForEach`, `GetArrayNum`, `GetArrayMax`, `GetArrayAddress`, `GetArrayDataAddress`뿐). 목록 길이는 reroll로만 늘릴 수 있다.
- **배열 전체 대입은 게임을 튕긴다.** `engine.availableMercs = { {Name=..., units=..., banner=..., ...}, ... }`는 UE4SS 안에서 `EXCEPTION_ACCESS_VIOLATION reading 0xb`를 냈다. 쓰지 않는다.
- 커스텀 항목 고용: 목록 1번을 이름 "토이박스 용병단", 비용 77, 분대 2개(`mercenary_infantry`, `mercenary_crossbowmen`), 도착 영지 = 내 영지로 바꾼 뒤, 카드 버튼 이벤트와 확인 창 이벤트(`BndEvt__HireConfirmation_menuButton_K2Node_ComponentBoundEvent_2_onReleased__DelegateSignature`)로 고용했다. `hiredMercs`에 `0=토이박스 용병단`이 생기고 분대 2개(type 2, 36명씩, 플레이어 소유)가 바로 만들어졌다.
- 이름 제외와 우회: `greencaps`를 고용한 뒤 reroll 30회에 `greencaps`는 0회였다. 표의 그 행 `Name`을 `greencaps#2`로 바꾸자 30회 가운데 10회 나왔다. 표 행 이름을 바꾸면 고용 중인 용병단도 다시 후보가 된다.

### 미확인
- ~~커스텀 항목(목록·고용 중)이 세이브와 로드를 거쳐 유지되는지.~~ 유지된다(아래 "용병 실측 M1~M5"의 M4).
- `wayward_sons`가 고용 기록 없이 목록에서 사라진 원인.
- reroll이 제외하는 두 번째 특성 이름.
- 분대가 없는 `hiredMercs` 항목이 정리되는 시점.

### 개발 절차 메모
- Lab으로 메인 메뉴에서 세이브를 불러올 수 있다(화면 조작 불필요). 도구: `tools/lab-load.ps1 -Slot <슬롯> [-Start]`.
  1. `GameplayStatics:LoadGameFromSlot("<슬롯>_descr", 0)`으로 기술자(`UMLSaveGameDescr`)를 읽는다.
  2. 게임 인스턴스(`BP_MLGameInstance_C`)의 `savefileToLoad`에 슬롯 이름을 쓴다.
  3. 뷰포트에 있는 `mainMenu_widget_C`에 `LoadGame({ SlotIndex = <번호>, Descriptor = <기술자> })`를 부른다. 번호는 autosave 0, quicksave 1, `saveGame_N`은 N + 2다.
- **정정(2026-09-30 저녁)**: 처음 적었던 절차(`SwitchToLoadScreen` → `SortedSlotsByDate` → `LoadGame`)는 세이브를 불러오지 않고 **새 게임을 시작했다**. `LoadGame`은 화면 전환만 하고, `savefileToLoad`가 비어 있으면 새 게임이 된다. 불러올 때마다 시작 영지가 달랐고 가구가 5였다(실제 `saveGame_8`은 영지 3개, 가구 155, 국고 148,500).
  - 따라서 이 절에서 게임을 다시 켠 뒤에 한 실측(커스텀 항목 고용, 이름 제외와 우회 등)은 `saveGame_8`이 아니라 새 게임에서 한 것이다. 엔진과 위젯의 동작을 본 것이라 결론은 그대로다.
  - "진행된 세이브의 상태"(목록 0개, 고용 4개, 국고 148,500)는 사용자가 직접 불러온 게임에서 잰 값이고, 고친 도구로 `saveGame_8`을 불러와 같은 값을 다시 확인했다.
- 로드 화면은 번호가 이어진 슬롯만 찾는다. `saveGame_900`은 목록(`SortedSlotsByDate`)에 나오지 않지만 위 절차로는 불러와진다.
- 메인 메뉴의 `menuButton_continue`는 보이지 않는 상태였다(`continue()` 경로는 쓰지 않았다).

## 용병 실측 M1~M5 (2026-09-30)
구현 계획 `docs/superpowers/plans/2026-09-30-mercenary-companies.md` Task 1의 결과다. M1, M2, M3, M5는 새 게임(맵 LargeLake)에서, M4는 그 게임을 `saveGame_900`에 저장했다가 게임을 다시 켜고 불러와서 쟀다(위 정정 참고).

| # | 항목 | 결과 | 근거 |
|---|---|---|---|
| M1 | 표 행의 깃발·특성을 목록 칸에 쓰기 | PASS | `wayward_sons` 행을 1번 칸에 쓴 뒤 `traits=[excellent_marksmen,melee_capable]`, `banner=.../wayward_sons.wayward_sons`, 색·문장 `1 3 1 1`로 행과 같았다. 특성이 2개 있는 칸에 `traits:Empty()` → 0개. 게임은 튕기지 않았다. |
| M2 | 민병대·친위대 병종의 용병 고용 경로 생성 | 8종 모두 PASS | `hired 0 name=MLT M2 squads=8 mine=8 units=[militia:32,spearMilitia:32,militiaPole:32,militiaFoot:32,bowMilitia:32,crossbowMilitia:32,retinue_tier1:36,retinue_tier3:36]` |
| M3 | 부대 패널 위젯의 커스텀 이름 | PASS | `W_HUD_MercenaryCompanyV2_C name=MLT M2`(저장·로드 뒤에도 같음). `hiredMercenaryCompanyBanner_C`에서는 이름을 읽지 못했다. 화면에 실제로 어떻게 보이는지는 확인하지 않았다. |
| M4 | 저장·로드 뒤 커스텀 목록 칸과 고용 중 용병단 유지 | PASS | 다시 불러온 뒤 `slot 1 name=MLT M4 list cost=1234 arrivesIn=1 units=[mercenary_infantry,mercenary_crossbowmen]`, `hired 0 name=MLT M2 cost=0 squads=8 mine=8` |
| M5 | 고용 창·확인 창 `IsVisible()` | PASS | `closed screen=false confirm=false` → `open screen=true confirm=false` → `confirming screen=true confirm=true` → `after cancel screen=true confirm=false`, 취소로 고용이 생기지 않음, 닫고 3초 뒤 `screen visible=false` |

- 확인 창의 취소 이벤트는 `BndEvt__HireConfirmation_menuButton_1_K2Node_ComponentBoundEvent_3_onReleased__DelegateSignature`다.
- 저장은 `MyPawnCPP_BP3_C:SaveToDiskWithThumbnail("saveGame_900", "MLToybox test")`로 했다. 파일 5개(`.sav`, `_descr.sav`, `.png`, `_coat_1.png`, `_coat_2.png`)가 생긴다.
- 테스트 세이브 `saveGame_900`("MLToybox test")이 세이브 폴더에 남아 있다. 게임의 로드 화면에는 보이지 않는다.
- 실제 `saveGame_8`의 용병 상태: 목록 0개. 고용 중 4개(`battle_brothers` 분대 0, `brotherhood_of_the_forest` 내 분대 1, `brigands` 분대 0, `vultures` AI 분대 2).

## 용병 기능 인게임 검증 (2026-09-30, saveGame_8)
실제 `saveGame_8`(영지 3개, 국고 148,500, 고용 중 4개, 목록 0개)을 불러와, 다른 기능을 모두 끄고 `control.json`의 `features.mercenaries`만 바꿔 가며 확인했다. 고용은 게임의 화면 경로(`tools/lab/merc_hire.lua`)로 했다. 끝난 뒤 게임을 저장 없이 종료했다.

| 항목 | 결과 | 근거 |
|---|---|---|
| 자동 보충 | 통과 | 로드 뒤 `available 3`: `crazy_goose@90`, `huntsmen@250`, `brigands_small@15`. 표의 고용비와 같다. `features.mercenaries.active=true`, `lastError` 없음 |
| 커스텀 등록 | 통과 | `slot 1 name=토이박스 용병단 arrivesIn=1 region=Mandlach units=[mercenary_infantry,mercenary_crossbowmen] traits=[]`. 떠 있던 순정 2개는 2·3번 칸으로 옮겨 가고 도착 정보(`arrivesIn=25`, `10`)를 유지했다 |
| AI 잠금 | 통과 | 닫힘: `cost=10000000`. 열림: `cost=3000`. 다시 닫으면 `10000000`. 닫은 채 5분(게임 날짜 297 → 306일) 동안 커스텀 칸이 1번에 남았고 AI가 고용하지 않았다 |
| 고용·상시 유지 | 통과 | `HIRED` 3회. 매번 `available 3`이고 1번 칸이 다시 `토이박스 용병단`. `hired 4·5·6 name=토이박스 용병단 cost=0 squads=2 mine=2 units=[mercenary_infantry:36,mercenary_crossbowmen:36]` |
| 환급 | 통과 | 국고 148,500 → (3회 고용 뒤) 148,500. `refunded=9000`, `hiredMine=4`(기존 1 + 3) |
| 전부 고용 | 통과 | 남은 순정 5개(`huntsmen`, `greencaps`, `crazy_goose`, `wayward_sons`, `brigands_small`)를 차례로 고용 → `available 1`(커스텀만). 표 행 이름에 `#mlt` 없음. 그 상태에서 커스텀을 2회 더 고용했고 매번 다시 채워졌다. `refunded=15490`(= 3000×5 + 250 + 90 + 90 + 45 + 15), 국고 148,500 |
| 안정성·다시 불러오기 | 통과 | `UE4SS.log`에 `mercenaries` 줄 없음. 가장 최근 크래시 폴더는 16:22(검증 전). 게임을 다시 켜고 불러온 뒤 `available 3`, 1번 칸 `토이박스 용병단`, 표 행 이름 정상 |
| 끄기 | 통과 | `enabled=false` 뒤 목록 3칸이 그대로이고 `status.mercenaries`가 없다(`active=false`) |

검증 중에 새로 확인한 것:
- **고용 창이 열려 있는 동안 게임은 일시정지다.** `GameplayStatics:IsGamePaused`가 열림에서 `true`, 닫은 뒤 `false`였고, 닫힌 뒤에만 날짜(`WeatherMaster.daysTotal`)가 흘렀다. 일시정지 중에는 AI가 고용하지 못할 것으로 보이지만, 이것은 직접 확인하지 않았다.
- **게임이 목록 칸을 고용 기록 없이 지우는 때가 있다.** 날짜가 1-12-31에서 2-1-1로 넘어갈 때 `crazy_goose`가 목록에서 사라졌고 `hiredMercs`에는 새 항목이 없었다. 모드가 다음 점검에서 `greencaps`로 채웠다. 스파이크에서 `wayward_sons`가 사라진 것과 같은 현상으로 보인다(지우는 조건은 확인하지 않았다).
- 계획과 다르게 고친 것: 목록이 꽉 찬 상태에서 커스텀을 등록하면 3번 칸에 들어갔다(이름이 같은 칸을 제자리에 두는 방식). 스펙 2.1에 맞춰, 제자리 수정도 "커스텀 → 순정" 순서를 따르고 옮겨 간 순정 칸의 도착 정보를 함께 옮기게 했다.
- 브랜치 리뷰 뒤에 고친 것(2026-09-30):
  - 고용 직후처럼 재구성을 기다리는 동안(최소 간격 3초, 불일치 뒤 10초)에는 커스텀 칸의 잠금 가격이 갱신되지 않았다. 이제 기다리는 동안에도 떠 있는 커스텀 칸의 고용비는 매 점검에서 맞춘다.
  - 표 행 이름을 임시로 바꾸는 도중에 실패하면 이미 바꾼 행이 되돌아오지 않았다. 이름 바꾸기와 다시 뽑기를 한 보호 구간에 넣었다.
  - 패널: 영지 목록이 없을 때(게임이 꺼져 있을 때) 용병단을 고쳐 등록하면 저장된 도착 영지가 "내 첫 영지"로 바뀌었다. 목록에 없는 키도 선택지로 남긴다.
- 고친 뒤 인게임 재확인(`saveGame_8`):
  - 순정 카드를 고용하고 1초 뒤 다른 카드를 고용하면서 창을 닫았다. 재구성 전(`available 2`)인데도 1초 안에 `토이박스 용병단@10000000`이 됐고, 약 3초 뒤 3칸으로 다시 채워졌다.
  - 순정 5개를 모두 고용한 뒤 커스텀을 2회 더 고용했다. 표 행 이름에 `#mlt` 없음, `note` 없음, 국고 148,500, `refunded=9490`.
- 켜기 전에 고용한 용병단이 로드 직후에 잘못 환급되는지 실측: 환급을 끈 채 커스텀을 고용해(`hired 12 cost=3000`, 국고 145,500) `saveGame_900`에 저장하고, 환급을 켠 뒤 게임을 다시 켜 그 세이브를 불러왔다. 로드 뒤 `refunded=0`, `hired 12 cost=0`, 국고 145,500. 환급 없이 0으로만 만들었다(1회 측정).
  - 이 과정에서 `saveGame_900`의 내용이 바뀌었다(M4 때의 새 게임 → 실제 게임에 테스트 고용을 더한 상태). 로드 뒤 목록은 `available 1`(커스텀, 10,000,000)로 유지됐다.
- 확인하지 않은 것: 고용 창과 부대 패널에서 커스텀 용병단이 화면에 어떻게 보이는지(이름, 깃발), 게임의 새 용병단 알림이 실제로 뜨는지.

## 커스텀 용병단 깃발 (2026-10-01)
`features.mercenaries.companies[].banner`에 용병 표 용병단 이름을 주면 그 행의 `banner`·`colorA/B`·`emblemA/B`를 카드에 쓴다.
- `saveGame_8`에서 `banner = "greencaps"`로 등록한 카드: `banner=greencaps.greencaps colors(1,5,1,1)`. 고용한 뒤 `hiredMercs`의 항목도 같았고, `saveGame_900`에 저장하고 게임을 다시 켜 불러온 뒤에도 같았다.
- 표에서 `greencaps`와 `the_huntsmen`(Name `huntsmen`)은 같은 깃발 그림(`greencaps.greencaps`)과 같은 색(1,5,1,1)을 쓴다. 그래서 칸의 깃발을 이름 하나로 되읽으면 고른 이름과 어긋나 매 점검마다 다시 쓰게 된다. 칸은 그 묶음을 가진 용병단 이름 **목록**으로 되읽고, 고른 이름이 목록에 있으면 같은 것으로 본다.
- 화면에서 깃발이 어떻게 보이는지는 확인하지 않았다.

## 수행원 꾸미기 — 모드가 만든 분대에 쓸 수 있는가 (2026-10-01, 스파이크)
질문: 모드로 만든 분대(`spawnArmy`, 커스텀 용병 고용)에 수행원 병종이 있을 때 게임의 수행원 꾸미기 화면을 쓸 수 있는가. `saveGame_8`(영지 gold·Lei·nus)에서 쟀다. 저장은 `saveGame_900`에만 했다.

### 구조 (덤프)
- 꾸미기 화면 `retinueEditor_C`는 부대 명령 버튼 `W_HUD_ArmyCommandList.Skill_CustomizeRetinue` → `W_HUD_ArmySkillButtonV2_C:OpenRetinueEditor()` → `retinueEditor:Open(ManorRef)`로 열린다. 화면의 필드: `squadID`, `ManorRef`, `selectedRegion`, `retinue`(`TArray<ASMUnit*>`), `Squad`(`FSquad`), `selectedRetainer`.
- 영지 `ARegion`에 `retinueSquadID`(+0x508)와 `retinueName`이 있다. 저택 `ASMBuildingMaster`에 `getBoundRetinue()`, `canHireRetinue()`, `GetRetinueHireTreasuryCost()`, `hireExtraRetinue()`가 있다. 폰에 `addRetinueSquad(region)`이 있다.
- 외형은 분대가 아니라 병사(`ASMUnit`)에 있다: `Equipment`(FEquipment 7개 int), `equipmentMeshVariations`·`equipmentTextureVariations`(`TMap<EEquipmentSlot, uint8>`), `equipmentColorSchemeUVs`(`TMap<EEquipmentSlot, FVector>`), `DisplayName`, `Home`. 함수 `setVAMPColorSchemeUVsForSlot(slot, vec)`, `SetVAMPVariationsToEquipment()`.

### 실측
| # | 내용 | 결과 |
|---|---|---|
| R1 | 영지별 수행원 | 영지마다 수행원 분대 1개(`squadType=3`, `unitType=retinue_tier1`, `unitArr` 0개 — 수행원은 집결 전에는 저택에 있다). `maxSize` 24/24/12. 저택에 묶인 수행원 5명, `canHireRetinue=true`, 비용 50 |
| R1b | 저택에 묶인 수행원 | 이름 있음(Mathes, Fritz, …), `Home`=저택, `assignedSquadID`=수행원 분대, 집에서는 `Equipment=[164,281,0,0,0,292,0]`, 슬롯 7개의 무늬·질감·색 값이 있음 |
| R2 | 모드 방식 생성(`spawnArmy(…, {retinue_tier1}, pawn, -1, 0)`) | `squadType=0`, 병사 36명, 이름 없음, `Home` 없음, `Equipment=[164,281,312,157,247,276,247]` |
| R3 | 게임 꾸미기 화면을 모드 분대로 열기 | `region.retinueSquadID`를 모드 분대 번호로 잠깐 바꾸고 `editor:Open(manor)` → `squadID=63`, `retinue=36`(그 분대 병사), `Squad=63/retinue_tier1/n36`. `Set Squad to Squad ID`·`populateRetainerGrid`·`Close` 정상. 번호를 되돌리면 원래대로 |
| R6 | 열린 화면의 조작이 모드 병사에게 가는가 | `setSelectedRetainer(모드 병사)` → `selectedRetainer`가 그 병사. `upgradeCurrentRetainersArmor(false)` → `isCurrentRetainerUpgraded` false→true(가격 26. 국고 HUD 값은 148,500 그대로). 무늬 버튼·`propagateColorChange`를 코드로 부른 것은 병사 값을 바꾸지 않았다(UI 상태를 쓰는 함수라 이 방법으로는 못 잰다). 이름 입력 핸들러를 Lua 문자열로 부르자 게임이 두 번 튕겼다(`EXCEPTION_ACCESS_VIOLATION reading 0x70`) — 호출 방식 문제이지 기능의 증거가 아니다 |
| R4 | 외형 값 복사 | 실제 수행원(Mathes)의 `DisplayName`, 무늬·질감 맵(`Add`), 색(`setVAMPColorSchemeUVsForSlot`)을 모드 병사에게 쓰고 `SetVAMPVariationsToEquipment()` → 되읽은 값이 원본과 같음 |
| R7 | 복사한 값의 저장·로드 | `saveGame_900`에 저장, 게임을 다시 켜 불러온 뒤 그 병사(`MLT Mathes`)의 무늬·질감·색이 그대로. (이 병사는 커스텀 용병으로 고용한 분대의 병사였다 — 모드가 만든 집 없는 병사라는 점은 같다) |
| R5 | 게임의 수행원 증원 | `manor:hireExtraRetinue()` → 묶인 수행원 5→6명(새 이름 Heintz)이 바로 생김. 국고 HUD 값 변화 없음 |

### 판단
- 게임의 꾸미기 화면은 영지의 `retinueSquadID`가 가리키는 분대를 대상으로 열리고, 모드 분대를 가리키게 하면 그 분대 병사 36명을 대상으로 열린다. 선택과 갑옷 업그레이드 함수는 그 병사에게 적용됐다. 실제 화면에서 버튼·이름 입력·색 선택이 동작하는지는 사람이 눌러 봐야 한다.
- 외형 값을 병사에게 직접 쓰는 것은 되고 세이브에 남는다. 화면에 어떻게 보이는지는 확인하지 않았다.
- 대안: 게임 자체의 수행원 증원(`hireExtraRetinue`)으로 진짜 수행원을 늘리면 꾸미기는 원래 기능 그대로다(영지당 최대 24명).
- 확인하지 않은 것: `retinueSquadID`를 바꿔 둔 동안 게임 로직(집결·귀환)이 어떻게 되는지, 화면의 `Size/maxSize`(항상 0/0으로 읽힘)의 뜻, 36명이 화면 격자에 다 나오는지.

## 수행원 꾸미기 기능 인게임 검증 (2026-10-01, saveGame_8)
`customizeRetinue` 명령(분대 63 = `spawnArmy`로 만든 `retinue_tier1` 36명, 영지 nus)으로 확인했다. 저장하지 않았다.

| 항목 | 결과 |
|---|---|
| 상태 보고 | `status.retinue.squads = [{id 63, retinue_tier1, 36, spawned}]`. 진짜 수행원 분대(22, 33, 62)와 민병대 분대는 없다 |
| 진짜 수행원 분대(22)에 명령 | `squad 22 is not a retinue squad made by the mod` |
| 열기 | 결과 `{ok, squad 63}`. 화면 `visible=true squadID=63 retinue=36`, 선택된 병사가 분대 63 소속. 영지 nus의 `retinueSquadID` 22 → 63, 다른 영지는 그대로. `status.retinue.editing = 63` |
| 열려 있는 동안 다시 명령 | `retinue editor is already open` |
| 닫기(`editor:Close()`) | 3초 안에 `retinueSquadID` 63 → 22 복원, `editing` 없음. 기능 오류 없음 |
| 일시정지 | 이 방법으로 연 화면에서는 `IsGamePaused=false`였다(용병 고용 창과 다르다) |

- 화면의 무늬·색·이름 조작을 사람이 눌렀을 때의 동작은 확인하지 않았다.

## 게임 안 오버레이 창 — DX12 후킹 + Dear ImGui (2026-10-01, 스파이크)
질문: 패널 기능을 게임 화면 안의 창으로 옮길 수 있는가. 버리는 DLL(레포 밖)을 Lab의 `package.loadlib`로 게임에 올려 쟀다. 게임은 UE 5.5, D3D12(SM6), 전체 창 모드 1920×1080, GPU RTX 2080 Ti.

### 방법
- 더미 창·장치·스왑체인을 만들어 가상 함수 표 주소를 얻고(kiero 방식) MinHook으로 `IDXGISwapChain::Present`(8), `ResizeBuffers`(13), `ID3D12CommandQueue::ExecuteCommandLists`(10)를 후킹했다.
- `Present` 안에서 Dear ImGui(1.93 WIP, win32 + dx12 백엔드)를 초기화하고, 백버퍼를 `PRESENT → RENDER_TARGET → PRESENT`로 전환하며 그 위에 그렸다. 창 프로시저는 `SetWindowLongPtr`로 바꿔 입력을 ImGui에 넘겼다.

### 실측
- **게임 화면 안에 창이 그려진다.** 게임 창만 캡처(`PrintWindow`, `PW_RENDERFULLCONTENT`)해 확인했다. 1만 프레임 넘게 그리는 동안 튕기지 않았다.
- **직접(DIRECT) 명령 큐가 두 개다.** 처음 본 직접 큐로 그리면 첫 프레임 직후 게임이 `GPU Crash dump Triggered`로 종료된다(2회 재현). `swapchain->GetDevice(IID_ID3D12CommandQueue)`는 `E_NOINTERFACE`(0x80004002)였다. **`Present`를 부르는 스레드에서 `ExecuteCommandLists`가 불린 직접 큐**를 고르자 안정됐다.
- 스왑체인: 백버퍼 3개, 형식 24(`DXGI_FORMAT_R10G10B10A2_UNORM`), `Present`와 그 큐의 실행은 같은 스레드에서 일어난다.
- 한글: `C:\Windows\Fonts\malgun.ttf`를 `GetGlyphRangesKorean()`으로 올리면 한글이 표시된다.
- 토글: 창 프로시저에서 `WM_KEYDOWN`/`VK_INSERT`를 가로채 열고 닫았다.
- 해상도·창 모드 변경(`GameUserSettings:SetScreenResolution` + `ApplyResolutionSettings`, 1600×900 창 → 1920×1080 전체 창): `ResizeBuffers`가 불렸고(백버퍼·ImGui 장치 객체를 풀었다가 다시 만듦) 튕기지 않았으며 창이 계속 그려졌다.
- 마우스·키보드: 사용자가 직접 확인했다 — 탭·체크박스·슬라이더·버튼 조작, 창 드래그, Insert 토글, 창 위 입력이 게임에 새지 않음. 이상 없음.
- 게임 창이 뒤에 있을 때 `PostMessage`로 보낸 마우스 메시지는 ImGui에 먹히지 않는다(백엔드의 `TrackMouseEvent` → `WM_MOUSELEAVE`로 위치가 지워진다). 자동 검증은 키 메시지와 창 캡처까지만 된다.

### 구현할 때 지킬 것
- 그리는 큐는 `Present` 스레드의 직접 큐로 고른다. 큐를 못 찾으면 그리지 않는다.
- 창 프로시저(게임 스레드)와 `Present`(RHI 스레드)는 다른 스레드다. 스파이크는 ImGui 입력을 창 프로시저에서 바로 넣었다(경합 가능). 구현은 입력 메시지를 큐에 모아 `Present` 스레드에서 넣어야 한다.
  - 정정(2026-10-01): 큐에 모았다가 넘기는 방식은 쓰지 않는다. 백엔드가 부르는 캡처·키 상태 함수는 창을 가진 스레드에서 불러야 한다. 두 스레드가 재진입 잠금 하나 아래에서 ImGui를 부르게 한다(아래 "계획 코드 사전 검증").
- UE4SS의 자체 GUI는 별도 창으로 뜬다(게임 화면 안이 아니다).

## 게임 안 오버레이 창 — 계획 코드 사전 검증 (2026-10-01, saveGame_8)
구현 계획(`docs/superpowers/plans/2026-10-01-ingame-overlay-plan-a.md`)에 넣을 코드를 레포 밖에서 빌드해 Lab의 `package.loadlib`로 게임 도중에 올려 쟀다(Dear ImGui v1.92.9b). 게임 시작 때 올리는 경로는 재지 않았다.

### 실측
- `overlay_status.json`: 올린 지 8초 뒤에 읽었을 때 `state = ready`, `font = malgun`이었고 `frames`가 계속 늘었다(약 6분 동안 20786).
- 게임 창 캡처로 탭 4개(영주, 건설, 업그레이드, 상태)를 확인했다. 영주 탭의 목표는 `control.json`의 `features.lord`(150000 / 20000 / 50000), "현재"는 `status.json`의 `lord`(148500 / 27910 / 50000)와 같았고 맨 위 줄은 "● 적용됨"이었다.
- `overlay.json`의 `devTab`을 밖에서 고치고 3초 뒤에 찍은 캡처에서 그 탭이 열려 있었다.
- Insert 키 메시지(`PostMessage`)로 `visible`이 `false` → `true`로 바뀌었다.
- `control.json`을 밖에서 고쳐 `seq`를 133으로 올리고 국고 목표를 150001로 바꾸자 3초 뒤 화면의 목표가 150001이 됐다(패널과 함께 쓰는 경로).
- 해상도·창 모드 변경(1600×900 창 → 1920×1080 전체 창) 뒤에도 `state = ready`였고 `frames`가 늘었다.

### 튕김: 창 프로시저의 재진입과 재진입이 안 되는 잠금
- 16:17:37 게임이 종료됐다. 이벤트 로그(Application Error 1000): `ManorLords-Win64-Shipping.exe`, 모듈 `KERNELBASE.dll`, 예외 코드 `0xc000041d`(창 프로시저 등 사용자 콜백 안에서 처리되지 않은 예외), 오프셋 `0xc483a`. 게임의 `Saved/Crashes`에는 새 폴더가 없었다. `status.json`은 16:17:19에, 오버레이의 `overlay_status.json`은 16:17:36에 마지막으로 쓰였다.
- 당시 코드는 창 프로시저(게임 스레드)와 `Present`(RHI 스레드)가 `std::mutex` 하나 아래에서 ImGui를 불렀다.
- 레포 밖 작은 프로그램으로 잰 것(이 레포의 네이티브 빌드와 같은 MSVC 도구, `/MT`):
  - 같은 스레드가 `std::mutex`를 두 번 잠그면 `std::system_error`(`resource deadlock would occur`)를 던진다.
  - 창 프로시저가 잠금을 쥔 채 `ReleaseCapture`를 부르면 Windows가 같은 스레드에 `WM_CAPTURECHANGED`를 보내 창 프로시저가 다시 불린다. 거기서 같은 잠금을 다시 잡으면 예외가 나고 프로세스가 끝난다(이 프로그램에서는 종료 코드 `0xC0000409`).
- ImGui의 Win32 백엔드는 `WM_LBUTTONDOWN`에서 `SetCapture`, 버튼을 뗄 때 `ReleaseCapture`를 부른다(`imgui_impl_win32.cpp`). 숨은 창에 후킹한 창 프로시저를 걸고 `WM_LBUTTONDOWN`/`WM_LBUTTONUP`을 보내는 테스트(`overlay_input_survives_reentrant_window_messages`)는 당시 코드에서 테스트 프로세스를 `0xC0000409`로 끝냈고, 잠금을 `std::recursive_mutex`로 바꾸자 통과했다.
- 16:17에 게임 창에 실제로 마우스 버튼 입력이 있었는지는 확인하지 못했다(추정). 예외 코드가 테스트(`0xC0000409`)와 게임(`0xc000041d`)에서 다른 이유도 확인하지 않았다.
- 고친 코드(재진입 잠금, 입력 경로의 예외 보호, 다른 스레드가 보낸 메시지는 ImGui에 넘기지 않기)는 게임에서 아직 돌리지 않았다. 계획 A의 Task 6에서 확인한다(아래 "계획 A 구현 검증").

### 구현할 때 지킬 것
- 창 프로시저는 같은 스레드에서 다시 불린다. 그 안에서 잡는 잠금은 재진입 잠금이어야 한다.
- 후킹한 창 프로시저 밖으로 예외를 내보내지 않는다. Windows가 게임을 끝낸다.

## 게임 안 오버레이 창 — 계획 A 구현 검증 (2026-10-01, saveGame_8)
`feat/overlay`의 빌드를 `deploy.ps1`로 배포하고 게임을 여섯 번 켜서 쟀다(매번 저장 없이 종료). 화면은 `tools/capture-game.ps1`로 게임 창만 찍었다. 17:30~17:50 사이 이벤트 로그에 게임의 Application Error는 없었고 `Saved/Crashes`에 새 폴더도 없었다.

### 올리기와 그리기
- 모드가 게임 시작 때 DLL을 올린다. `UE4SS.log`: `native: loaded`, 0.6초 뒤 `overlay: loaded`. `status.json`의 `overlay`는 `loaded = true`, `state = ready`, `stale = false`.
- Steam으로 게임을 켠 지 15초 뒤 `overlay_status.json`이 `ready`였다(`frames` 40, `font = malgun`). 세이브를 불러온 뒤에도 `frames`가 계속 늘었다.
- 닫힌 채로 시작하면 화면 왼쪽 위에 "Insert: MLToybox" 안내가 뜨고 10초 뒤 캡처에서는 없었다. 안내가 뜬 때는 게임이 아직 검은 시작 화면일 때였다(메인 메뉴가 나오기 전).

### 화면
- 탭 순서는 영주, 건설, 업그레이드, 상태. 영주 탭의 목표(150000 / 20000 / 50000)와 체크는 `control.json`의 `features.lord`, "현재"(148500 / 27910 / 50000)는 `status.json`의 `lord`와 같았다. 건설 탭의 체크 6개와 업그레이드 탭의 체크도 `control.json`과 같았다.
- 상태 탭: heartbeat, inGame, appliedSeq / sent, bridgeError, 기능 7줄, 네이티브 4줄, 오버레이(빌드 날짜, 글꼴 맑은 고딕, 여닫는 키, 글자 배율). 열이 맞게 그려졌다.
- 메인 메뉴: 맨 위 줄 "● 메인 메뉴", "지금 설정" 버튼이 흐리게 보이고 "현재: -", 아래에 "게임에 들어가면 표시됩니다".

### 동작
- Insert 키 메시지: `visible`이 `true` → `false` → `true`.
- 창이 열린 상태에서 마우스 버튼 메시지(`capture-game.ps1 -Click 400,500`)를 5번, 다른 실행에서 3번 보냈다. 게임 프로세스가 살아 있었고 `state = ready`, `frames`가 계속 늘었다. 고치기 전 DLL에 같은 메시지를 보내 튕기는지는 재지 않았다.
- `control.json`을 밖에서 고쳐 `seq`를 133으로 올리고 국고 목표를 123456으로 바꾸자 3초 뒤 캡처에 123456과 "● 적용됨"이 보였다(`appliedSeq = 133`).
- 창을 (1250, 300)에 두고 해상도를 1280×720 창 모드로 줄이자 창이 (640, 0)으로 옮겨져 화면 안에 다 들어왔고 `overlay.json`에도 그 위치가 적혔다. 1920×1080 전체 창으로 되돌린 뒤에도 `state = ready`였다.

### 안전장치
- 배포된 `config.lua`의 `overlay = false`: `status.overlay`가 `loaded = false`, `error = "disabled in config"`, `stale = true`이고 `state`가 없었다(지난 실행의 `overlay_status.json`에는 `ready`가 남아 있었다). `features.build.active = true`, `native.loaded = true`, 화면에 오버레이 창 없음.
- 배포된 `mltoybox_overlay.dll`을 다른 이름으로 바꿈: `error = "not deployed"`, `native.loaded = true`, `features.build.active = true`.

### 확인하지 못한 것
- 실제 마우스·키보드 조작(체크, 숫자 입력, "지금 설정", 창 끌기와 크기 바꾸기, 창 위 입력이 게임에 새지 않는지, 여닫는 키와 글자 배율 바꾸기). 게임 창이 앞에 있어야 해서 사용자 확인 항목이다.
- 화면에서 값을 바꿨을 때 `control.json`에 저장되는 경로는 단위 테스트(`overlay_session_*`, `overlay_bridge_*`)로만 확인했다.
- 프레임 생성(FSR·DLSS FG), HDR, 전체 화면 전용 모드, 모니터 사이 이동, 오래 켜 둔 상태.

### 코드 리뷰 뒤 고친 것 (2026-10-01)
병합 전 브랜치 전체 리뷰에서 나온 네 가지를 테스트로 재현하고 고쳤다. 고친 빌드를 다시 배포해 게임에서 한 번 더 쟀다.

- **오버레이가 스스로 꺼진 뒤의 `ResizeBuffers`**: 백버퍼 참조를 프레임 사이에 들고 있었고, 꺼진 뒤에는 놓지 않았다. 실제 D3D12 스왑체인으로 시험하는 테스트(`overlay_render_draws_and_never_blocks_the_games_resize`, 화면 밖 창, 게임 없음)에서 고치기 전 코드는 꺼진 뒤의 `ResizeBuffers`가 실패했다(정상일 때의 `ResizeBuffers`와 그리기는 통과). 이제 백버퍼는 그릴 때 얻고 그 프레임 안에서 놓는다. 게임이 이 실패를 어떻게 처리하는지는 재지 않았다.
- **연달아 누른 일회성 명령**: 명령을 한 번만 싣고 비웠기 때문에, 모드가 읽기 전(1초 주기)에 나간 다음 저장에는 앞의 명령이 없었다. 테스트(`overlay_session_commands_ride_every_save_until_the_mod_reports_them`)에서 고치기 전 코드는 두 번째 저장의 `commands`가 1개였다. 이제 모드가 `status.json`의 `commands`에 그 `id`를 올릴 때까지(또는 60초가 지날 때까지) 저장할 때마다 다시 싣는다. 모드는 같은 `id`를 한 번만 실행한다(`core/commands.lua`의 `done`). 게임에서 "지금 설정"을 연달아 누르는 확인은 사용자 확인 항목이다.
- **명령 할당자 재사용**: 백버퍼 수만큼의 할당자를 GPU 완료 확인 없이 다시 썼다. 펜스로 완료를 확인하고, 다 쓴 할당자가 없으면 늘리고(최대 8개), 그래도 없으면 그 프레임은 그리지 않는다(`overlay_frame_gate_*`). 실제로 GPU가 밀린 상태를 만들어 재지는 않았다.
- **패널과 함께 쓸 때의 안내**: 패널은 켤 때와 "되돌리기"에서만 파일을 읽고 "적용"·명령 버튼에서 모든 탭의 화면 값을 다시 쓴다(`MainForm.cs`의 `Apply`). README에 주의를 넣었다.

고친 빌드의 게임 확인(saveGame_8, 한 번 실행): `status.overlay`가 `loaded = true`, `state = ready`. Insert 메시지로 `visible`이 `false`가 됐다가 돌아왔다. 창이 열린 채 마우스 버튼 메시지 5번 뒤 프로세스가 살아 있었다. 창을 (1250, 300)에 두고 1280×720 창 모드로 줄이자 창이 화면 안으로 옮겨져 그려졌고, 1920×1080 전체 창으로 되돌린 뒤에도 그려졌다(캡처). `frames`는 3035에서 4086으로 늘었고 이벤트 로그에 게임의 Application Error는 없었다.

리뷰에서 나온 나머지(작은 것 16건)는 고치지 않고 계획 B의 입력으로 남겼다. 그 가운데 게임에서 재야 알 수 있는 것: 창 위에서 게임이 직접 읽는 입력(화면 가장자리 스크롤 등)이 새는지, 마우스 커서 모양, 창이 열린 채 버튼을 누른 상태로 닫았을 때의 입력 상태.

## 게임 안 오버레이 창 — 계획 A 사용자 확인과 계획 B 코드 사전 검증 (2026-10-01, saveGame_8)

### 계획 A의 사용자 확인
"계획 A 구현 검증"에서 확인하지 못했던 실제 마우스·키보드 조작(체크, 숫자 입력, "지금 설정", 창 끌기와 크기 바꾸기, 창 위 입력이 게임에 새지 않는지, 여닫는 키와 글자 배율)을 사용자가 게임에서 해 보고 "이상 없음"이라고 알렸다. 항목별 결과를 따로 받지는 않았다. 그 뒤 `control.json`은 `seq` 144였고 영주 목표가 300000으로 바뀌어 있었으며 `setLord` 명령이 하나 실려 있었다. 파일 수정 시각(18:58)이 아래 사전 검증 실행(19:35)보다 앞이라 그 실행이 바꾼 것은 아니다. 오버레이와 패널 가운데 어느 쪽에서 바꿨는지는 확인하지 않았다.

### 계획 B 코드 사전 검증
계획 B의 코드를 레포 밖 사본에서 태스크 순서대로 만들고(네이티브 테스트 127개 통과, Lua 스펙 통과), 그 빌드의 오버레이 DLL만 배포해 게임을 한 번 켰다(19:35, 저장 없이 종료, 끝난 뒤 `develop`의 빌드를 다시 배포). 이벤트 로그에 게임의 Application Error는 없었다.

- **안내**: 닫힌 채로 시작해 세이브를 불러온 직후(1.5초 뒤) 캡처의 왼쪽 위에 "Insert: MLToybox"가 있었다. 그때 화면은 아직 검은 전환 화면이었다. 맵이 보인 뒤에도 안내가 남아 있는지는 찍지 않았다.
- **탭 막대**: 자원, 영주, 건설, 업그레이드, 군사, 용병, 인구, 상태 순서.
- **자원 탭**: "자원 목표값 유지" 체크, 주기 2, "공통 (모든 내 영지, 현재=합계)", 채울 값 500, 표의 줄이 이름순(Ale, Barley, Beer, …)이고 현재 값(Ale 1500, Charcoal 1767, Firewood 1738)과 목표 500이 보였다. "현재" 열이 좁았다. 열 너비 비율은 이 실행 뒤에 고쳤고 게임에서 다시 보지 않았다.
- **군사 탭**: 체크 5개가 모두 켜짐(`features.military`와 같음), 병종 "민병대 - 창", 개수 1, 위치 "Mandlach (gold)", "해제된 생성 분대: 0개"와 흐린 "재구성", "꾸밀 수 있는 분대가 없습니다".
- **용병 탭**: 체크 3개, 표에 "검사대 / 용병 - 보병 × 4 / 1,000 / Haderwand (nus) / battle_brothers"(사용 체크됨), 편집 영역(합계 0 / 10, 고용비 1000, "내 첫 영지", "칸의 것 그대로"), 아래에 "고용 창: 검사대(커스텀) 10,000,000, brigands_small 15, crazy_goose 90", "고용 중: 내 용병단 1개, AI 1개 · 맵을 불러온 뒤 환급 0".
- **인구 탭**: "공통 (모든 내 영지)", 배율 1, 최소 가족 수 0, 들일 가족 수 3, "현재(모든 내 영지 합계): 가족 155 · 인구 477 · 집 없는 가족 0 · 빈 자리 49 · 미배치 가족 65".
- 창이 열린 채 마우스 버튼 메시지를 3번 보낸 뒤 프로세스가 살아 있었고 `overlay_status.json`은 `state = ready`, `visible = true`, `frames` 1856, `font = malgun`이었다.

### 확인하지 못한 것
- 새 탭의 마우스 조작(체크, 표 편집, 분대 생성, 용병단 등록·삭제, "사용" 체크), 한글 조합, 복사·붙여넣기. 게임 창이 앞에 있어야 해서 사용자 확인 항목이다.
- 게임이 평소에 창의 IME를 꺼 두는지. 오버레이 코드는 꺼져 있을 때만 켜고 되돌리도록 만들었고(단위 테스트), 게임에서 재지는 않았다.
- 화면 스레드가 클립보드를 비울 때 실제로 멈추는지. 가능성 때문에 작업 스레드로 옮겼을 뿐 재현하지 않았다.
- "사용"을 끈 용병단이 고용 창에서 빠지는 것. 모드의 계획 단계는 `enabled == true`인 정의만 본다(`merc_plan.lua`). 게임에서 재는 것은 계획 B의 Task 8에 넣었다.

## 게임 안 오버레이 창 — 계획 B 구현 검증 (2026-10-02, saveGame_8)
`feat/overlay-tabs`의 빌드를 태스크마다 `deploy.ps1`로 배포하고 게임을 네 번 켜서 쟀다(08:05, 08:27, 08:30, 08:33. 매번 저장 없이 종료). 화면은 `tools/capture-game.ps1`로 게임 창만 찍었다. 네 번 모두 이벤트 로그에 게임의 Application Error가 없었고, 끝난 뒤 `control.json`은 시작 전과 같았다(해시 비교. 네 번째 실행은 밖에서 고친 뒤 사본으로 되돌렸다). 네이티브 테스트는 127개가 통과했다.

### 안내
- 닫힌 채로 시작해 세이브를 불러왔다. 1.5초 뒤 캡처(전환 중이라 화면 위쪽이 검었다)와 그다음 캡처(맵이 보이는 화면) 모두 왼쪽 위에 "Insert: MLToybox"가 있었다. 13초쯤 뒤 캡처에는 없었다.
- 사전 검증에서는 맵이 보인 뒤의 캡처를 찍지 않았었다. 이번에 맵 위에서 보이는 것을 확인했다.

### 입력과 해상도 (Task 1의 빌드)
- Insert 키 메시지로 `visible`이 `true`가 됐다. 창이 열린 채 마우스 버튼 메시지를 5번 보낸 뒤 `state = ready`, `frames`가 892에서 1129로 늘었다.
- 왼쪽 버튼을 누른 메시지만 보내고(뗀 메시지 없이) Insert로 닫았다가 다시 열었다. `visible`이 `false`, `true`로 바뀌었고 `state = ready`였다. 실제 마우스에서 창이 붙어 다니는지는 이 방법으로 알 수 없다(사용자 확인 항목).
- 창을 (1250, 300)에 두고 1280×720 창 모드로 줄이자 창이 (640, 0)으로 옮겨져 화면 안에 그려졌다. 1920×1080 전체 창으로 되돌린 뒤 `state = ready`, `frames` 1813.

### 탭 화면
- **탭 막대**: 자원, 영주, 건설, 업그레이드, 군사, 용병, 인구, 상태.
- **군사**: 체크 5개 켜짐(`features.military`가 모두 `true`), 병종 "민병대 - 창", 개수 1, 위치 "Mandlach (gold)"(`playerRegions`의 첫 영지), "해제된 생성 분대: 0개"와 흐린 "재구성"(`spawn.disbanded = 0`), "꾸밀 수 있는 분대가 없습니다"(`retinue.squads`가 빔).
- **인구**: "공통 (모든 내 영지)", 배율 1, 최소 가족 수 0, 들일 가족 수 3. "현재(모든 내 영지 합계): 가족 155 · 인구 477 · 집 없는 가족 0 · 빈 자리 49 · 미배치 가족 65"가 `status.population`과 같았다.
- **자원**: 체크 켜짐, 주기 2, "공통 (모든 내 영지, 현재=합계)", 채울 값 500. 표의 줄이 이름순이고 Ale 1500 / 500, Charcoal 1767 / 500, Firewood 1738 / 500으로 `status.resources`, `features.resources.targets`와 같았다. 사전 검증에서 좁았던 "현재" 열은 너비 비율을 고친 뒤 넉넉하게 보였다. 이 세이브에 없는 영지(`sel`, `hof`)의 영지 목표는 `control.json`에 그대로 남아 있었다. 다만 이 실행에서 오버레이는 파일을 쓰지 않았다. 값을 바꿔 저장한 뒤에도 남는지는 게임에서 재지 않았고, 읽은 영지 키를 모두 다시 쓰는 것은 단위 테스트(`overlay_features_population_and_resources_round_trip`)로만 확인했다.
- **용병**: 체크 3개 켜짐. 표의 줄은 "사용 체크 / 검사대 / 용병 - 보병 × 4 / 1,000 / Haderwand (nus) / battle_brothers". 편집 영역은 합계 0 / 10, 고용비 1000, "내 첫 영지", "칸의 것 그대로". 아래 줄 "고용 창: 검사대(커스텀) 10,000,000, crazy_goose 90, huntsmen 250", "고용 중: 내 용병단 1개, AI 1개 · 맵을 불러온 뒤 환급 0"이 `status.mercenaries`와 같았다.
- 탭마다 창이 열린 채 마우스 버튼 메시지를 3번 보낸 뒤 `state = ready`였다.

### "사용"을 끈 용병단 (R7)
- `control.json`을 밖에서 고쳐 `seq`를 145로 올리고 "검사대"의 `enabled`를 `false`로 바꿨다. 오버레이에서 "사용" 체크를 끄는 것과 같은 설정 변경이다.
- 고치기 전 `status.mercenaries.slots`는 검사대, crazy_goose, huntsmen이었다. 2초 뒤 첫 확인에서 crazy_goose, huntsmen, greencaps였다(`appliedSeq = 145`, `note` 없음, `skipped` 없음).
- 그 뒤 캡처에서 표의 "사용" 체크가 꺼져 있었고 "고용 창:" 줄에 순정 용병단 셋만 있었다. 오버레이가 밖에서 바뀐 설정을 다시 읽었다.
- 게임의 고용 창 화면을 열어 카드가 빠진 것을 눈으로 보지는 않았다.

### 확인하지 못한 것
- 새 탭의 마우스 조작(체크, 표 편집, "모두 이 값으로", 분대 생성, 재구성, 꾸미기 열기, 가족 추가, 용병단 등록·고치기·삭제, 네 번째 "사용" 체크). 화면에서 바꾼 값이 저장되는 경로는 단위 테스트로만 확인했다.
- 한글 조합(글자가 쳐지는지, 두 번 들어가지 않는지, 조합 글자의 위치), 복사와 붙여넣기. 게임이 평소에 창의 IME를 꺼 두는지도 재지 않았다.
- 버튼을 누른 채 닫았다 열었을 때 실제 마우스에서 창이 붙어 다니는지.
- 프레임 생성(FSR·DLSS FG), HDR, 전체 화면 전용 모드, 모니터 사이 이동.

### 코드 리뷰 뒤 고친 것 (2026-10-02)
병합 전 브랜치 전체 리뷰는 게임을 튕기거나 멈추게 하는 경로를 찾지 못했다. 작은 지적 15건 가운데 둘을 테스트로 재현하고 고쳤다.

- **밖에서 목록이 바뀐 뒤의 용병단 편집**: 편집 영역은 고치던 용병단을 자리 번호로 기억했다. 패널이나 손 편집으로 앞의 용병단이 지워지면 그 번호가 다른 용병단을 가리켜, "삭제"나 "등록"이 엉뚱한 용병단을 지우거나 덮을 수 있었다. 이제 실었던 내용으로 다시 찾고(`locateCompany`), 없어졌거나 내용이 바뀌었으면 편집 대상에서 풀고 알린다. 테스트 `overlay_merc_rules_editor_follows_its_company_when_the_list_changes_outside`. 탭에서 이 경로를 타려면 줄을 눌러야 해서 게임에서 재현하지는 않았다.
- **스스로 꺼질 때 켜 둔 IME**: 글자 칸에 커서가 있어 IME를 켜 둔 채 오버레이가 꺼지면(예외, 장치 제거) 그 뒤로는 아무것도 가로채지 않아 IME가 켜진 채 남았다. 테스트(`overlay_input_restores_the_ime_when_the_overlay_disables_itself`)에서 고치기 전 코드는 꺼진 뒤에도 창에 IME 컨텍스트가 붙어 있었다. 이제 꺼진 뒤 첫 메시지에서 되돌린다.

고친 빌드의 게임 확인(saveGame_8, 09:05, 한 번 실행): 용병 탭 화면이 앞의 실행과 같았고 편집 영역에 잘못된 안내가 뜨지 않았다. "검사대"의 `enabled`를 밖에서 끄자 2초 뒤 첫 확인에서 `slots`가 검사대, wayward_sons, crazy_goose에서 wayward_sons, crazy_goose, greencaps로 바뀌었다. 마우스 버튼 메시지 3번 뒤 `state = ready`(`frames` 3238), Application Error 없음. 네이티브 테스트는 129개가 통과했다.

고치지 않고 남긴 것(원인을 재지 않았거나 드문 경우): 오버레이의 글자 칸에서 게임의 글자 칸으로 바로 넘어갈 때 게임이 켠 IME까지 꺼질 가능성, 붙여넣기 때 화면 스레드가 클립보드를 읽는 것, 클립보드가 바쁠 때 복사한 글을 다시 쓰지 않는 것, 마우스 옆 버튼을 누른 채 닫는 경우와 다시 연 뒤 마우스를 움직이기 전의 첫 클릭, 모드 heartbeat가 끊겼다 돌아올 때의 안내, "재구성"을 연달아 누를 때의 중복 명령, `enabled` 키가 없는 손 편집 정의.

## 게임 안 오버레이 창 — 사용자 확인에서 나온 두 문제 (2026-10-02)
사용자가 계획 B의 확인 항목을 게임에서 해 보고 알렸다: 한글 입력이 되지 않는다. 군사 탭에서 분대 하나를 해제한 뒤 "재구성"을 여러 번 누르면 누른 횟수만큼 새로 생긴다. 그 밖은 정상.

### "재구성" 연타 — 원인과 수정
- 원인(코드로 확인, Lua 스펙으로 재현): `spawn_squads.lua`의 `reform`은 호출될 때마다 지금 배열에 있는 유령 분대 전부로 새 분대를 만든다. 유령은 틱마다 하나씩 지워지므로(`removeSquad`는 요청만 등록한다), 정리가 끝나기 전에 온 명령은 같은 유령을 다시 센다. 오버레이의 "재구성" 단추는 다음 `status.json`(1초)까지 눌리는 상태라 연달아 누르면 명령이 여러 개 간다. 패널도 같았다.
- 수정: 제거 대기 중인 유령이 있거나, 제거를 요청했지만 게임이 아직 배열을 줄이지 않았으면 `reform`이 `reform already in progress`로 거절한다. 스펙 `reform_is_refused_while_earlier_ghosts_are_still_being_removed`는 고치기 전 코드에서 두 번째 호출이 `ok = true`였다.
- 게임에서는 다시 재지 않았다(유령 분대를 만들려면 게임 화면에서 분대를 해제해야 한다). 사용자 확인 항목이다.

### 한글 입력 — 실측
진단 코드를 넣은 빌드로 게임을 세 번 켜서 창 스레드에서 쟀다(saveGame_8, 저장 없이 종료, 진단 코드는 커밋하지 않았다). 글자 칸에 커서가 있는 상태는 진단용 설정으로 흉내 냈다. 세 번 모두 게임 창이 앞에 있지 않았다(`GetForegroundWindow`가 게임 창이 아님, TSF의 `IsThreadFocus`가 거짓).

- 게임 창은 평소에 IMM 입력 컨텍스트가 없다(`ImmGetContext`가 0). 계획 B에서 추정으로 적었던 것이 확인됐다. 자판 배열은 한국어(`0x4120412`)였다.
- 게임의 창 스레드에는 TSF 스레드 관리자가 있다(`TF_GetThreadMgr`가 값을 돌려준다). 포커스 문서 관리자가 있고 그 안에 컨텍스트가 하나 있다.
- 그 문서 관리자는 게임 창에 묶여 있다: `AssociateFocus(창, null)`이 돌려준 이전 값이 포커스 문서와 같았다.
- 계획 B의 방식(`ImmAssociateContextEx(IACE_DEFAULT)`)을 적용하면 IMM 컨텍스트는 붙는다(열림 상태 0, 변환 모드 0). TSF의 포커스 문서는 그대로였다.
- TSF 연결을 먼저 풀고 IMM을 붙여도, IMM을 붙인 뒤 TSF 연결을 풀어도 포커스 문서는 그대로였다.
- 게임 창의 자식 창을 만들어 Win32 포커스를 줘도(자식 창에는 IMM 컨텍스트가 있었다) TSF의 포커스 문서는 그대로였다.
- 재지 못한 것: 게임 창이 앞에 있을 때의 같은 값, 실제 글쇠를 눌렀을 때 오는 메시지(한/영 글쇠가 `WM_KEYDOWN`으로 오는지, 한글 모드에서 글쇠가 `VK_PROCESSKEY`로 바뀌는지). 그래서 "TSF 포커스가 게임의 문서에 있어서 조합이 오지 않는다"는 것은 위 값들과 사용자의 증상에 맞는 설명이지, 글쇠 기록으로 확인한 것은 아니다.

### 한글 입력 — 고친 방식
- 시스템 입력기에 기대지 않기로 했다. 창의 IMM을 다시 켜는 것으로는 안 됐고, 되게 하려면 게임의 TSF 포커스를 가로채야 하는데 게임 자체의 글자 입력을 건드리게 되고 자동으로 시험할 수도 없다.
- 오버레이가 두벌식 조합기를 갖는다(`native/overlay/core/hangul.*`). 게임 창의 IME는 건드리지 않는다. 글쇠는 IME 없이도 `WM_CHAR`로 들어오므로(숫자 칸이 계획 A부터 그렇게 동작했고, 사용자 확인에서 용병단 등록도 정상이었다) 영문자를 자모로 조합한다. 한/영은 한/영 글쇠나 이름 칸 옆의 단추로 바꾼다.
- 단위 테스트: 조합(받침 넘기기, 겹받침, 겹모음, 쌍자음, 지우기), 글자 칸 연결(커서 이동·붙여넣기 뒤 조합 끝내기, 바이트 제한), 그리고 글자 칸 위젯을 그래픽 없이 실제 ImGui 프레임으로 돌려 "dkssud" → "안녕", 지우기 글쇠, 윗글쇠를 확인한다. 창 프로시저 테스트는 글자 칸에 커서가 있을 때만 한/영 글쇠를 오버레이가 받는 것과 창의 IME가 꺼진 채로 남는 것을 본다.
- 확인하지 못한 것: 게임에서의 실제 타자. 특히 한/영 글쇠가 게임 창에 `WM_KEYDOWN`으로 오는지(오지 않으면 단추로만 바꿀 수 있다), Windows 입력기가 한글 상태일 때 영문 글쇠가 그대로 오는지. 사용자 확인 때의 글쇠 메시지를 보려고 개발용 설정 `inputLog`(`bridge/overlay.json`)를 넣었다. 켜면 창이 열려 있는 동안의 글쇠 메시지가 `bridge/overlay_input.log`에 적힌다.
