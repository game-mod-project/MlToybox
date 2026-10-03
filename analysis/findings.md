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

### 고친 뒤의 사용자 확인 (2026-10-02)
사용자가 고친 빌드로 한글 이름 입력과 "재구성" 연타를 게임에서 해 보고 "이상 없음"이라고 알렸다. 그때 `inputLog`로 적힌 글쇠 메시지 157줄을 종류별로 셌다(친 내용은 보지 않았다). 센 뒤 `inputLog`를 끄고 기록 파일을 지웠다.

- 한/영 글쇠는 글자 칸에 커서가 있을 때 `WM_KEYDOWN`(`w = 0x15`)으로 한 번 왔다. 앞에서 확인하지 못했던 것이다. 한/영 글쇠로 조합기를 켜고 끌 수 있다.
- `VK_PROCESSKEY`(`w = 0xE5`)로 바뀐 글쇠는 없었다. `WM_CHAR` 54개 가운데 영문자가 47개였고 `0x7F`를 넘는 글자는 없었다. Windows 입력기는 글쇠를 가로채지 않았고, 한글은 오버레이의 조합기가 만든 것이다.
- IME 메시지는 `WM_IME_SETCONTEXT` 4개와 `WM_IME_NOTIFY` 4개뿐이었다. 조합 메시지(`WM_IME_STARTCOMPOSITION` 등)는 없었다.
- 확인 뒤 `control.json`의 용병단은 검사대, B, A, AA였다. 한글 이름은 고치기 전부터 있던 "검사대"뿐이라, 조합기로 친 한글이 저장된 것을 파일로 확인하지는 못했다(처음에 이것을 한글 입력의 증거처럼 적었다가 바로잡았다). 한글이 쳐진다는 것은 사용자의 보고와 위의 글쇠 기록에 근거한다.
- "재구성"은 사용자의 보고("이상 없음") 말고 따로 잰 값이 없다.

## 게임 안 오버레이 창 — 리뷰에서 미룬 항목 정리 (2026-10-02)
계획 B의 최종 리뷰에서 미뤄 둔 작은 항목 10건을 다뤘다(나머지 둘 가운데 IME를 끌 때의 문제는 조합기로 바꾸며 없어졌고, "재구성" 연타는 사용자 확인 뒤 고쳤다). 고치기 전 코드에서 실패하는 테스트를 먼저 썼다. 네이티브 테스트는 150개, Lua 스펙은 모두 통과한다.

### 고친 것과 확인한 방법
- **`enabled` 키가 없는 용병단**: 패널과 오버레이는 "사용"으로 읽는데 모드(`merc_plan.lua`)는 `enabled == true`만 썼다. 모드가 `enabled ~= false`로 보게 했다. 게임(saveGame_8): 키가 없는 정의 "키없음"을 설정 파일에 넣자 2초 뒤 첫 확인에서 `status.mercenaries.slots`가 검사대, 키없음, crazy_goose였고(`skipped` 없음) 오버레이 표의 "사용"도 켜져 보였다.
- **용병단 항목의 모르는 키**: `setMercenaries`가 항목을 새로 만들어 썼다. 같은 이름의 예전 항목에서 시작하도록 고쳤다(테스트 `overlay_features_mercenary_items_keep_their_unknown_keys`). 게임에서 오버레이가 저장하는 경로로는 재지 않았다.
- **클립보드**: 붙여넣기 때 화면 스레드가 두 잠금을 쥔 채 클립보드를 읽었고, 복사는 한 번 실패하면 버렸다. 작업 스레드가 읽고 쓰게 했다(`ui/clipboard_sync`). 가짜 클립보드로 다시 쓰기, 새 복사가 앞의 것을 대신하는 것, 글자 칸에 커서가 있을 때만·바뀌었을 때만 읽는 것을 시험한다. 실제 Windows 클립보드는 읽기 함수가 클립보드를 바꾸지 않는 것만 확인했다(쓰기는 사용자의 클립보드라 테스트하지 않는다). 게임에서 실제 붙여넣기는 재지 않았다.
- **닫을 때와 다시 열 때의 마우스**: 옆 버튼을 누른 채 닫으면 마우스 캡처가 남았고, 다시 연 뒤 마우스를 움직이기 전에는 ImGui가 마우스 위치를 몰랐다(닫을 때 비우기 때문). 옆 버튼도 뗀 것으로 알리고, 열 때 커서가 게임 창 안에 있으면 그 위치를 넣는다. 테스트는 실제 창에 메시지를 보내고 ImGui 프레임을 돌려 위치를 본다(커서 위치는 가짜로 준다). 게임에서는 옆 버튼을 누른 메시지를 보낸 뒤 닫았다 열어도 `state = ready`인 것만 봤다.
- **안내**: 모드의 heartbeat가 끊겼다 돌아오면 맵 안에서도 다시 떴다. 연결이 끊긴 동안에는 끊기기 전의 상태를 기억한다. 게임에서는 세이브를 불러온 직후 안내가 뜨는 것만 다시 확인했다(끊김은 만들지 않았다).
- **창 프로시저 걸기**: `SetWindowLongPtrW`가 돌려준 값(실제로 갈아 끼운 프로시저)이 미리 읽은 값과 다르면 그것을 이어 부른다. 테스트는 없다(다른 프로그램이 그 사이에 끼어드는 것을 만들 수 없다). 걸어 둔 창이 없어졌을 때 다시 거는 경로는 게임에서는 불리지 않는다: 화면 출력 후킹이 처음 그릴 때 한 번만 건다. 그 경로는 창을 여러 번 만드는 테스트가 쓴다.
- **용병 탭**: 이름에 `##`가 있으면 표에서 잘렸다. 게임에서 "A##B"가 그대로 보였다. 줄 전체가 눌리게 했고(게임에서 누르는 것은 재지 못했다), 손으로 쓴 깃발 "Greencaps"처럼 대소문자가 다른 값은 편집 영역에 실을 때 선택지의 이름으로 맞춘다.
- **읽을 수 없는 `control.json`**: 사본 만들기가 실패해도 덮었고, 사본은 시작할 때만 만들었다. 저장할 때마다 확인해 사본을 남긴 뒤에만 덮는다(테스트 `overlay_bridge_save_keeps_a_copy_of_an_unreadable_file_or_does_not_save`).
- **테스트의 빈 곳**: 숫자 칸(Enter와 커서 이동으로 반영, 범위, 빈칸), 이 세이브에 없는 영지의 목표가 고쳐 써도 남는 것, Lua 견본의 한글 40자 이름.

### 바꾸지 않은 것
- 군사 탭의 "징집 조건(집 레벨·훈련) 무시"는 패널처럼 한 줄로 합치지 않았다. 패널의 한 줄 문구는 기본 창 너비(640)에 겨우 들어가고, 사용자가 줄여 쓰는 창(너비 587)에서는 잘린다(글자 수로 어림한 값이다. 재지는 않았다). 같은 문구를 체크와 흐린 줄로 나눠 둔다.

### 확인하지 못한 것
- 실제 마우스·글쇠로 하는 것: 다른 프로그램에서 복사한 글 붙여넣기와 오버레이에서 복사한 글을 다른 프로그램에 붙이기(처리하는 스레드가 바뀌었다), 용병 표의 줄 누르기, 다시 연 직후의 첫 클릭.

### 사용자 확인 (2026-10-02)
위의 "확인하지 못한 것" 네 가지(다른 프로그램에서 복사한 글 붙여넣기, 오버레이에서 복사한 글을 다른 프로그램에 붙이기, 용병 표의 줄 누르기와 "사용" 체크, 다시 연 직후의 첫 클릭)를 사용자가 게임에서 해 보고 "이상 없음"이라고 알렸다. 항목별 값을 따로 재지는 않았다.

## 게임 안 오버레이 창 — 업그레이드 탭을 건설 탭으로 (2026-10-02)
사용자 요청: 업그레이드의 기능이 건물 관련이면 건설 탭으로 합친다.

- 건물 관련인가: 모드의 업그레이드 기능(`features/upgrade.lua`)은 `DT_Upgrades`의 모든 행(비용, 필요한 건물, 필요한 특성, 정착지 레벨·번영도·집 레벨 조건)과 주거지 레벨 조건(`ResidentialRequirementSettings`)을 고친다. `DT_Upgrades`는 건물 설정의 업그레이드 표다(`UBuildingSettings.UpgradeDataTable`, 위 "③ 업그레이드"). 건물 기능이다.
- 바꾼 것: 오버레이의 "업그레이드" 탭을 없애고 그 체크를 건설 탭의 "업그레이드" 구분선 아래로 옮겼다. 설정은 그대로 `features.upgrade`이고 "건설 기능 사용"과 따로 켜고 끈다. 패널은 바꾸지 않았다.
- 테스트(`overlay_tabs_upgrade_lives_in_the_build_tab`): 탭 이름이 7개인 것, 건설 탭을 그래픽 없는 ImGui 프레임으로 그리고 마우스 클릭을 넣었을 때 어느 한 줄이 업그레이드 설정만 바꾸는 것(건설 설정은 그대로).
- 게임(saveGame_8, 한 번 실행, 저장 없이 종료): 탭 막대가 자원, 영주, 건설, 군사, 용병, 인구, 상태였다. 건설 탭에 체크 6개와 "업그레이드" 구분선, "업그레이드 조건·비용·해금 무시" 체크가 켜져 보였고 `control.json`의 `features.upgrade.enabled = true`, `status.features.upgrade.active = true`와 같았다. 마우스 버튼 메시지 3번 뒤 `state = ready`, Application Error 없음.
- 확인하지 못한 것: 게임에서 실제 마우스로 그 체크를 눌렀을 때의 반영(단위 테스트로만 확인).

## 자원 목록과 게임의 품목 표 (2026-10-02, Steam buildid 24905706)
사용자가 영지 창 캡처로 알렸다: 자원 설정에 없는 품목이 있다. 캡처에서 모르타르, 목제 부품, 소시지, 양고기, 치즈, 약초, 철제 부품, 벌꿀주 등이 0이었고 당근 98, 비트 100, 동물 사료 109처럼 목표와 무관한 값이었다.

### 원인
- 모드의 자원 목록(`features/resources_catalog.lua`)은 `EItemType` 열거형에서 손으로 고른 55종이었다. 게임이 쓰는 품목 30종이 빠져 있었고, 게임이 쓰지 않는 품목 7종이 들어 있었다.
- 게임 빌드는 같았다(`appmanifest`의 `buildid` 24905706). 업데이트 때문이 아니다.
- 그때 사용자 게임의 `status.json`에는 `meat = 1000`, `vegetables = 1000`이 있었다. 모드는 영지 창에 없는 옛 통합 품목을 목표까지 채우고 있었다.

### 품목 표 실측 (`tools/lab/items.lua`, saveGame_8)
- 표는 `/Game/NotStronghold/Data/DT_Items.DT_Items`, 행 구조는 `FItem`. 행 이름이 품목 번호(`EItemType` 값)다. 211행.
- 행마다 `ItemCategory`(`EItemCategory`)와 `Subcategory`가 있다. 분류별 행 수: 없음 110, 건설 7, 식량 25, 제작 재료 27, 일용품 8, 가축 10, 군사 11, 과도기 8, 공성 1, 조리법 4.
- 영지 창의 묶음은 이 분류와 같다. 예: 식량 아래 채집한 상품(열매, 버섯), 고기(소시지, 소형 사냥감, 양·염소·돼지·소·닭고기), 물고기(잉어, 장어, 훈제 생선), 채소, 과일, 곡물, 동물 생산물. 목제 부품은 분류가 제작 재료, 하위 분류가 유지보수인데 영지 창에서는 건설 아래에 보인다.
- 지금까지 목록에 있던 7종은 분류가 "없음"이다: `meat`(147), `vegetables`(230, 표의 이름이 `vegetables_DEPREC`), `Pastries`(173), `Hops`(166), `Beer`(167), `dyes`(302), `Candle`(153). 사용자의 캡처에도 없다.
- 전체 표는 `mod/MLToybox/tests/fixtures/dt_items.tsv`에 두었다.

### 고친 것
- 목록을 분류가 건설, 식량, 제작 재료, 일용품, 군사인 78종 전부로 바꿨다. 이름은 열거형 이름 그대로라 이미 저장된 목표의 키(`Timber`, `WheatBread`, `fish` 등)는 같은 품목을 가리킨다. 가축, 과도기, 공성, 조리법은 넣지 않았다.
- 스펙 `resources_catalog_spec.lua`가 목록과 견본 표를 맞춰 본다. 고치기 전 목록에서는 빠진 30종을 이름과 함께 대며 실패했다.
- 오버레이의 자원 표: 모드가 자원 목록을 알려 주면 그 목록으로 줄을 만든다. 목표만 남아 있고 목록에 없는 품목(위 7종)은 줄로 보이지 않는다. 설정의 값은 지우지 않는다. 패널은 바꾸지 않았으므로 그 줄이 남는다.

### 게임 확인 (saveGame_8, 저장 없이 종료)
- 새 목록으로 `status.resourceIds`가 79개(78종과 지역 재화)였고 옛 7종은 없었다.
- 새 품목 30종에 공통 목표 50을 걸자 10초 안에 모두 150이 됐다(영지 3곳 × 50). 걸기 전에는 Rubble 256, Herbs 10 말고 모두 0이었다. Rubble은 이미 150보다 많아 그대로였다. Application Error 없음.
- 오버레이 자원 탭에 AnimalFeed, Beef, Beetroots, Cabbages, Carrots, Cheese, Chevon, Chicken, Eel, Herbs 등이 150 / 50으로 보였다.
- 사용자의 설정에는 옛 품목의 목표가 남아 있다. 줄을 숨기기 전 빌드의 캡처에는 `Beer`, `Candle` 줄이 "현재: -"로 보였고, 숨긴 뒤 빌드의 캡처에는 없었다. 확인 뒤 `control.json`은 사본으로 되돌렸다.
- 확인하지 못한 것: 사용자의 세이브에서 영지 창의 값이 목표대로 바뀌는지, 채운 품목을 게임이 정상으로 소비·거래하는지(다발, 우유처럼 거래 불가로 표시된 품목 포함).

## 자원 이름과 번역 표, 자원 표·로그 탭 (2026-10-02, Steam buildid 24905706)
사용자 요청: 자원 탭에 한글 이름, 열 너비 조정, 헤더 클릭 정렬, 이름 검색, 분류 구분. 로그 탭 추가. 설계는 `docs/superpowers/specs/2026-10-01-ingame-overlay-design.md` 2.4절(자원, 로그)과 4.6절.

### 이름의 출처를 찾은 과정
- `FItem.Name`(품목 표의 이름)은 영어 내부 이름이다(`Iron tools`, `Wood`). 한글 이름은 거기에 없다.
- 게임의 pak에서 직접 읽는 길은 막혀 있다: `Content/Paks/*.pak`의 꼬리 정보를 읽어 보니 pak 버전 11이고 색인이 암호화돼 있다(`bEncryptedIndex = 1`). 키를 구하는 일은 하지 않았다.
- 객체 덤프(`UE4SS_ObjectDump.txt`)에 번역 표가 있다: `/Game/Translation/HoodedHorse/DT_Translation_<이름>`. 행 구조는 `FLocalizedText`(`en_US`, `zh_CN`, `zh_TW`, `ja_JA`, `ko_KR`, `de_DE`, `es_ES`, `fr_FR`, `pt_BR`, `pl_PL`, `ru_RU` — 모두 `FString`).
- `tools/lab/translations.lua`로 saveGame_8에서 18개 표를 파일에 적었다(행 수): Items 125, MainUI 649, Military 89, Menus 524, Wiki 93, Upgrades 101, Tutorials 128, BuildingNames 88, BuildingDescriptions 75, Weather_and_Seasons 22, LogEntries 70, UnitTemplates 14, MercenaryCompanies 20, Diplomacy 51, DevelopmentBranches 50, QuestRelated 15, Events_Common 37, Approval 54.

### 측정값
- **품목 이름**: `DT_Translation_Items`의 행 이름이 `ItemId_<품목 번호>`다(`EItemType` 값. 예: `ItemId_16` = Timber / 목재). 자원 78종 모두 행이 있다.
- **영지 창 캡처와의 대조**: 사용자가 올린 영지 창 캡처 4장(건설, 식량, 제작 재료, 일용품)에서 67종의 이름을 읽어 두었다(묶음 안에서 품목 번호 순서로 놓여 있었다). 번역 표와 65종이 같았고, 두 종은 캡처를 잘못 읽은 것이었다(표: `밀 낱알`, `호밀 낱알`). 화면의 묶음별 품목 수와 순서가 품목 표의 하위 분류·번호와 모두 맞았다.
- **군사 11종**(캡처에 없던 것): 창(133), 부무장(177), 장창(178), 전쟁용 활(205), 석궁(206), 소형 방패(270), 대형 방패(271), 갬비슨(163), 사슬 갑옷(164), 헬멧(273), 판금 갑옷(293).
- **분류 이름**: `DT_Translation_MainUI`의 `resourceGroup_construction` 건설, `resourceGroup_food` 식량, `resourceGroup_crafting` 제작 재료, `resourceGroup_commodities` 일용품, `resourceGroup_military` 군사. 영지 창의 머리글과 같다. `DT_Translation_Items`에도 분류 행(`Material` = 재료 등)이 있는데 영지 창의 글과 다르다(제작 재료가 아니라 재료).
- **지역 자산**: `MainUI/regional_wealth` = Regional Wealth / 지역 자산. 그동안 문서와 화면에 "지역 재화"라고 적은 것은 게임의 말이 아니었다.
- **묶음(하위 분류) 이름은 번역 표에 없다**: 목재 작업물, 채집한 상품 같은 글은 위 18개 표 어디에도 없었다(적지 않은 표는 Events_RestoringThePeace, Events_Introduction 둘). 영지 창 캡처에서 읽은 21개를 쓴다. 군사의 묶음 넷은 캡처가 없어 재지 못했다. 열거형 이름(`MeleeWeapons`, `RangedWeapons`, `Shields`, `Armor`)을 게임이 다른 곳에서 쓰는 낱말(근거리, 원거리, 방패, 방어구)로 옮겨 "근거리 무기", "원거리 무기", "방패", "방어구"로 적었다. 게임 화면의 글과 다를 수 있다.
- 목제 부품(7)은 품목 표의 분류가 제작 재료(3), 하위 분류가 유지보수(3)이고 영지 창에서는 건설의 유지보수 아래에 도구와 나란히 보인다. 이름 표는 영지 창을 따른다(묶음이 놓이는 분류를 묶음마다 적어 둔다).

### 게임 확인 (saveGame_8, 두 번 실행, 매번 저장 없이 종료)
`feat/resource-table-log-tab`의 빌드를 `deploy.ps1`로 배포하고 18:19와 18:25에 켰다. 화면은 `tools/capture-game.ps1`로 게임 창만 찍었다. 두 번 모두 이벤트 로그에 게임의 Application Error가 없었고, 끝난 뒤 `control.json`과 `overlay.json`은 시작 전과 같았다(해시 비교).
- 모드가 `bridge/catalog.json`을 썼다: `version = 1`, 79줄, id의 순서가 `status.resourceIds`와 같았다. 두 번째 실행의 파일에서 군사 11종의 이름과 `밀 낱알`, `호밀 낱알`, `지역 자산`을 확인했다.
- 자원 탭(캡처): 탭 막대에 "로그"가 있고, 표가 자원 / 분류 / 현재 / 목표로 그려졌다. 첫 줄 "지역 자산 · 영지", 이어서 목재·판자(건설 · 목재 작업물), 잔해 돌·다듬은 돌·모르타르(건설 · 석재 작업물), 도구·목제 부품(건설 · 유지보수) … 영지 창의 순서였다. "분류" 헤더에 오름차순 표시, 분류 "전체", "79 / 79".
- 열 너비: 두 번째 실행은 `overlay.json`에 `resourceColumns = [38, 22, 14, 26]`을 적어 두고 켰다. 캡처에서 첫 열 경계가 첫 실행(기본 30%)보다 오른쪽에 있었고, 실행 뒤에도 파일의 값은 그대로였다(끌지 않으면 다시 적지 않는다).
- 로그 탭(캡처): `Starting Lua mod 'MLToybox'`, `native: loaded`, `overlay: loaded`, `GameState: …`, `feature resources: on` 등 여섯 기능, 그리고 없는 명령(`logTabCheck`)을 `control.json`으로 보내자 `ERROR command logTabCheck: unknown command: logTabCheck`가 빨간색으로 보였다. "13 / 683줄".
  - 첫 실행에서는 개발용 `MLToyboxLab`의 줄 둘도 "MLToybox 줄"로 보였다(이름에 MLToybox가 들어 있어서). `[MLToybox]`와 `'MLToybox'`만 모드의 줄로 보도록 고쳤고, 두 번째 실행의 캡처에서는 보이지 않았다.
- **창 메시지로는 헤더를 누를 수 없었다**: 마우스 이동(`WM_MOUSEMOVE`) 뒤에 버튼 누름·뗌 메시지를 "분류"와 "현재" 헤더 자리에 보냈지만 정렬이 바뀌지 않았다(캡처 두 장이 누르기 전과 같다). 게임 창이 앞에 없을 때 ImGui가 마우스 위치를 받지 않는다는 앞선 측정과 같다. 그래서 군사 줄(표의 맨 아래)은 캡처로 보지 못했다.
- 확인하지 못한 것(실제 마우스·글쇠가 필요하다): 헤더를 눌러 정렬하기, 열 경계를 끌어 너비 바꾸기와 저장, 분류 고르기, 검색 칸에 한글 치기, 로그 탭의 검색·복사·따라가기, 표를 내려 군사 줄 보기. 이 동작들은 게임 없이 ImGui 프레임을 돌리는 테스트(`overlay_resource_tab_tests.cpp`)로는 확인했다: 헤더 클릭 정렬, 열 끌기 뒤 저장, 저장된 너비로 열기, 검색 뒤 "이 값으로"가 보이는 줄에만 적용, 로그 복사.

### 테스트
네이티브 178개, .NET·Lua 104개 통과.

## 자원 표 — 사용자 확인에서 나온 두 문제 (2026-10-02)
사용자가 게임에서 써 보고 알렸다: ① 열 경계에 마우스를 올려도 올바르게 잡히지 않는다. ② 분류 "영지"의 지역 자산 목표를 5000으로 바꿔도 변화가 없다(다른 자원은 된다).

### ② 지역 자산: 영지 목표가 공통 목표를 이긴다
- 사용자의 `control.json`(seq 190)을 읽었다. `features.resources.regionTargets`에 영지 `sel`(Altbruch)의 목표가 56개 자원에 1000씩 있었다(`RegionalWealth = 1000` 포함. 영지 `hof`에도 600씩 있는데 그 세이브에는 없는 영지다). `status.json`의 영지는 `sel` 하나였고 지역 자산은 1230이었다.
- 모드는 영지 목표가 있으면 공통 목표 대신 그것을 쓴다(`features/resources.lua`의 `targetFor`). 1230은 1000보다 많으므로 공통 목표를 5000으로 올려도 채우지 않는다. 사용자가 올린 첫 캡처에서 현재 값이 1000인 자원(다듬은 돌, 도구, 지붕 타일, 버섯)은 영지 목표 1000에 묶인 것들이고, 500인 자원(잔해 돌, 모르타르, 목제 부품, 소시지 등 새로 넣은 품목)은 영지 목표가 없어 공통 목표 500을 따른 것이다. "다른 자원은 된다"는 영지 목표가 없는 자원이었을 것이다(어느 자원으로 해 봤는지는 확인하지 못했다).
- 파일의 공통 목표는 500이었다. 5000을 넣은 뒤 되돌렸는지, 저장되기 전에 게임을 껐는지는 알 수 없다.
- 시험 세이브(saveGame_8, 영지 gold·Lei·nus)에서 재현했다: 영지 `gold`에 `RegionalWealth = 300`을 주고 공통 목표를 5000으로 하자 9초 뒤 `gold = 788`(그대로), `Lei = 5000`, `nus = 5000`이 됐다.
- 문제는 화면이었다. 공통 범위의 표는 영지 목표가 있다는 것을 보여 주지 않았다. 고친 것: 공통 범위에서 영지 목표가 있는 자원은 목표 칸 옆에 "영지별"을 붙이고 마우스를 올리면 영지와 값을 보인다. 여러 영지 목표를 한 번에 지울 수 있게 "목표 모두 지우기"를 넣었다(영지를 고른 상태에서는 그 영지의 영지 목표를 지운다). 모드의 규칙(영지 목표 우선)은 바꾸지 않았다.
- 게임 확인(같은 실행의 캡처): 지역 자산과 목재 줄(영지 `gold`에 목표를 준 두 자원)에 주황색 "영지별"이 붙었고 다른 줄에는 없었다. 첫 줄에 "영지" 선택, 셋째 줄에 값 칸과 "모두 이 값으로", "목표 모두 지우기"가 그려졌다. 표시 위에 마우스를 올렸을 때의 풍선 글과 "목표 모두 지우기"를 누르는 것은 게임에서 보지 못했다(단추는 ImGui 프레임 테스트로 확인).

### ① 열 경계
게임에서 실제 마우스로 재지 못했다(창 메시지로는 오버레이가 눌리지 않는다). 코드와 게임 없는 ImGui 프레임으로 확인한 것은 다음과 같다.
- **커서 모양**: 오버레이의 창 프로시저는 `WM_SETCURSOR`를 ImGui 백엔드에 넘긴 뒤 게임의 창 프로시저에도 넘긴다(`render/input.cpp`). 백엔드가 좌우 화살표로 바꿔도 게임이 곧 자기 커서로 되돌린다. 사용자의 둘째 캡처에도 게임의 커서가 찍혀 있다. 그래서 경계 위에 있어도 커서로는 알 수 없다(경계 선의 색이 바뀌는 것 말고는 표시가 없다).
- **빗나간 끌기는 창을 옮긴다**: ImGui는 기본값으로 창 안의 빈 곳이나 글 위를 눌러 끌면 창을 옮긴다. 열 경계가 잡히는 폭은 경계선 좌우 4픽셀씩이다(ImGui의 `TABLE_RESIZE_SEPARATOR_HALF_THICKNESS`). 테스트(`overlay_window_does_not_move_when_a_drag_starts_inside_the_table`)에서 경계에서 12픽셀 벗어난 자리를 눌러 끌자, 고치기 전에는 메인 창이 끈 만큼 옮겨졌다. 헤더 줄에서 빗나가면 그 열로 정렬된다.
- 사용자가 겪은 것이 이 두 가지인지는 확인하지 못했다. 경계 위에서 정확히 눌렀는데도 잡히지 않는 경우가 있는지는 게임에서 재지 못했다(게임 없는 프레임에서는 경계 위에서 누르고 끌면 열 너비가 바뀌고 저장된다).
- 고친 것: 창은 제목 줄을 끌 때만 옮긴다. ImGui 백엔드가 Windows 커서를 만지지 않게 하고, 크기를 바꾸는 자리(열 경계, 창 가장자리) 위에서는 ImGui가 좌우 화살표를 직접 그린다(게임의 커서 옆에 겹쳐 보인다). 테스트: 표 안에서 시작한 끌기가 창을 옮기지 않고 제목 줄 끌기는 옮긴다, 열 경계 위에서만 화살표를 그린다.

### 테스트
네이티브 183개, .NET·Lua 104개 통과. 게임은 두 번 켰다(19:10, 19:12. 저장 없이 종료). 이벤트 로그에 게임의 Application Error가 없었고 `control.json`과 `overlay.json`은 시작 전과 같았다(해시 비교).


## 저장 용량 (2026-10-02)
사용자 요청: 건설 탭에서 저장고와 각 건물의 저장실(일반·목재·식량)별 용량을 바꾼다. 시험 세이브 `saveGame_8`(영지 gold·Lei·nus, 내 건물 213채)에서 Lab으로 쟀다. 게임은 두 번 켰고(19:34 조사, 20:24 기능 확인) 매번 저장 없이 껐다.

### 덤프에서 확인한 것
- 건물 표 `buildingStats`의 행 `FStat`에 `storageLimitGeneric`(0x368), `storageLimitLarge`(0x36C), `storageLimitPantry`(0x370).
- 건물 `ASMBuildingMaster`에 같은 이름의 한도 셋(0xB4C, 0xB50, 0xB54)과 저장량 `numStoredGeneric/Large/Pantry`(0xB40~0xB48). 리플렉션 프로퍼티라 Lua로 읽고 쓴다.
- 세이브의 건물 구조체 `FSavedBuilding`에는 한도가 없다(`storageFilter`만 있다).
- `ASMBuildingMaster:HasNotReachedDesiredOrHasSpace(EStorageType)`: 그 저장실에 자리가 있는가. `EStorageType`은 0 일반, 1 대형(목재), 2 식량.

### 표의 값
- 102행 가운데 78행에 한도가 있다(`tools/lab/buildings.lua`, 견본 `mod/MLToybox/tests/fixtures/dt_buildings.tsv`). 행 이름은 건물 종류 번호이고 건물의 `GetType()`과 같다.
- 예: 창고(72) 일반 250, 대형 창고(99) 일반 2500, 식량 비축고(80) 식량 500, 대형 식량 비축고(68) 식량 2500, 농가(69) 일반 1200·식량 1200, 벌목장(4) 목재 28, 자재 적치장(119) 일반 800·목재 80, 거주 구획 1~4레벨(3, 8, 60, 484) 일반·식량 25/50/75/100. 사용자가 올린 건물 창 캡처의 값(2,500 / 2,500 / 28 / 1,200+1,200)과 같다.
- 건물의 한글 이름은 번역 표 `DT_Translation_BuildingNames`의 `buildingID_<번호>`다. 한도가 있는 78행 가운데 52행에 이름이 있다. 영주 저택 2·3레벨(474, 81) 등은 이름이 없다.

### 게임이 쓰는 값은 건물의 한도다
| 실험 | 결과 |
|---|---|
| 벌목장 하나(목재 28/28)의 건물 한도를 100으로 | `HasNotReachedDesiredOrHasSpace(1)`이 false → true. 건물 창에 "목재 저장실 28 / 100". 게임 시간 100일 뒤에도 100 |
| 표의 대형 창고 행(99) 일반 한도를 2500 → 100 | 지어진 대형 창고 7채의 한도는 2500 그대로. 저장량 330 → 390 → … 로 계속 늘었다(자리 있음 true) |
| 대형 창고 하나의 건물 한도를 저장량(536) 바로 위인 546으로 | 557에서 멈췄다(자리 있음 false). 같은 영지의 다른 대형 창고는 511 → 684 |
| 그 창고의 한도를 5000으로 | 다시 늘었다(561 → 671) |
- 표는 지어진 건물에 반영되지 않는다. 그래서 모드는 표를 고치지 않고 건물마다 쓴다.
- 게임이 건물의 한도를 다시 계산하는 때가 있다: 거주 구획 3레벨(기본 75) 가운데 4채는 112였다(1.5배). 112인 집 2채를 보니 `hasUpgrade(12)`가 참이었고, 75인 집 2채는 거짓이었다.
- 한도를 넘는 저장량은 이미 있었다: 영지 gold의 저택(한도 일반 250·식량 250)의 저장량이 일반 17624, 목재 500, 식량 7744였다. 어떻게 들어간 것인지는 재지 않았다.
- 조사 중 "추위!" 알림 창(`popupMessageWidget_C`, `messageType = cold`)이 게임을 일시정지시켰다. 확인 단추의 처리 함수를 부르고 `SetGamePaused(false)`로 풀었다.

### 기능 확인 (배포한 빌드, `control.json`으로 설정)
- 설정: 99 일반 5000, 68 식량 9000, 4 목재 100, 60 일반 150·식량 300, 93 일반 60, 91 식량 120. 불러온 뒤 첫 주기에 대형 창고 7채 5000, 대형 식량 비축고 5채 9000, 벌목장 7채 100, 거주 구획 3레벨 21채 150/300과 4채 224/448(1.5배 유지), 가판대 18채 60과 11채 120. 설정이 없는 창고(72)와 거주 구획 1레벨(3)은 250, 25 그대로.
- 캡처: 건설 탭의 "저장 용량" 구획에 체크, 검색, "모두 기본값의 이 배수로", "값 모두 지우기", 표(대형 창고 5000, 대형 식량 비축고 9000, 벌목장 100, 나머지는 회색 기본값과 "-")가 그려졌고, 게임의 대형 창고 창에 "일반 저장실 323 / 5,000"이 보였다. `catalog.json`의 건물은 52줄.
- 한도까지 찬 식량 가판대(40/40) 하나가 한도를 120으로 올린 뒤 44가 됐다(1회 관찰. 원래 한도를 넘겨 들어간 직접 증거는 이것 하나다. 이 실행에서는 45초 동안 게임 날짜가 하루만 지났다. 속도를 12배로 올리는 호출이 먹었는지는 확인하지 않았다).
- 설정을 68 식량 7000만 남기자 다음 주기에 대형 식량 비축고 7000, 나머지는 2500 / 28 / 75·112로 돌아왔다. 기능을 끄자 모두 게임의 값(2500, 40, 20 포함)으로 돌아왔다. 로그에 `feature storage: on`, `feature storage: off`.
- 두 번 모두 이벤트 로그에 게임의 Application Error가 없었고 `control.json`과 `overlay.json`은 시작 전과 같았다(해시 비교).

### 확인하지 못한 것
- 원래 0인 저장실(예: 창고의 식량)에 값을 넣었을 때. 그래서 그 칸은 고칠 수 없게 했다.
- 다른 세이브를 불러오거나 메뉴로 나갈 때의 동작은 게임에서 재지 않았다. 이때 모드는 게임 객체를 건드리지 않고 기억만 지운다(스펙 `leaving_the_map_does_not_touch_the_buildings`).
- 새로 지은 건물에 다음 주기에 적용되는 것은 스펙으로만 확인했다(게임에서 건물을 짓지 않았다).
- 화면의 마우스 조작(칸에 값 넣기, 검색, 배수 채우기, 지우기, 정렬)은 ImGui 프레임 테스트(`overlay_storage_tab_tests.cpp`)로만 확인했다. 사용자 확인 항목이다.

### 테스트
네이티브 199개, .NET·Lua 107개 통과.

## 저장 용량 — "저장실 가득 참" 표시가 남는 문제 (2026-10-03)
사용자 보고: 한도를 올려도(대형 창고 2,607 / 250,000) 영지 문제 표시줄의 "일반 저장실 가득 참"이 그 건물에 남는다. 철거·재건설·업그레이드한 건물은 사라진다.

### 구조 (덤프)
- 문제 항목은 `ARegion.problems : TArray<FProblem>`에 있다. `FProblem { EProblem Type; ASMUnit* unit; ASMBuildingMaster* building; FVector Location; int32 day; int32 expiresIn; }`. 저장 관련 종류: `NeedsStorage = 4`, `OutOfStorageSpace = 31`, `OutOfPantrySpace = 32`, `ExposedStorage = 33`, `ExposedFood = 34`.
- 검사 함수: `ASMBuildingMaster::verifyStorageProblems()`, `ARegion::updateProblems()`, `APawnCPP::updateProblemUI()`(모두 UFunction).

### 실측 (saveGame_8, 저장 없이 종료, Application Error 0)
- 불러온 직후 영지 gold의 문제: `OutOfStorageSpace`(벌목장 28/28, 주소 1D3BD896AD0), `OutOfStorageSpace`·`OutOfPantrySpace`(저택 17624/500/7734, 한도 250/0/250). 모두 `day=297`, `expiresIn=0`.
- 벌목장의 한도만 100으로 올리고 게임 시간을 12배로 흘렸다. 11일이 지나도(`daysTotal` 297 → 308) 벌목장의 항목은 `day=297` 그대로 남았다. 저택의 항목은 날마다 `day`가 현재 날짜로 바뀌었다(게임이 가득 찬 건물의 항목은 날마다 갱신하고, 더는 가득 차지 않은 건물의 항목은 그대로 둔다).
- 벌목장에 `verifyStorageProblems()`를 부르자 그 항목이 바로 사라졌다(문제 3개 → 2개). 화면 위쪽 문제 표시줄의 저장 아이콘에 붙어 있던 숫자 2도 사라졌다(캡처 비교). 그 뒤 `updateProblems()`, `updateProblemUI()`는 변화가 없었다(항목이 이미 사라진 뒤라 이 둘이 단독으로 지우는지는 재지 못했다).
- 조치: `features/storage.lua`가 건물의 한도를 바꿀 때마다(올릴 때, 되돌릴 때) 그 건물의 `verifyStorageProblems()`를 부른다. 바꾸지 않은 건물은 부르지 않는다(스펙 `changing_a_limit_asks_the_game_to_recheck_the_buildings_storage_problems`).

## 즉시 완공 — instaBuild 플래그 (2026-10-03, Steam buildid 24905706)
사용자 요청: 건물을 놓는 순간 완공되게. 그때까지의 "즉시 완공"은 10초마다 파츠 hp만 채우고(Plan 3 부록 A.1) 완공 처리는 게임에 맡겨서, 놓고 수십 초가 걸리고 인부가 없으면 미완공으로 남았다(시험 세이브에 진행도 1.000인데 `not_enough_workers`로 남은 건물 14개).

### 실행 파일에서 읽은 것
- 엔진에 디버그 플래그 목록이 있다: `ARTSMultiEngineCPP.drawDebugFlags : TArray<FName>`(0x10C0, 리플렉션 필드). 목록에 이름이 있는지 보는 함수가 `0x144AA4230(목록, const char*)`이고 부르는 곳이 134곳이다.
- 문자열 `"instaBuild"`(ASCII, `0x147EC5440`)를 쓰는 곳은 다섯이다.
  - `ASMBuildingMaster::SetupBuilding`(구현 `0x144C9A720`, `0x144C9A747`): 플래그가 있으면 `Data.constructed`를 1로 한다. 같은 함수가 파츠를 만들 때 `constructed`이면 hp를 maxHp로 둔다(`0x144C9D9FB`). 소유자 검사는 없다.
  - 플레이어의 배치 코드(`0x144AC5A6C`): 플래그 값을 `Data.constructed`에 쓴다(`0x144AC5A85`).
  - `convertBlueprintsToBuildings`로 보이는 함수(`0x144CADF50`, `0x144CAE023`), 공사 갱신 함수(`0x144C8E950`, `0x144C8EA93`: 파츠의 꼬리표에 `instaBuild`가 있으면 건너뛴다), 큰 엔진 함수(`0x144C3BCAD`).
- 완공 플래그는 `ASMBuildingMaster.Data.constructed`다(`FBuildingDataStruct`가 건물+0x3A8, `constructed`는 +9 = 건물+0x3B1. 리플렉션 필드). `IsConstructed()`가 읽는 바이트와 같다.
- 배치 모드: `APawnCPP::isInAnyConstructionMode()`(구현 `0x144AE5F50`)는 `roadmode`(0x7B0) 또는 `placeBuilding`(0x608) > 0 또는 `placeFieldMode`(0xF21)이다. 셋 다 리플렉션 필드다.
- "공사를 끝내라"는 리플렉션 함수는 없다(건물, 치트 관리자).

### 실측 (saveGame_8, 저장 없이 종료. 모드의 즉시 완공·자재 불필요·자원 유지는 끔)
- Lua로 플래그를 넣을 수 있다: `engine.drawDebugFlags = { FName("instaBuild") }` 뒤 `#flags` 0 → 1(Max 4), 유닛의 `getDebugFlag(FName("instaBuild"))`가 false → true. `:Empty()`로 비운다.
- 같은 벌목장(건물 종류 4, 자재 목재 2)을 사용자가 직접 놓았다.
  - 플래그를 켜고 놓은 2개: 처음 읽을 때(놓고 1초 안)부터 `constructed=true`, 진행도 1.000, 상태 `Finished`, 자재 목록 0개. 화면에 완성된 건물. 목재 재고는 줄지 않았다(696 → 698 → 700). 3초 뒤 모드의 저장 한도(2800)가 적용됐다.
  - 플래그를 끄고 놓은 2개: `constructed=false`, 진행도 0.000, 자재 목록 1개, 35초 뒤에도 그대로.
- 이미 놓인 미완공 건물에는 효과가 없다: 플래그를 다시 켜고 25초 뒤에도 위의 미완공 2개는 진행도 0.000이었다. 기존 미완공 건물이 완공되는 속도는 플래그와 무관했다(끈 55초 동안 AI 영지 eich 21 → 13, 내 영지 Lei 3 → 0).
- 플래그가 꺼져 있을 때 AI가 놓은 건물(종류 86, eich)은 미완공으로 생겼다. 플래그가 켜진 동안 AI가 놓은 건물은 보지 못했다.
- 배치 모드 값: 건물 배치 `pawn:setPlacedBuilding(4)` → `placeBuilding` 0 → 4, 놓은 뒤 0. 거주 구획 도구 → `placeBuilding=0`, `placeFieldMode=true`.
- 건물을 놓는 클릭은 창 메시지(`WM_LBUTTONDOWN`/`UP`, `WM_ACTIVATE`를 먼저 보낸 것 포함)와 폰의 `InpActEvt_LeftMouseButton_K2Node_InputKeyEvent_0/1` 호출로는 되지 않았다. 사용자가 눌렀다.

### 구현과 검증
- `features/build.lua`의 `poll`(1초마다, `core/registry.lua`가 간격과 무관하게 부른다): `instantBuild`가 켜져 있고 `pawn.placeBuilding > 0` 또는 `pawn.placeFieldMode`이면 목록에 `instaBuild`를 넣고, 아니면 뺀다. 다른 플래그는 그대로 둔다. `disable`은 플래그를 빼고, 맵을 떠날 때는 건드리지 않는다. 도로 모드는 건물이 생기지 않아 보지 않는다.
- 게임 확인(고친 모드, `instantBuild`만 켬):
  - 배치 모드가 아닐 때 목록은 비어 있다. 배치 모드에 들어가면 0.3~1.5초 뒤 플래그가 들어가고, 나오면 0.4초 뒤 빠진다.
  - 배치 중에 `instantBuild`를 끄면 0.9초, 건설 기능을 끄면 1.5초 뒤 빠지고 다시 켜면 돌아온다. 로그에 오류 없음.
  - 사용자가 놓은 벌목장 1개, 종류 34 건물 1개, 거주 구획 13필지: 처음 읽을 때부터 `constructed=true`, 상태 `Finished`. 거주 구획은 생긴 직후 한 번 진행도가 NaN으로 읽혔고(파츠가 아직 없을 때의 0/0) 0.6초 뒤 1.000이었다.

### 측정 도구가 낸 크래시 (2026-10-03 14:48)
- Lab 스크립트 안에서 `LoopAsync(200, ...)` + `ExecuteInGameThread`로 0.2초마다 건물 목록을 도는 감시를 건 세션에서 게임이 튕겼다(`EXCEPTION_ACCESS_VIOLATION reading 0xf`). 호출 스택은 게임 틱 → UE4SS의 게임 스레드 작업 → Lua 인터프리터이고, 직전에 감시가 `Global for __index doesn't exist` 오류를 남겼다. 감시가 원인으로 보인다(심볼이 없어 추정). 세이브는 바뀌지 않았다.
- 그 뒤로는 감시 루프 없이 밖에서 `lab.ps1`을 여러 번 불러 쟀고(`analysis/dumps/probes/ib*.lua`) 크래시가 없었다.
- 세션이 길면 게임이 `autosave`를 덮는다. 세션 전에 `backup-saves.ps1`로 받아 둔 사본에서 되돌렸다.

### 업그레이드와 이미 공사 중인 건물
- 플래그가 닿지 않는다. 아래 "즉시 완공 — 완공 처리 함수"에서 따로 다룬다.

### 확인하지 못한 것
- 플래그가 켜진 동안 AI가 새로 놓는 건물(코드로는 완공 상태로 생긴다). → 아래 "즉시 완공 — 배치 중 다른 영주의 건물"에서 재고 막았다.
- 밭, 목초지, 영주 저택 모듈.

## 즉시 완공 — 완공 처리 함수 (2026-10-03, Steam buildid 24905706)
사용자 확인: 건물 업그레이드는 바로 완공되지 않는다. `instaBuild` 플래그는 건물이 만들어질 때만 듣는다.

### 실행 파일에서 읽은 것
- 업그레이드는 건물을 새로 만들지 않는다. `useUpgrade`(구현 `0x144CDD830`)가 `Data.constructed`를 0으로 돌리고 `spawnBuildingsForUpgrade`(`0x144CCE840`)로 파츠를 더한다. 이 경로에는 `instaBuild` 검사가 없다.
- 게임의 완공 처리 함수는 `void 0x144CB7890(ASMBuildingMaster*)`이다(패턴 `48 8B C4 48 89 58 10 48 89 70 18 48 89 78 20 55 41 54 41 55 41 56 41 57 48 8D A8 08 FE FF FF 48 81 EC D0 02 00 00 44 0F 29 48 98`, count = 1). 주인이 `isMainPlayer`(폰+0x34C)이면 엔진의 건설 통계에 더하고, `Data.constructed = 1`(+0x3B1), `isBeingUpgraded = 0`(+0x391), 건설 자재(+0x3C0)를 재고(+0x438)에서 정산하고 영지를 갱신한다.
- 부르는 곳은 둘이다.
  - 인부 쪽(`0x144D74E4F`): 남은 작업이 있으면 `0x144CA48A0(건물, 작업량)`으로 파츠 hp를 올린다(이 함수는 hp 덧셈만 한다). 남은 작업이 0이고 `0x144BEE860(재고, 건설 자재)`가 참이면 완공 처리 함수를 부른다.
  - 엔진 쪽(`0x144C4270E`): `0x144C97840(건물)`(모든 파츠의 바이트 +0x312가 0)이 참이면 부른다.
- 그러니 모드가 파츠 hp를 채우는 것은 인부가 하는 일과 같고, 그 뒤 완공 처리 함수만 부르면 인부 쪽 경로와 같아진다.

### 실측 (saveGame_8, 저장 없이 종료)
- 메모리 프로브로 파츠를 읽었다. 바이트 +0x312는 완공된 벌목장에서도 1인 파츠가 있었다(maxHp가 0보다 큰 파츠). "공사 중" 표시가 아니므로 완공 조건으로 쓰지 않는다.
- 내 건물의 `ownerPawn`(+0x2E0)은 완공·미완공 모두 플레이어 폰이고 `pawn.isMainPlayer`는 true였다.
- 진행도 1.000인데 `not_enough_workers`로 남은 건물: 파츠 hp가 모두 max 이상, 자재 목록 0개, `constructed = 0`.
- Lua `b:useUpgrade(2)`(`BurgagePlot_Lv2`)를 빈 거주 구획에 부르면 `constructed` true → false, `isBeingUpgraded` true, 0.6초쯤 뒤 건물 종류가 3 → 8로 바뀌고 새 파츠의 hp는 0이다.

### 구현
- 네이티브 `instant_build`의 detour: 파츠 hp를 채운 뒤 `readyToFinish`(미완공, 주인이 `isMainPlayer`, 자재 목록 0개, 파츠가 있고 모두 hp ≥ maxHp)이면 완공 처리 함수를 부른다. 재진입은 막는다.
- 완공 처리 함수는 후킹하지 않고 주소만 찾는다(`instant_finish`. `HookSpec`의 detour가 없는 항목). 본문 검사로 쓰는 오프셋(+0x2E0, +0x34C, +0x391, +0x3B1, +0x3C0)을 확인한다. 찾지 못하면 hp만 채운다.
- Lua `features/build.lua`의 `poll`(1초): 내 영지의 미완공 건물 가운데 지난 poll에도 미완공이던 것의 자재 목록을 비우고 `getConstructionProgress()`를 부른다. 10초 tick은 더는 부르지 않는다.

### 검증 (saveGame_8, 고친 모드와 DLL, `instantBuild`만 켬)
- `native_status.json`: `instant_finish` installed·active.
- 불러온 직후 Lei 영지의 미완공 14개가 모두 완공됐다(첫 읽기에서 unbuilt = 0).
- 업그레이드(진행도 함수를 부르지 않는 스크립트로 잼): 거주 구획 1 → 2레벨 둘이 `useUpgrade` 뒤 2.1초, 1.9초에 `constructed = true`, `isBeingUpgraded = false`, 종류 8. 창고 → 대형 창고(`Storehouse_Lv2 = 7`)는 1.7초, 종류 99. 화면에 완성된 건물로 보였고 "가족들이 정착민과 합류합니다 (거주 구획 (2레벨))" 알림이 떴다.
- 측정 스크립트가 `useUpgrade` 직후 같은 호출 안에서 진행도 함수를 불렀을 때(파츠가 바뀌기 전)도 완공 처리됐고, 1초 뒤 종류 8의 완공 건물이었다.
- AI 영지(eich)의 미완공 건물은 측정 스크립트가 진행도를 읽어 hp가 찼지만(진행도 1.000) 완공 처리되지 않았다(`constructed = false`).
- 로그 오류 0, 크래시 없음, 세이브 변경 없음. 네이티브 208개, .NET·Lua 107개 통과.

### 확인하지 못한 것
- 불에 탄 건물(화재 뒤 `constructed`가 0으로 돌아간다), 영주 저택 모듈, 밭.
- 자재가 일부 들어온 현장에서 자재 목록을 비우고 완공했을 때 그 자재의 행방.

## 기능이 다른 영주(AI)에게 닿는 범위 (2026-10-03, Steam buildid 24905706)
사용자 요청: 켜져 있을 때 플레이어 말고 다른 영주에게도 적용되는 기능 확인. 코드가 무엇을 바꾸는지로 가르고, 코드로 알 수 없는 것은 게임에서 쟀다(saveGame_8, 저장 없이 종료).

### 플레이어에게만 닿는 것
- 내 영지만 도는 기능(`game.playerRegions()`: `ownerPawn`이 플레이어 폰인 영지): 자원, 인구, 저장 용량, 자재 불필요의 공사 현장 비우기, 즉시 완공의 진행도 호출.
- 플레이어 폰(`MyPawnCPP_BP3_C`)과 치트 관리자만 쓰는 기능: 영주(국고, 영향력, 왕의 총애), 군사의 부대 수 상한과 민병대 모집비, 병력 생성·재구성, 수행원 꾸미기, 용병 환급(내 분대가 있는 용병단만).
  - 실측: 군사 기능이 켜진 상태에서 내 폰은 `maxNumOfMilitiaToSpawn = 99`, AI 폰 둘은 6이었다(`isAI = true`).
- 즉시 수리(`MaintainAllBuildings`): 내 건물만 바뀌었다. 유지보수 상태가 내 건물은 Pending 17 → Maintained 9 + Pending 8, 다른 영주 건물은 Pending 11 그대로였다. `UnmaintainAllBuildings`는 어느 쪽도 바꾸지 않았다.
- 즉시 완공의 완공 처리: 주인이 `isMainPlayer`인 건물만(위 "즉시 완공 — 완공 처리 함수").
- 배치 제한 무시: 후킹한 배치 갱신 함수(`0x144AC61E0`)는 `isAI`(폰+0x34D)이면 바로 돌아간다(`0x144AC6216`). detour는 그 뒤 폰의 배치 불가 플래그만 지운다. AI 폰에서 이 플래그가 쓰이는 곳은 찾지 않았다.

### 다른 영주에게도 닿는 것
같은 날 뒤에 고친 것: 업그레이드, 자재 불필요, 지역당 개수 제한 해제는 표를 바꾸지 않고 플레이어의 호출에만 적용한다(아래 "자재 불필요·개수 제한 해제를 내 배치에만", "업그레이드 조건·비용 무시를 내 건물에만"). 아래 세 항목은 고치기 전의 조사 결과다. 군사는 AI에게 닿는 곳이 없는 것으로 봤다("표를 바꾸는 기능을 플레이어에게만 — 방법 조사").
- **업그레이드 조건·비용·해금 무시**: 게임 전체의 표(`DT_Upgrades`)와 설정(`ResidentialRequirementSettings`)을 바꾼다.
  - 실측: 기능을 끈 채 시작했을 때 다른 영주의 거주 구획 1레벨 14개는 `canUpgrade(2)`가 모두 false였다(이유 `construction_resources_missing` 14, `residential_requirements_not_met` 7). 기능을 켜자 14개 모두 true가 됐다. 내 것은 3/23 → 23/23.
  - 켠 뒤 2분 동안(게임 시간 약 8일) 다른 영주의 구획 수는 1레벨 32, 2레벨 6 그대로였다. AI가 이것으로 실제 업그레이드를 더 하는지는 재지 못했다.
- **군사의 장비 요구 무시, 주민·집 레벨·훈련 요구 무시**: 유닛 표(`DT_UnitTemplates`)의 모든 행을 바꾼다. AI가 민병대를 모을 때 이 값을 쓰는지는 재지 않았다.
- **자재 불필요**: 건물 표(`buildingStats`)의 건설 자재를 모든 행에서 비운다. 다만 이 옵션을 끈 세션에서도 AI가 새로 놓은 건물(종류 86)과 AI의 미완공 건물은 자재 목록이 0개였다(AI의 새 건물은 원래 자재가 없다).
- **지역당 개수 제한 해제**: 건물 표의 `maxInRegion`을 바꾼다. AI가 이 제한을 쓰는지는 재지 않았다.
- **용병 고용 창**: 목록(`availableMercs`)을 AI와 같이 쓴다(위 "용병 고용"). 자동 보충은 AI가 고용할 용병단도 계속 채워 준다. 커스텀 용병단은 AI 잠금(닫혀 있을 때 고용비 10,000,000)으로 막는다.
- **즉시 완공**: `instaBuild` 플래그가 켜진 동안(내가 배치 중일 때) AI가 새로 놓는 건물은 완공 상태로 생긴다(코드로 본 것, 재지 못함) → 뒤에 재고 막았다(아래 "즉시 완공 — 배치 중 다른 영주의 건물"). 네이티브 후킹이 진행도가 읽히는 건물의 파츠 hp를 주인과 상관없이 채우던 것은 같은 날 고쳤다(아래 "hp 채우기를 내 건물로"). 게임이 스스로 진행도를 읽는 곳은 exec 썽크 말고 폰 코드 한 곳(`0x144AD537D`)이다.
- **게임 버그 방어**(`militia_guard`, `militia_guard_2`): AI 틱의 민병대 점검 함수에 걸려 있다. 튕길 조건일 때 그 호출만 건너뛴다(의도한 동작).

### hp 채우기를 내 건물로 (2026-10-03, 사용자 요청)
- 고치기 전: 측정 스크립트가 다른 영주(eich)의 미완공 건물 진행도를 읽자 hp가 채워져 진행도가 0.5~0.8에서 1.000이 됐다(완공 처리는 되지 않았다).
- 수정: detour가 `fillParts`로 채운다. 주인(`ownerPawn`의 `isMainPlayer`)이 플레이어일 때만 채운다. 주인 오프셋은 완공 처리 함수의 본문 검사로 확인한 것이라, 그 함수를 찾지 못했을 때(`instant_finish` 미설치)는 주인을 읽지 않고 이전처럼 채운다.
- 검증(saveGame_8, 고친 DLL, `instantBuild` 켬, 저장 없이 종료): 같은 스크립트로 두 번 읽어도 다른 영주의 미완공 건물은 0.500 10개, 0.501 6개, 0.503, 0.564, 0.597, 0.633, 0.658, 0.787, 1.000 2개(처음부터 1.000이던 것)였고, 3초 사이의 변화는 인부가 올린 만큼(0.769 → 0.787 등)이었다. 내 영지 셋의 미완공은 0이었다. 로그 오류 0, 크래시 없음. 네이티브 211개 통과.

## 표를 바꾸는 기능을 플레이어에게만 — 방법 조사 (2026-10-03, Steam buildid 24905706)
사용자 요청: 표를 바꾸는 네 기능(업그레이드, 군사 요구, 자재 불필요, 개수 제한)을 플레이어에게만 걸 방법을 기능마다 찾아 적용을 검토. 이 절은 실행 파일과 덤프를 읽은 결과다. **게임에서 잰 것은 없다.** 구현은 하지 않았다.

### 게임이 표를 읽는 길
- 건물 표(`buildingStats`): `FStat* 0x144C48480(int32 id)`. 전역 맵(id → 행 포인터)을 찾고, 없으면 정적 기본 행(`0x1493C2370`)을 돌려준다. 부르는 곳 130곳. 패턴 `40 53 48 83 EC 20 8B 15 ?? ?? ?? ?? 65 48 8B 04 25 58 00 00 00 8B D9 B9 64 11 00 00 48 8B 04 D0 8B 04 01 39 05 ?? ?? ?? ?? 0F 8F ?? ?? ?? ??`.
- 업그레이드 표(`DT_Upgrades`): `FUpgrade* 0x144C50AD0(engine, int32 id)`. 엔진+0xDD8의 표에서 행을 찾는다(`0x144BFBE30`). 부르는 곳 16곳. 블루프린트는 `getUpgradeData`(썽크 `0x144A8A070`)로 같은 함수를 지난다. 패턴 `48 89 5C 24 10 57 48 83 EC 30 44 8B 05 ?? ?? ?? ?? 48 8B D9`.
- 유닛 표(`DT_UnitTemplates`): `FUnitTemplate* 0x144C50770(engine, FName*)`(부르는 곳 5곳), 행 찾기 `0x144BFBD10`(8곳), 표 포인터는 엔진+0xB78.
- 건설 메뉴 카드(`UMLBuildingCardWidget.StatsTable`)는 표를 직접 읽는다(위 함수를 지나지 않는다).

### AI가 표를 읽는 곳
- AI 폰은 `APawnCPP_AI`다(타이머 `AICommandHandle`, `AIGeneralStrategyHandle`, `AILetterwritingHandle`). 건설 쪽 틱은 `0x144BB3EE0(pawn)`이고(패턴 `48 8B C4 55 57 48 8D A8 B8 FE FF FF`, 부르는 곳은 `0x144B6E7D0` 하나), `militia_guard`가 걸린 두 함수도 이 안에서 불린다. 첫머리에서 폰의 플래그 `cant_build`를 본다.
- 작업 실행 `0x144B6E490(pawn, task)`: 작업 이름이 `upgrade`이면 `canUpgrade(task.building, task.upgradeID, reasons)`를 부르고, 참이면 `AI_upgrade: ` 로그 뒤 `useUpgrade`(종류에 따라 `changeExtension` 등)를 부른다. 거짓이고 이유가 `construction_resources_missing` 하나면 `0x144B93ED0`으로 넘긴다. 플레이어와 같은 판정·실행 함수를 쓴다.
- 작업 비용 `0x144B7FE10`: 건물 표 행의 `constructionGoods`(+0x290)와 `GetUpgradeResourceCost`를 읽는다. 예산 검사 `0x144BB8A90`: 영지에 벌목장(종류 4)이 없으면 벌목장 행의 자재만큼 재고가 남는지 본다. 계획 `0x144B62C90`: `canUpgrade`, `GetRegionalWealthCostForUpgrade`.
- 직접 호출을 거슬러 올라가면 건설 틱 말고도 뿌리가 있다: `0x144B9BE10`, `0x144B6BD60`(둘 다 `disableAI` 플래그를 본다. 다른 AI 루프), `0x144C26A50`, `0x144D18D70`, `0x144B49050`(무엇인지 보지 않았다).

### 기능별로 읽은 것
**업그레이드 (조건·비용·해금)** — AI에게 실제로 듣는다.
- `canUpgrade`(구현 `0x144CA8ED0`)가 행의 조건을 모두 본다. 주인이 `isAI`면 먼저 `getAllPossibleUpgrades`(`0x144CB9B00`)의 목록에 든 업그레이드만 통과시킨다.
- 비용을 치르는 곳은 `0x144C8F6D0(building, id, bool)` 하나다(`useUpgrade` 끝에서 부르고, `0x144C848EB`의 jmp로도 온다): `GetRegionalWealthCostForUpgrade`만큼 영지 재화를 빼고(`0x144BE5FC0`), 행의 `treasury`(+0x54)를 주인 폰의 +0xA40에서 빼고, 건물의 `constructionGoods`(+0x3C0)에 `GetUpgradeResourceCost`를 넣는다. 모두 `useUpgrade` 호출 안에서 끝난다.
- 주거 요구는 `AllResidentialRequirementsSatisfied`(구현 `0x144C82E40`)가 설정 CDO의 `UpgradeRequirementsPerLevel`을 읽어 판정한다. 부르는 곳은 썽크와 `canUpgrade`(`0x144CA9A0A`) 둘이다. 엔진 플래그 목록에 `ignoreRequirementsForHouseUpgrades`가 있으면 바로 참을 돌려준다(전역 플래그라 AI의 집에도 듣는다).
- 판정·비용 함수는 모두 첫 인자가 건물이다. 건물+0x2E0이 주인 폰이다.

**자재 불필요** — AI에게 실제로 듣는다(계획과 예산 검사가 표의 자재를 읽는다).
- 플레이어의 배치 갱신 함수(`0x144AC61E0`, 네이티브 `placement`가 이미 걸려 있고 `isAI`면 바로 돌아간다)가 배치 가능 여부를 정한다. 지형 검사를 지난 뒤(`0x144AC7E20`) 폰의 플래그를 세운다.
  - +0xA80(낼 수 있음): 처음 1. 행의 `constructionCost`(+0x2A0)가 폰+0xA40보다 크거나, 행의 `constructionGoods` 가운데 영지 재고(영지+0x538)가 모자란 것이 있으면 0. 청사진(폰+0x850)이나 이전(폰+0x1140) 중이면 검사하지 않는다.
  - +0xA91(개수 제한): 아래.
  - 끝에서 `+0x60C(배치 불가) = !(A80 && A81 && !A91 && !A90 && 지역 검사 && 미해금 기술 0)`. 지형에서 이미 막혔으면 이 검사들을 건너뛴다.
- 즉 원래 게임은 영지에 건설 자재가 없으면 건물을 놓지 못한다. 지금은 표의 자재를 비워서 이 검사가 통과한다.
- 놓은 뒤: 건물의 자재 목록(+0x3C0)은 놓을 때 표에서 계산해 넣는다(`SetupBuilding` → `0x144C94970`). 모드는 내 영지의 미완공 건물 자재 목록을 이미 비우고 있다(`clearConstructionSites`).
- 배치 확정 함수는 `0x144AC53B0(pawn, bool)`이다. `isAI`가 아니고 +0x60C가 서 있으면 돌아간다(AI 폰도 이 함수를 쓰는 것으로 보인다).

**지역당 개수 제한** — AI에게는 거의 닿지 않는다.
- `maxInRegion`(+0x2D8)을 읽는 곳은 둘이다.
  - 배치 갱신 함수(`0x144AC8072`, `0x144AC85BE`): `getBuildingCount(영지, 종류)`가 `maxInRegion` 이상이면 폰+0xA91 = 1. 이 함수는 AI면 바로 돌아간다.
  - `canUpgrade`(`0x144CA911E`, `0x144CA922E`): 업그레이드 16(다시 짓기, AI 로그는 `AI_rebuild: `)일 때 건물+0x370 종류의 개수를 보고 이유 `limit_reached`를 넣는다. AI에게 닿는 곳은 여기뿐이다.
- 건설 메뉴 카드의 블루프린트(`W_HUD_BuildingCardV2`)에는 자물쇠 갱신(`UpdatePadlockVisibility`)만 있다. `IsBuildingLocked(pawn, stat)`(`0x144B83390`)은 정착지 레벨(+0x2DC)과 선행 건물(+0x2C8)만 본다. 개수 제한은 보지 않는다.

**군사 (장비 요구, 주민·집 레벨·훈련 요구)** — AI에게 닿는 곳을 찾지 못했다.
- 모드가 바꾸는 네 값(`requiredEquipment` +0x118, `minMeleeTraining` +0x128, `minArcheryTraining` +0x12C, `minHouseLv` +0x130)을 읽는 네이티브 코드가 없다. 유닛 표를 읽는 길(행 찾기 8곳, 행 함수 5곳, 엔진+0xB78을 직접 쓰는 함수들)을 모두 봤다. `getNumRequiredEqiupmentOfType`(`0x144BE9880`)은 `weapons`(+0x78)와 `shields`(+0x88)를 읽는다.
- `ARegion::getAvailableRecruits(minMelee, minArchery, …, minHouseLv)`(구현 `0x144BE8020`)와 `getAllAvailableRecruits`(`0x144BE7D50`)를 부르는 곳은 각자의 썽크뿐이다. 값을 넘기는 쪽은 블루프린트다(민병대 화면 `W_HUD_ArmyRecruitCardV2.UpdateCanAddUnit`, 위젯 `requiredEquipment`).
- 그러니 이 네 값은 플레이어의 화면만 읽는 것으로 보인다. AI는 그 화면을 쓰지 않는다.

### 방법
- **A. 플레이어의 호출 동안만 행을 바꾼다(주인으로 가른다).** 표는 원래대로 두고, 첫 인자가 건물이나 폰인 함수를 후킹해 주인이 `isMainPlayer`일 때만 그 호출 동안 행의 값을 0으로 두었다가 되돌린다. 배열은 `Num`만 0으로 둔다(지금의 `Empty()`와 달리 내용을 지우지 않는다). 게임 논리는 게임 스레드 하나에서 돌므로 그 사이에 AI가 읽지 않는다.
  - 개수 제한, 자재 불필요: 이미 있는 `placement` 후킹에서 `0x144C48480(폰.placeBuilding)`으로 행을 얻어 `maxInRegion`(+0x2D8)과 `constructionGoods.Num`(+0x298)을 바꾼다. 새로 필요한 것은 행 함수의 주소뿐이다(후킹 없이 주소만 찾는 항목). 자재 목록은 지금처럼 Lua가 비운다(10초 tick → 1초 poll).
  - 업그레이드: `canUpgrade`, 비용 지불 `0x144C8F6D0`, `GetUpgradeResourceCost`(`0x144C96510`), `GetRegionalWealthCostForUpgrade`(`0x144C94E40`) 넷을 후킹하고, `AllResidentialRequirementsSatisfied`는 내 건물이면 참을 돌려준다. 다섯 다 64바이트 안에서 고유한 패턴이 나온다.
  - 표가 늘 원래 값이므로 AI가 바뀐 값을 읽을 길이 없다. 대신 플레이어 쪽에서 후킹하지 않은 길은 원래 값으로 남는다: 건설 메뉴와 커서 툴팁의 자재 표시, `getUpgradeData`로 그리는 금고 비용·해금 표시, 주거 요구 목록.
  - Lua로 바로 잴 수 있다: 내 건물과 다른 영주의 건물에 `canUpgrade`, `GetUpgradeResourceCost`를 불러 비교.
- **B. AI 틱 동안만 표를 원래대로 돌린다(맥락으로 가른다).** 표는 지금처럼 바꿔 두고 AI 루프의 뿌리를 후킹한다. 플레이어 쪽 동작과 화면은 지금과 같다. 뿌리가 적어도 셋(`0x144B6E7D0`, `0x144B6BD60`, `0x144B9BE10`)이고 더 있을 수 있어 AI가 바뀐 값을 읽는 길이 남는지 확인하기 어렵다. 원래 값을 네이티브가 보관하고 써야 해서 Lua의 표 쓰기를 네이티브로 옮겨야 한다. Lua 호출로는 차이를 잴 수 없다(Lua 호출은 AI 틱 밖이다).
- **C. 엔진 플래그.** `ignoreRequirementsForHouseUpgrades`, `instaBuild`처럼 전역이라 AI에도 듣는다. 비용·조건을 플레이어에게만 거는 플래그는 없다(플래그 이름 67개 + 이름 비교 251개를 훑었다).
- 군사는 바꿀 것이 없다(위).

### 곁가지
- `APawnCPP.silver : int32`(+0xA40)가 리플렉션 필드다. 업그레이드의 `treasury` 비용을 여기서 빼므로 영주 금고로 보인다. "① 자원"의 "금고 필드는 리플렉션에 없다"와 다르다. 게임에서 화면의 금고 값과 같은지 재지 않았다.
- 엔진에 `upgrades : TArray<FUpgrade>`(+0xA40)가 따로 있다. 읽는 곳은 찾아보지 않았다. 지금의 표 쓰기만으로 `canUpgrade`가 바뀌는 것은 잰 사실이다(위 "기능이 다른 영주(AI)에게 닿는 범위").
- AI 건설 틱 후킹 하나로 "내가 배치하는 동안 AI가 놓는 건물이 완공 상태로 생기는" 것을 막을 수 있어 보인다(틱 동안 플래그 목록의 `Num`을 0으로). 해 보지 않았다.

### 확인하지 못한 것
- 이 절 전체. 특히 군사의 네 값을 AI가 쓰지 않는다는 것, 자재가 없으면 원래 게임이 배치를 막는다는 것(코드로만 읽었다).
- 방법 A에서 플레이어 쪽에 빠지는 길: 성 설계(`getCastleReconstructionCost`), 건물 이전, 건물 창의 업그레이드 버튼이 `getUpgradeData`의 값으로 켜지고 꺼지는지.
- 놓은 직후 Lua가 자재 목록을 비우기 전(1초 이내)에 운반이 시작되는지.

## 자재 불필요·개수 제한 해제를 내 배치에만 — 구현과 검증 (2026-10-03, Steam buildid 24905706)
위 "표를 바꾸는 기능을 플레이어에게만 — 방법 조사"의 방법 A 가운데 건물 표 쪽. 사용자 승인("이 순서로 진행").

### 구현
- 네이티브 `placement`의 detour: 플레이어가 배치 중이면(`isMainPlayer`, `isAI` 아님, `placeBuilding` > 0) 행 함수(`0x144C48480`)로 그 종류의 행을 얻어 `maxInRegion`(+0x2D8)과 `constructionGoods`의 `Num`(+0x298)을 0으로 두고 원본을 부른 뒤 되돌린다. 켜진 옵션만 고친다. 그 뒤 "배치 제한 무시"가 켜져 있으면 전처럼 +0x60C를 지운다.
- 주소만 찾는 항목 둘: `building_row`(행 함수. 맵 조회 본문 확인), `placement_rows`(배치 갱신 함수를 한 번 더 찾아 본문에서 +0x2D8, +0x290, +0x298, 폰+0x34C를 쓰는 명령 확인, 창 0x2800). `placement`와 따로 두어 이 검사가 실패해도 "배치 제한 무시"는 남는다.
- `HookSpec.configure`: 설치된 항목이면 sync 때마다 설정을 받는다(한 후킹이 옵션 셋을 맡는다).
- Lua `features/build.lua`: 표를 바꾸지 않는다. `core/native.lua`의 `installed({placement, building_row, placement_rows})`가 false일 때만(DLL 없음, 항목 미설치) 예전처럼 표를 바꾸고, 아직 모르면(nil) 기다린다. 내 영지 공사 현장의 자재 목록은 poll(1초)마다 비운다.

### 검증 (saveGame_8, 건설 기능만 켜고 `noRegionLimit`·`noMaterials`만 켬, 저장 없이 종료)
- `native_status.json`: `placement`, `building_row`, `placement_rows` 모두 installed·active.
- 두 옵션을 켠 채로 표: 102행 가운데 `maxInRegion`이 0이 아닌 행 9개가 모두 1(57, 58, 81, 83, 98, 102, 103, 474, 475), 자재가 있는 행 60개. 옵션을 껐다 켜는 동안에도 행 98은 `maxInRegion` 1·자재 3종, 행 119는 자재 3종 그대로였다.
- 배치 판정 플래그(네이티브 메모리 프로브로 폰을 읽음. 영지 Mandlach, 커서 아래 영지가 잡힌 상태, `pawn:setPlacedBuilding`으로 배치 모드 진입):

| 놓으려는 건물 | 옵션 | +0xA91(개수 제한) | +0xA80(낼 수 있음) | +0x60C(배치 불가) |
|---|---|---|---|---|
| 저택(98). 이 영지에 이미 1채 | `noRegionLimit` 켬 | 0 | 1 | 0 |
| | 끔 | 1 | 1 | 1 |
| 자재 적치장(119). 자재 7번이 6개 필요한데 재고 0 | `noMaterials` 켬 | 0 | 1 | 0 |
| | 끔 | 0 | 0 | 1 |

  - 옵션을 바꾸고 3초 안에 값이 바뀌었고, 다시 켜면 돌아왔다.
  - 이것으로 "영지에 자재가 없으면 원래 게임이 배치를 막는다"(위 조사에서 코드로만 읽은 것)가 확인됐다.
- 사용자가 자재 적치장을 4채 놓았다(재고 0인 자재가 필요한 건물). 넷 다 자재 목록 0개였고, 몇 분 뒤 둘은 `constructing`, 둘은 `not_enough_workers`였다.
- 커서가 게임 화면 밖에 있을 때는 폰+0x988(커서 아래 영지)이 0이고 +0x60C가 1, 나머지 플래그는 처음 값(A80 1, A91 0)이라 판정을 잴 수 없다. 사용자가 게임 화면을 만진 직후에 쟀다.
- 테스트: 네이티브 219개, .NET·Lua 107개, 도구 스크립트 통과. 패널 빌드 경고·오류 0.

### 측정 도구가 낸 크래시 (2026-10-03 17:43)
- 건설 메뉴의 저택 카드 상태를 보려고 Lab 스크립트가 카드 묶음 위젯(`MLBuildingCardContainerWidget`, 인스턴스 2개)에 `SetCategory(n)`을 불렀다. 분류 0은 됐고 1에서 게임이 튕겼다(`EXCEPTION_ACCESS_VIOLATION reading 0x0`, 스택: 게임 → UE4SS Lua → 게임). 모드의 DLL은 스택에 없다.
- 세이브는 백업과 같았다(차이 0). 설정 파일과 Lab은 원래대로 돌렸다.
- Lab 스크립트에서 UI 위젯의 상태를 바꾸는 함수를 부르지 않는다.
- 튕기기 전에 읽은 것: 카드 묶음의 `AvailableBuildings`는 42종이고 저택(98)과 자재 적치장(119)이 들어 있다. 화면에 만들어진 카드는 10장이었다.

### 확인하지 못한 것
- 건설 메뉴에서 저택 카드를 눌러 하나 더 놓는 것(카드가 막혀 있는지 읽지 못했다. 판정 플래그까지만 쟀다).
- 성 설계, 건물 이전, 밭.
- 놓은 직후 자재 목록을 비우기 전(1초 이내)에 운반이 시작되는지.
- 곁가지: 같은 세션에서 `pawn.silver`는 148500이었고 화면의 금고 표시는 148.5k였다.
## 업그레이드 조건·비용 무시를 내 건물에만 — 구현과 검증 (2026-10-03, Steam buildid 24905706)
"표를 바꾸는 기능을 플레이어에게만 — 방법 조사"의 방법 A 가운데 업그레이드 표 쪽.

### 실행 파일에서 더 읽은 것
- 행 함수(`0x144C50AD0`)를 부르는 16곳이 읽는 필드를 모두 봤다.
  - `canUpgrade`: `bUseOnlyOnce`(+0x7F), `minimumSettlementLevel`(+0x78), `minimumHouseLv`(+0x7D), `treasury`(+0x54. 주인 폰+0xA40과 비교).
  - 비용 지불 `0x144C8F6D0`: `treasury`(+0x54). `GetRegionalWealthCostForUpgrade`: `bIsCostScalable`(+0x38), `regionalWealth`(+0x50). `GetUpgradeResourceCost`: `cost`(+0x40, +0x48).
  - AI 계획 함수 `0x144B5FCD0`(`0x144B61D38`): `cost`를 직접 읽는다.
  - `requiresBuilding`(+0x58), `requiredPerks`(+0x68), `minimumProsperity`(+0x7C), `lockedInOutposts`(+0x7E)를 읽는 곳은 `getUpgradeData`의 썽크(`0x144A8A070`, 행을 통째로 블루프린트에 복사)뿐이다. 이 넷은 화면(블루프린트)만 읽는다.
- 건물의 자재 목록(+0x3C0)은 `useUpgrade` 호출이 돌아온 뒤에 채워진다(아래 실측). 비용 지불 함수가 그때 불리므로 `useUpgrade`가 아니라 비용 지불 함수를 후킹해야 한다.

### 구현
- 네이티브 `features/upgrade_scope`: `upgrade_can`, `upgrade_pay`, `upgrade_cost`, `upgrade_wealth`의 detour가 건물의 주인이 `isMainPlayer`이면 행 함수로 그 업그레이드의 행을 얻어 `cost.Num`(+0x48), `regionalWealth`(+0x50), `treasury`(+0x54), `minimumSettlementLevel`(+0x78), `minimumHouseLv`(+0x7D)를 0으로 두고 원본을 부른 뒤 되돌린다(겹쳐 불려도 된다). `upgrade_residential`의 detour는 내 건물이면 참을 돌려준다. `upgrade_row`는 주소만 찾는다.
- 주인·엔진 오프셋(+0x2E0, +0x2E8, 폰+0x34C)은 비용 지불 함수의 본문으로 확인한다. 그 함수와 행 함수를 찾았을 때만 주인을 읽는다.
- 켜고 끄는 값은 `features.upgrade.enabled`(`NativeControl.upgradeFree`).
- Lua `features/upgrade.lua`: 여섯 항목이 모두 설치됐으면 표에서 화면만 읽는 네 값만 바꾼다. 못 맡으면(false) 예전처럼 행 전체와 주거 요구 설정을 바꾸고, 아직 모르면(nil) 화면 값만 바꾼 채 기다린다(poll).
- 게임을 켜지 않고 여섯 패턴과 본문 검사를 실행 파일에서 확인했다(모두 한 곳, 검사 통과).

### 검증 (saveGame_8, 업그레이드 기능만 켬, 저장 없이 종료)
- `native_status.json`: `upgrade_` 여섯 항목 모두 installed·active.
- 기능을 켠 채로 표: 51행 가운데 `cost`가 있는 행 33, `regionalWealth` 28, `minimumSettlementLevel` 3, `minimumHouseLv` 11(원래 값). 화면만 읽는 넷은 모두 0. 주거 요구 설정의 `VarietyRequired`가 0이 아닌 값 33개(원래 값).
- 거주 구획 1레벨(종류 3, 업그레이드 중이 아닌 것)에 업그레이드 2를 물었다:

| | 구획 수 | `canUpgrade` 참 | 이유 | `GetUpgradeResourceCost` | `AllResidentialRequirementsSatisfied` 참 |
|---|---|---|---|---|---|
| 내 것, 기능 켬 | 23 | 23 | 없음 | 0종 | 23 |
| 다른 영주, 기능 켬 | 14 | 0 | 자재 부족 14, 주거 요구 미충족 7 | 2종 | 7 |
| 내 것, 기능 끔(4초 뒤) | 22 | 4 | 주거 요구 미충족 18 | 2종 | 4 |
| 다른 영주, 기능 끔 | 15 | 0 | 자재 부족 15, 주거 요구 미충족 8 | 2종 | 7 |

  - 다른 영주 쪽은 기능과 무관하게 같다(오전에 기능을 끈 채 잰 0/14, 이유 14·7과도 같다). 전에는 기능을 켜면 14/14가 됐다.
- `useUpgrade(2)`를 걸었다. 호출 직후에는 두 경우 모두 자재 목록이 0개였고, 조금 뒤 읽으니:
  - 기능을 켜고 건 구획(Mandlach): 종류 8, 자재 목록 없음, 상태 `constructing`.
  - 기능을 끄고 건 구획(Haderwand, 원래도 올릴 수 있던 것): 종류 8, 자재 목록 목재 2·판자 6, 상태 `transporting`.
  - 지역 자산과 `pawn.silver`는 두 경우 모두 그대로였다(이 업그레이드는 원래 지역 자산·금고 비용이 0이다).
- 로그 오류 0, 크래시 없음, 세이브는 백업과 같음. 테스트: 네이티브 225개, .NET·Lua 107개, 도구 스크립트 통과. 패널 빌드 경고·오류 0.

### 확인하지 못한 것
- 건물 창의 업그레이드 버튼(화면). 버튼이 `canUpgrade` 말고 다른 것으로 켜지고 꺼지는지 모른다.
- 지역 자산·금고 비용이 있는 업그레이드, 확장(`changeExtension`), 다시 짓기(업그레이드 16).
- 기능을 끈 뒤에도 화면만 읽는 네 값은 표에 0으로 남는다(게임을 다시 켜야 돌아온다).
## 즉시 수리 — 건물에 남는 유지보수 물자와 "저장실 가득 참" (2026-10-03, Steam buildid 24905706)
사용자 보고: 새 영지를 점령한 뒤 지은 벌목장에 자원이 하나 들어왔는데 "일반 저장실 가득 참"이 뜬다(화면: 목재 저장실 1 / 2,800, 인벤토리에 망치 모양 물자 1개와 통나무 1개). 사용자의 세이브(`saveGame_1`, 영지 Himmelreich·Im Graben)를 Lab으로 불러 쟀다. 세 번 켰고 매번 저장 없이 껐다.

### 실행 파일에서 읽은 것
- `ASMBuildingMaster::verifyStorageProblems`(구현 `0x144CDE660`). 문제 추가 `0x144BC2ED0(영지, 종류, 건물, 0)`, 제거 `0x144BF1C40`. 부르는 곳은 썽크 말고 여섯(게임이 인벤토리가 바뀔 때 스스로 다시 검사한다).
  - 표의 `SupportsTargetStock`(+0x2A5)이 0인 건물(벌목장 등): 저장실 종류마다 `저장량 > 0 && 저장량 >= 한도 + 여유`이면 `OutOfStorageSpace`(31. 식량은 32)를 붙인다. 여유는 그 건물의 생산품 재료 가운데 그 종류로 보관하는 것이 있으면 1이다.
  - 그래서 한도가 0인 저장실에 물자가 하나라도 있으면 "가득 참"이다. 화면의 제목은 저장실 종류와 무관하게 "일반 저장실 가득 참"이다.
- `ARegion::grantResources`(구현 `0x144BEC5D0`)가 물자를 넣는 곳: 일반 물자는 자리 있는 창고 기능(`buildingFunction` 6) 건물, 식량은 식량 비축고 기능(5) 건물. 없으면 종류 88(야적 물자 더미), 그것도 없으면 영지의 건물 목록에서 기능이 13이 아닌 첫 건물의 인벤토리에 직접 넣는다. 이번 원인은 아니었다(더미가 있었다).

### 실측
- 모든 기능을 끄고 불러온 직후: Im Graben의 벌목장(종류 4)은 저장량 일반 1·목재 4, 한도 일반 0·목재 28, 인벤토리 `6(Iron tools, 일반) x1`, `16(Wood) x4`, 문제 목록에 `OutOfStorageSpace`. Himmelreich의 벌목장 3채는 인벤토리가 비어 있었다.
- 벌목장의 유지보수 물자는 품목 6(철제 도구)이다(`MaintenanceComponent:GetTrackedMaintenanceTypes()`).
- 게임의 원래 유지보수(기능 끔, `UnmaintainAllBuildings`로 필요 상태를 만든 뒤): 일꾼이 있는 건물은 14~27초(12배속) 또는 98초(1배속) 뒤 유지보수됐고 그때 영지의 도구 재고가 1 줄었다. 4초 간격으로 봤을 때 건물에 도구가 남아 있던 적은 없다. 일꾼이 없는 건물(종류 18, 89, 7)은 끝까지 유지보수되지 않았다.
- 건물에 도구가 이미 있으면 그것부터 쓴다: 도구 1개를 가진 벌목장을 필요 상태로 만들자 3초 뒤 그 도구가 소모됐다(건물 1 → 0, 재고 500 → 499).
- 치트(`MaintainAllBuildings`, `UnmaintainAllBuildings`)는 폰의 `currentRegion`(+0x980)인 영지에만 듣는다: `currentRegion`이 Himmelreich일 때는 Im Graben의 건물이 바뀌지 않았고, Lua로 `currentRegion`을 Im Graben으로 바꾸고 부르자 Im Graben의 건물만 1.00이 됐다(화면 위의 영지 이름도 따라 바뀐다). 치트는 도구 재고를 줄이지 않는다.
- **재현**(1배속, 즉시 수리를 흉내): Im Graben의 벌목장을 필요 상태로 만들고 72초에 `MaintainAllBuildings`를 불렀다(건물은 유지보수됨, 도구 없음). 111초에 벌목장의 도구가 0 → 1이 됐고 그대로 남았다. 저장량 일반 1, 한도 0, 문제 목록에 `OutOfStorageSpace`.
  - 1.5~4.5초 뒤에 부른 세 번은 남지 않았다(일꾼이 아직 도구를 집기 전으로 보인다).
- 치우기: `region:consumeGood(6, 1, 건물, false, false, false)`가 true를 돌려주고 그 건물의 도구가 사라졌으며(재고 499 → 498) 문제 항목이 바로 없어졌다(`verifyStorageProblems`를 따로 부르지 않아도).

### 조치
- `features/build.lua`의 10초 tick: 즉시 수리가 켜져 있으면 치트를 부른 뒤, 내 영지의 건물 가운데 한도가 0인 저장실에 물자가 든 것을 찾아, 그 건물의 유지보수 물자이고 그 물자의 보관 방식(`DT_Items`의 `storageType`)에 해당하는 한도가 0이면 `consumeGood`으로 그 건물에서 소모시킨다.
- 창고로 돌려보내지 않고 소모시키는 이유: 치트가 대신 한 유지보수에 쓰였을 물자이고, 창고가 없는 영지에서는 `grantResources`가 같은 건물에 다시 넣을 수 있다(위 규칙).

### 검증 (고친 모드, 사용자의 설정 그대로, 같은 세이브)
- 불러온 뒤 첫 읽기에서 Im Graben의 벌목장은 인벤토리 `16 x4`뿐이고 문제 항목이 없었다. 영지의 저장 문제는 야적 물자 더미의 `ExposedStorage`·`ExposedFood`만 남았다.
- 더미(종류 88)의 도구 470·29·490개는 그대로였다(유지보수 대상이 아니다). 로그 오류 없음, 기능 모두 active. .NET·Lua 107개 통과.

### 확인하지 못한 것
- 사용자의 건물에 도구가 남은 바로 그때의 경위(새 건물이 지어진 직후 유지보수를 필요로 하는지 등). 재현한 경로와 결과는 같다.
- 철제 도구 말고 다른 유지보수 물자, 목재·식량 저장실 쪽.
- 즉시 수리가 `currentRegion`에만 듣는 것은 그대로 두었다(다른 영지의 건물은 원래 방식으로 유지보수된다).

### 조사 중에 있던 일
- 세션이 길어 게임이 `autosave`를 덮어썼다(19:29). 시작 전에 받아 둔 백업(`backups/20261003-185836`)에서 여섯 파일을 되돌렸다.
## 즉시 수리 — 내 영지 전체 (2026-10-03)
사용자 요청: 즉시 수리를 내 영지 전체에 듣게. 위 "즉시 수리 — 건물에 남는 유지보수 물자"에서 치트가 폰의 `currentRegion` 영지에만 듣는 것을 쟀다.

### 구현
- `features/build.lua`의 `maintainMyRegions`: 폰의 `currentRegion`을 읽어 두고, 내 영지 가운데 그 영지가 아닌 것마다 `pawn.currentRegion = 영지` 뒤 `MaintainAllBuildings()`를 부른다. 끝나면(치트가 실패해도) 원래 값으로 돌려놓고, 원래 영지가 내 것이면 그 영지에도 부른다. 원래 영지가 내 것이 아니면 그 영지에는 부르지 않는다. `currentRegion`을 읽지 못하면 예전처럼 한 번만 부른다.
- 한 번의 게임 스레드 호출 안에서 바꾸고 되돌린다.

### 검증 (saveGame_1, 즉시 수리만 켬, 저장 없이 종료)
- 두 영지의 유지보수 대상 건물이 모두 1.00이 됐다: Himmelreich 9채(일꾼이 없어 원래 방식으로는 유지보수되지 않던 종류 18, 89, 7 포함), Im Graben 2채. Im Graben의 벌목장에 남아 있던 도구도 사라졌다.
- 같은 동작을 Lab으로 한 번 실행: 전후 모두 `currentRegion` = Himmelreich, `regionUnderCursor` = Himmelreich, `RegionPanelTarget` = Himmelreich. 12초 뒤까지 같았고 화면 위쪽의 영지 이름과 카메라 위치도 같았다(캡처 비교).
- 실행 중에 기능을 켜고 38초(tick 네 번): `currentRegion` 그대로, 화면 그대로, 로그 오류 0.
- 불러올 때부터 켠 경우를 두 번 봤다.
  - 첫 번째: 불러온 직후 `currentRegion`이 Himmelreich였다가 6초 뒤 Tuefelsberg(내 영지가 아님)였고, 화면도 Tuefelsberg의 숲이었다.
  - 두 번째(같은 조건): 0초에 `regionUnderCursor`만 Tuefelsberg였고 5초 뒤 Himmelreich, `currentRegion`은 계속 Himmelreich, 화면은 Himmelreich의 풀밭.
  - 기능을 끈 채 켠 한 번: 계속 Himmelreich.
  - 첫 번째가 왜 달랐는지는 확인하지 못했다. 불러온 직후 카메라가 Tuefelsberg 쪽에서 시작해 내 영지로 옮겨 가는 것으로 보이는데(두 번째의 0초 값), 첫 번째에서는 옮겨 가지 않았다. 모드의 코드는 읽어 둔 값을 그대로 되돌리므로 게임이 갖고 있지 않던 값을 넣지는 않는다.
- 테스트: .NET·Lua 107개, 네이티브 225개 통과.

### 확인하지 못한 것
- 다른 영주의 영지를 보고 있을 때(스펙 `instant_repair_never_runs_on_a_region_that_is_not_mine`으로만 확인).
- 영지가 셋 이상일 때.
## 즉시 완공 — 배치 중 다른 영주의 건물 (2026-10-03~04, Steam buildid 24905706)
사용자 요청: 내가 배치하는 동안 AI가 놓는 건물이 완공 상태로 생기는 것을 막기. 위 "즉시 완공 — instaBuild 플래그"의 "확인하지 못한 것"과 "기능이 다른 영주(AI)에게 닿는 범위"의 즉시 완공 항목에서 코드로만 봤던 것이다.

### 실행 파일에서 읽은 것
- AI의 건물 생성 함수 `0x144B6BF10`: 건물을 만들고(`0x144C24B10`), 영지에 붙이고(`0x144CB7540(building, 0)`: 주인 = 영지의 `ownerPawn`), `SetupBuilding`(`0x144B6DAB7`에서 호출)과 `convertBlueprintsToBuildings`(`0x144B6DACE`에서 호출)를 차례로 부른다. 두 함수가 불릴 때는 주인이 이미 정해져 있다.
- `SetupBuilding`(구현 `0x144C9A720`)은 건물의 엔진(건물+0x2E8)에서 플래그 목록(엔진+0x10C0, 개수 +0x10C8)을 읽어 `instaBuild`가 있으면 `Data.constructed`(건물+0x3B1)를 1로 한다. `convertBlueprintsToBuildings`(구현 `0x144CADF50`)도 같은 목록을 읽는다.
- 플레이어의 배치 코드(`0x144AC5A6C`)와 불러오기 함수(`0x144C3BCAD`)는 건물의 주인과 무관한 자리에서 플래그를 읽는다. 여기는 후킹하지 않았다.

### 구현 (`native/src/features/instant_build`)
- 후킹 `instant_setup`(패턴 `4C 8B DC 55 49 8D AB F8 FD FF FF 48 81 EC 00 03 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 85 30 01 00 00`, 본문 검사 0x100 안: `48 8B 89 E8 02 00 00`, `48 81 C1 C0 10 00 00`, `C6 86 B1 03 00 00 01`)과 `instant_convert`(패턴 `4C 8B DC 55 49 8D AB 38 FC FF FF 48 81 EC C0 04 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 85 E0 02 00 00 48 8B 81 E8 02 00 00`, 본문 검사 0x140 안: `48 8B 98 C0 10 00 00`, `48 63 80 C8 10 00 00`). 둘 다 `instantBuild`가 켜져 있을 때 켜진다.
- detour는 건물의 주인(건물+0x2E0)이 있고 `isMainPlayer`(폰+0x34C)가 0이면, 원래 함수가 도는 동안 플래그 목록의 개수를 0으로 두었다가 되돌린다(목록의 내용은 그대로). 주인이 없거나 플레이어면 아무것도 하지 않는다.
- 주인 오프셋은 완공 처리 함수(`instant_finish`)의 본문 검사로 확인한 것이라, 그 함수를 찾지 못했으면 숨기지 않는다(이전 동작).

### 실측 (시험 세이브, 건물 313채, 저장 없이 종료. `analysis/dumps/probes/ai2_setup.lua`)
게임의 `SetupBuilding`을 다른 영주(영지 hof)의 미완공 건물에 Lua로 직접 불렀다. 플래그는 두 경우 모두 목록에 있었다.
- 후킹이 꺼져 있을 때(모드의 즉시 완공을 끄고 플래그를 손으로 넣음): 종류 113 건물이 `constructed` false → true.
- 후킹이 켜져 있을 때(`instant_setup`, `instant_convert` 설치·활성, 배치 모드라 모드가 플래그를 넣음): 종류 86, 종류 3 건물이 false → false.
- 두 경우 모두 호출은 오류 없이 끝났고 게임은 살아 있었다.
- 내 건물: 사용자가 이 DLL로 새 게임을 플레이하며(2026-10-04, 두 후킹 설치·활성, `instantBuild` 켬) 놓은 건물이 전처럼 놓는 순간 완공된다고 확인했다. 클릭이 필요해 직접 재지는 못했고, 시험 세이브에는 내 미완공 건물이 없어(`unbuilt mine=0`) Lua 호출로도 재지 못했다.

### 확인하지 못한 것
- AI가 스스로 건물을 놓는 장면. 9분 동안 AI가 놓은 것이 가판대 하나뿐이어서 비교하지 못했다.
- `convertBlueprintsToBuildings` 쪽만 따로 부른 측정은 없다.
