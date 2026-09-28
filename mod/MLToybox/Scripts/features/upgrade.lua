local datatable = require("core.datatable")

local M = { name = "upgrade" }

local function clear(arr) if arr and arr.Empty then arr:Empty() end end

function M.patchUpgradeRow(row)
  clear(row.cost)
  clear(row.requiresBuilding)
  clear(row.requiredPerks)
  row.regionalWealth = 0
  row.treasury = 0
  row.minimumSettlementLevel = 0
  row.minimumProsperity = 0
  row.minimumHouseLv = 0
  row.lockedInOutposts = false
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

function M.enable(state)
  local rows = datatable.forEachRow("upgrades", function(_, row) M.patchUpgradeRow(row) end)
  local requirements = M.patchResidential(datatable.object("residentialSettings"))
  state.upgradePatch = { rows = rows, requirements = requirements }
end

M.configure = M.enable

return M
