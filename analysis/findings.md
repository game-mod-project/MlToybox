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
3. (조건부) DataTable 행 쓰기가 UE4SS 3.0.1 Lua에서 불가능하면 ③과 ④(장비)도 해당된다.

## Plan 2 선행 스파이크 (우선순위순)

1. (완료) 인게임 GameMode `RTSGame_C`, 인스턴스 경로 확인
2. Lua에서 `DT_Upgrades` 행 1개 읽기·쓰기가 되는지, 그리고 `canUpgrade` 결과가 바뀌는지
3. `grantResources`로 목재 +10이 되는지, `ChangeTreasury(9999)`가 되는지
4. `AConstruction.constructionProgress=1` + `updateConstructionLevel()`로 완공되는지
