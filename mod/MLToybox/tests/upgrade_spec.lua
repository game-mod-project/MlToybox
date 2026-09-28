local T = require("t")
local F = require("fakes")
local datatable = require("core.datatable")
local upgrade = require("features.upgrade")

local function upRow()
  return {
    cost = F.array({ { Type = 16, amt = 2 } }), requiresBuilding = F.array({ 5 }), requiredPerks = F.array({ "x" }),
    regionalWealth = 25, treasury = 10, minimumSettlementLevel = 2, minimumProsperity = 1, minimumHouseLv = 2, lockedInOutposts = true,
  }
end

local function residential()
  local function req(v) return { VarietyRequired = v } end
  return { UpgradeRequirementsPerLevel = F.array({
    { Requirements = F.array({}) },
    { Requirements = F.array({ req(1), req(0), req(2) }) },
  }) }
end

T.run({
  patch_upgrade_row_clears_everything = function()
    local r = upRow()
    upgrade.patchUpgradeRow(r)
    T.eq(#r.cost, 0, "cost"); T.eq(#r.requiresBuilding, 0, "requiresBuilding"); T.eq(#r.requiredPerks, 0, "perks")
    T.eq(r.regionalWealth, 0, "wealth"); T.eq(r.treasury, 0, "treasury"); T.eq(r.minimumSettlementLevel, 0, "settle")
    T.eq(r.minimumProsperity, 0, "prosperity"); T.eq(r.minimumHouseLv, 0, "house"); T.eq(r.lockedInOutposts, false, "outposts")
  end,
  patch_residential_zeroes_values_keeps_arrays = function()
    local s = residential()
    T.eq(upgrade.patchResidential(s), 2, "two changed")
    local reqs = s.UpgradeRequirementsPerLevel[2].Requirements
    T.eq(#reqs, 3, "array kept"); T.eq(reqs[1].VarietyRequired, 0, "zeroed"); T.eq(reqs[3].VarietyRequired, 0, "zeroed")
  end,
  enable_patches_all_rows_and_settings = function()
    local rows = { ["2"] = upRow(), ["4"] = upRow() }
    local s = residential()
    datatable.find = function(p)
      if p == datatable.PATHS.upgrades then return F.datatable(rows) end
      if p == datatable.PATHS.residentialSettings then return F.object(s) end
    end
    local st = {}
    upgrade.enable(st, { enabled = true })
    T.eq(rows["4"].regionalWealth, 0, "row patched"); T.eq(st.upgradePatch.rows, 2, "rows"); T.eq(st.upgradePatch.requirements, 2, "reqs")
  end,
  enable_fails_when_table_missing = function()
    datatable.find = function() return nil end
    T.eq((pcall(upgrade.enable, {}, { enabled = true })), false, "error propagates to registry")
  end,
})
