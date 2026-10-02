-- 저장 한도가 있는 건물 종류. 게임의 건물 표(buildingStats)에서 한도(일반·목재·식량)가 하나라도 있고,
-- 게임의 번역 표(DT_Translation_BuildingNames 의 buildingID_<번호>)에 한글 이름이 있는 행이다.
-- type 은 건물 종류 번호(표의 행 이름, 건물의 GetType())이고 설정(features.storage.limits)의 키다.
-- generic, large, pantry 는 표의 기본 한도(storageLimitGeneric / storageLimitLarge / storageLimitPantry). 0 은 그 저장실이 없다는 뜻이다.
-- 출처: tools/lab/buildings.lua 로 읽어 tests/fixtures/dt_buildings.tsv 에 두었다(2026-10-02, Steam buildid 24905706).
-- 이름은 tests/fixtures/dt_translations.tsv. tests/storage_catalog_spec.lua 가 이 목록과 두 견본을 맞춰 본다.
-- 순서: 저장 건물(창고, 대형 창고, 식량 비축고, 대형 식량 비축고)이 먼저, 나머지는 건물 번호 순서.
-- 게임이 업데이트되면 표를 다시 읽어 견본을 바꾸고, 스펙이 알려 주는 대로 여기를 고친다.
local M = {}

M.buildings = {
  { type = 72, name = "창고", generic = 250, large = 0, pantry = 0 },
  { type = 99, name = "대형 창고", generic = 2500, large = 0, pantry = 0 },
  { type = 80, name = "식량 비축고", generic = 0, large = 0, pantry = 500 },
  { type = 68, name = "대형 식량 비축고", generic = 0, large = 0, pantry = 2500 },
  { type = 3, name = "거주 구획 (1레벨)", generic = 25, large = 0, pantry = 25 },
  { type = 4, name = "벌목장", generic = 0, large = 28, pantry = 0 },
  { type = 6, name = "교역소", generic = 500, large = 0, pantry = 500 },
  { type = 7, name = "채굴지", generic = 50, large = 0, pantry = 0 },
  { type = 8, name = "거주 구획 (2레벨)", generic = 50, large = 0, pantry = 50 },
  { type = 14, name = "사냥 야영지", generic = 12, large = 0, pantry = 36 },
  { type = 18, name = "톱질 구덩이", generic = 50, large = 5, pantry = 0 },
  { type = 20, name = "풍차", generic = 0, large = 0, pantry = 250 },
  { type = 25, name = "간이 대장간", generic = 50, large = 0, pantry = 0 },
  { type = 26, name = "공용 오븐", generic = 12, large = 0, pantry = 50 },
  { type = 28, name = "제혁소", generic = 50, large = 0, pantry = 0 },
  { type = 31, name = "가축 농장", generic = 50, large = 0, pantry = 100 },
  { type = 32, name = "숯가마", generic = 60, large = 0, pantry = 0 },
  { type = 33, name = "방직공 작업장", generic = 50, large = 0, pantry = 0 },
  { type = 35, name = "채석장", generic = 200, large = 0, pantry = 0 },
  { type = 36, name = "채집꾼 오두막", generic = 0, large = 0, pantry = 50 },
  { type = 37, name = "소형 술집", generic = 0, large = 0, pantry = 15 },
  { type = 42, name = "돌 채집자 야영지", generic = 50, large = 0, pantry = 0 },
  { type = 43, name = "괴철로", generic = 50, large = 0, pantry = 0 },
  { type = 50, name = "벽과 관문", generic = 50, large = 0, pantry = 0 },
  { type = 52, name = "지하 광산", generic = 50, large = 0, pantry = 0 },
  { type = 57, name = "수비용 탑", generic = 50, large = 0, pantry = 0 },
  { type = 58, name = "세무서", generic = 50, large = 0, pantry = 0 },
  { type = 59, name = "어부 오두막", generic = 0, large = 0, pantry = 50 },
  { type = 60, name = "거주 구획 (3레벨)", generic = 75, large = 0, pantry = 75 },
  { type = 61, name = "맥아 저장고", generic = 12, large = 0, pantry = 100 },
  { type = 69, name = "농가", generic = 1200, large = 0, pantry = 1200 },
  { type = 75, name = "돌무더기", generic = 50, large = 0, pantry = 0 },
  { type = 89, name = "나무꾼 오두막", generic = 50, large = 0, pantry = 0 },
  { type = 91, name = "식량 가판대", generic = 0, large = 0, pantry = 40 },
  { type = 92, name = "장작 가판대", generic = 20, large = 0, pantry = 0 },
  { type = 93, name = "일반 물자 가판대", generic = 20, large = 0, pantry = 0 },
  { type = 95, name = "관문 관리실", generic = 50, large = 0, pantry = 0 },
  { type = 98, name = "저택", generic = 250, large = 0, pantry = 250 },
  { type = 100, name = "도적 야영지", generic = 500, large = 0, pantry = 500 },
  { type = 102, name = "장작 수레", generic = 15, large = 0, pantry = 0 },
  { type = 103, name = "식량 수레", generic = 0, large = 0, pantry = 15 },
  { type = 104, name = "망가진 풍차", generic = 50, large = 0, pantry = 0 },
  { type = 108, name = "점토 화덕", generic = 250, large = 0, pantry = 0 },
  { type = 110, name = "관목 제거", generic = 50, large = 0, pantry = 0 },
  { type = 111, name = "일꾼 야영지", generic = 200, large = 0, pantry = 200 },
  { type = 116, name = "대형 술집", generic = 0, large = 0, pantry = 50 },
  { type = 117, name = "석공의 거점", generic = 50, large = 0, pantry = 0 },
  { type = 118, name = "석회 가마", generic = 250, large = 0, pantry = 0 },
  { type = 119, name = "자재 적치장", generic = 800, large = 80, pantry = 0 },
  { type = 473, name = "염색공 작업장", generic = 50, large = 0, pantry = 50 },
  { type = 483, name = "공성 야영지", generic = 500, large = 0, pantry = 500 },
  { type = 484, name = "거주 구획(레벨 4)", generic = 100, large = 0, pantry = 100 },
}

-- 오버레이가 표에 쓸 목록(bridge/catalog.json 의 buildings). id 는 설정의 키와 같은 글자다
function M.describe()
  local out = {}
  for i, b in ipairs(M.buildings) do
    out[i] = { id = tostring(b.type), name = b.name, generic = b.generic, large = b.large, pantry = b.pantry }
  end
  return out
end

return M
