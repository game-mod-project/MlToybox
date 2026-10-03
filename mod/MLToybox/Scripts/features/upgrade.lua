local datatable = require("core.datatable")
local native = require("core.native")

-- 업그레이드 조건·비용·해금 무시 (findings "표를 바꾸는 기능을 플레이어에게만 — 방법 조사").
--  게임 코드가 읽는 값(비용, 정착지·집 레벨 조건, 주거 요구)은 AI 영주도 같은 판정·비용 함수로 읽는다.
--    표는 그대로 두고, 네이티브가 내 건물에 대한 호출이 도는 동안만 그 행을 고친다(주거 요구는 내 건물이면 충족으로 본다).
--  화면(블루프린트)만 읽는 값(선행 건물, 해금, 번영도, 전초기지 잠금)은 AI 가 읽지 않으므로 여기서 표를 바꾼다.
--  네이티브가 못 맡으면(DLL 없음, 게임 업데이트로 함수를 못 찾음) 예전처럼 표 전체와 주거 요구 설정을 바꾼다. 이때는 AI 영주에게도 적용된다.
local M = { name = "upgrade",
  -- 네이티브가 맡는 데 필요한 항목: 행 함수, 판정, 비용 지불, 자재 비용, 지역 자산 비용, 주거 요구
  SCOPE = { "upgrade_row", "upgrade_can", "upgrade_pay", "upgrade_cost", "upgrade_wealth", "upgrade_residential" } }

-- 네이티브 상태를 알고 그에 맞게 처리했는가. 모르는 동안은 poll 이 다시 본다
local decided = false

local function clear(arr) if arr and arr.Empty then arr:Empty() end end

function M.patchScreenRow(row)
  clear(row.requiresBuilding)
  clear(row.requiredPerks)
  row.minimumProsperity = 0
  row.lockedInOutposts = false
end

function M.patchUpgradeRow(row)
  M.patchScreenRow(row)
  clear(row.cost)
  row.regionalWealth = 0
  row.treasury = 0
  row.minimumSettlementLevel = 0
  row.minimumHouseLv = 0
end

-- 배열을 비우면 네이티브가 레벨 인덱스로 접근할 때 위험하므로 값만 0으로 만든다
function M.patchResidential(settings)
  local levels = settings.UpgradeRequirementsPerLevel
  local changed = 0
  for li = 1, #levels do
    local reqs = levels[li].Requirements
    for ri = 1, #reqs do
      if reqs[ri].VarietyRequired ~= 0 then
        reqs[ri].VarietyRequired = 0
        changed = changed + 1
      end
    end
  end
  return changed
end

local function apply(state)
  local scoped = native.installed(M.SCOPE)
  local whole = scoped == false
  local rows = datatable.forEachRow("upgrades", function(_, row)
    if whole then M.patchUpgradeRow(row) else M.patchScreenRow(row) end
  end)
  local requirements = whole and M.patchResidential(datatable.object("residentialSettings")) or 0
  state.upgradePatch = { rows = rows, requirements = requirements, scoped = scoped == true }
  decided = scoped ~= nil
end

function M.enable(state) apply(state) end

M.configure = M.enable

function M.poll(state)
  if not decided then apply(state) end
end

return M