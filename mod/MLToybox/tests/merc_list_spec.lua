local T = require("t")
local F = require("fakes")
local game = require("core.game")
local datatable = require("core.datatable")
local plan = require("features.merc_plan")
local list = require("features.merc_list")

local function row(name, cost, units, traits)
  return { Name = name, cost = cost, units = F.array(units), traits = F.array(traits or {}), banner = F.object({}), colorA = 1, colorB = 2, emblemA = 3, emblemB = 4 }
end

-- 가짜 게임: 용병 표 5행(q 는 quest 행), rerollMercenaries 는 실측한 규칙(고용 중 이름과 quest 행 제외, 최대 3개)을 따른다
local function setup(opts)
  opts = opts or {}
  -- 출하 기본값(M1 결과)과 무관하게 케이스마다 명시한다. 기본은 표 행 복사 가능
  list.CAN_COPY_ROWS = opts.canCopyRows ~= false
  local regions = { gold = F.object({ key = "gold" }), nus = F.object({ key = "nus" }) }
  local rows = {
    a = row("a", 10, { "inf" }),
    b = row("b", 20, { "inf", "bow" }, { "Looters" }),
    c = row("c", 30, { "spear" }),
    d = row("d", 40, { "bow" }),
    q = row("q", 50, { "inf" }, { "questOnly" }),
  }
  datatable.find = function(p)
    if p == datatable.PATHS.mercenaries then return F.datatable(rows) end
  end
  game.fname = function(s) return s end
  game.regionKey = function(r) return r.key end
  game.regionByKey = function(key) return regions[key] end

  local hired = {}
  for i, name in ipairs(opts.hired or {}) do hired[i] = { i - 1, { Name = name, cost = 0 } } end
  local real = F.object({ availableMercs = F.array(opts.list or {}), hiredMercs = F.map(hired), rerolls = 0 })
  real.rerollMercenaries = function(self)
    self.rerolls = self.rerolls + 1
    if opts.rerollFails then error("reroll exploded") end
    local taken = {}
    for _, e in ipairs(hired) do taken[e[2].Name] = true end
    self.availableMercs:Empty()
    for _, rowName in ipairs({ "a", "b", "c", "d", "q" }) do
      local r = rows[rowName]
      if #self.availableMercs < 3 and r.traits[1] ~= "questOnly" and not taken[r.Name] then
        self.availableMercs[#self.availableMercs + 1] = {
          Name = r.Name, cost = r.cost, units = F.array({ table.unpack(r.units) }), traits = F.array({ table.unpack(r.traits) }),
          banner = r.banner, colorA = r.colorA, colorB = r.colorB, emblemA = r.emblemA, emblemB = r.emblemB,
          arrivesIn = 20, arrivalRegion = regions.gold,
        }
      end
    end
  end
  -- 배열 전체 대입(engine.availableMercs = ...)은 게임을 튕긴다. 코드가 그렇게 하면 테스트가 실패한다
  local engine = setmetatable({}, {
    __index = real,
    __newindex = function(_, k, v)
      if k == "availableMercs" then error("whole-array assignment is forbidden") end
      real[k] = v
    end,
  })
  return engine, rows, regions
end

local function custom(name, over)
  local company = { name = name, units = { "inf", "bow" }, cost = 3000, region = "nus" }
  for k, v in pairs(over or {}) do company[k] = v end
  return { kind = "custom", name = name, company = company, cost = plan.LOCK_COST }
end

local function vanilla(name, cost, keep)
  return { kind = "vanilla", name = name, rowName = name, cost = cost, keep = keep or false }
end

local function entry(name, over)
  local e = { Name = name, cost = 1, units = F.array({ "inf" }), traits = F.array({}), banner = F.object({}), colorA = 0, colorB = 0, emblemA = 0, emblemB = 0, arrivesIn = 5 }
  for k, v in pairs(over or {}) do e[k] = v end
  return e
end

local function listNames(engine)
  local out = {}
  for i = 1, #engine.availableMercs do out[i] = engine.availableMercs[i].Name end
  return table.concat(out, ",")
end

T.run({
  rows_summarise_the_company_table = function()
    setup()
    local rows = list.rows()
    T.eq(#rows, 5, "five rows"); T.eq(rows[1].rowName, "a", "row name"); T.eq(rows[1].name, "a", "name"); T.eq(rows[1].cost, 10, "cost")
    T.eq(rows[2].quest, false, "other traits are not quest"); T.eq(rows[5].quest, true, "questOnly trait")
  end,
  read_reports_each_slot = function()
    local engine, _, regions = setup({ list = { entry("x", { cost = 7, units = F.array({ "inf", "bow" }) }), entry("y") } })
    engine.availableMercs[1].arrivalRegion = regions.nus
    local cur = list.read(engine)
    T.eq(#cur, 2, "two"); T.eq(cur[1].name, "x", "name"); T.eq(cur[1].cost, 7, "cost")
    T.eq(table.concat(cur[1].units, ","), "inf,bow", "units"); T.eq(cur[1].region, "nus", "region key")
    T.eq(cur[2].region, nil, "no arrival region")
  end,
  hired_names_lists_every_hired_company = function()
    local engine = setup({ hired = { "a", "토이박스" } })
    local names = list.hiredNames(engine)
    T.eq(names.a, true, "vanilla"); T.eq(names["토이박스"], true, "custom"); T.eq(names.b, nil, "not hired")
  end,
  rebuild_writes_customs_first_then_vanilla = function()
    local engine, rows, regions = setup()
    local made = list.rebuild(engine, { custom("토이박스"), vanilla("c", 30), vanilla("d", 40) }, 0)
    T.eq(made, 3, "three slots"); T.eq(engine.rerolls, 1, "one reroll"); T.eq(listNames(engine), "토이박스,c,d", "names")
    local first = engine.availableMercs[1]
    T.eq(table.concat(first.units, ","), "inf,bow", "custom units"); T.eq(#first.traits, 0, "no traits"); T.eq(first.cost, plan.LOCK_COST, "cost")
    T.eq(first.arrivesIn, list.CUSTOM_ARRIVES_IN, "arrival days"); T.eq(first.arrivalRegion, regions.nus, "region")
    local second = engine.availableMercs[2]
    T.eq(table.concat(second.units, ","), "spear", "row units copied"); T.eq(#second.traits, 0, "stale traits cleared")
    T.eq(second.banner, rows.c.banner, "row banner"); T.eq(second.cost, 30, "row cost"); T.eq(second.arrivesIn, 20, "a new pick keeps the rerolled arrival")
  end,
  rebuild_makes_room_by_renaming_hired_rows_and_restores_them = function()
    local engine, rows = setup({ hired = { "a", "b", "c", "d" } })
    local made = list.rebuild(engine, { custom("토이박스"), custom("궁수대") }, 2)
    T.eq(made, 2, "two slots"); T.eq(listNames(engine), "토이박스,궁수대", "customs")
    T.eq(rows.a.Name, "a", "row a restored"); T.eq(rows.b.Name, "b", "row b restored")
  end,
  rebuild_restores_row_names_when_the_reroll_fails = function()
    local engine, rows = setup({ hired = { "a" }, rerollFails = true })
    local ok = pcall(list.rebuild, engine, { custom("토이박스") }, 1)
    T.eq(ok, false, "the error surfaces"); T.eq(rows.a.Name, "a", "row restored")
  end,
  rebuild_restores_row_names_when_a_rename_fails = function()
    local engine, rows = setup({ hired = { "a", "b" } })
    -- 두 번째 행의 임시 이름 쓰기가 실패한다(원래 이름으로 되돌리는 쓰기는 된다)
    local real = rows.b
    rows.b = setmetatable({}, {
      __index = real,
      __newindex = function(_, k, v)
        if k == "Name" and v ~= "b" then error("write failed") end
        real[k] = v
      end,
    })
    local ok = pcall(list.rebuild, engine, { custom("토이박스"), custom("궁수대") }, 2)
    T.eq(ok, false, "the error surfaces"); T.eq(rows.a.Name, "a", "the row renamed before the failure is restored")
    T.eq(engine.rerolls, 0, "no reroll on a half-renamed table")
  end,
  rebuild_restores_the_arrival_of_kept_vanilla = function()
    local engine, _, regions = setup({ list = { entry("b", { cost = 20, arrivesIn = 33 }) } })
    engine.availableMercs[1].arrivalRegion = regions.nus
    list.rebuild(engine, { vanilla("b", 20, true), vanilla("a", 10), vanilla("c", 30) }, 0)
    T.eq(listNames(engine), "b,a,c", "desired order")
    T.eq(engine.availableMercs[1].arrivesIn, 33, "kept arrival days"); T.eq(engine.availableMercs[1].arrivalRegion, regions.nus, "kept region")
    T.eq(table.concat(engine.availableMercs[1].traits, ","), "Looters", "row traits copied")
    T.eq(engine.availableMercs[2].arrivesIn, 20, "a new pick keeps the rerolled arrival")
  end,
  rebuild_writes_what_it_can_when_fewer_slots_come_back = function()
    local engine = setup({ hired = { "a", "b", "c" } })
    local made = list.rebuild(engine, { custom("토이박스"), vanilla("d", 40), vanilla("a", 10) }, 0)
    T.eq(made, 1, "only one slot came back"); T.eq(listNames(engine), "토이박스", "first desired entry written")
  end,
  without_row_copy_customs_take_the_temp_slots = function()
    local engine, rows = setup({ hired = { "a", "b", "c" }, canCopyRows = false })
    local made = list.rebuild(engine, { custom("토이박스"), vanilla("d", 40) }, 1)
    T.eq(made, 2, "two slots"); T.eq(listNames(engine), "토이박스,d", "custom replaced the temp slot, vanilla left as rerolled")
    T.eq(rows.a.Name, "a", "row restored")
  end,
  apply_in_place_writes_only_changed_fields = function()
    local engine, _, regions = setup({ list = {
      entry("토이박스", { cost = plan.LOCK_COST, units = F.array({ "inf", "bow" }), arrivesIn = 1 }),
      entry("b", { cost = 20 }),
    } })
    engine.availableMercs[1].arrivalRegion = regions.nus
    local units = engine.availableMercs[1].units
    local d = custom("토이박스"); d.cost = 3000
    list.applyInPlace(engine, { d })
    T.eq(engine.availableMercs[1].cost, 3000, "cost updated"); T.eq(engine.availableMercs[1].units, units, "unit array untouched")
    T.eq(engine.availableMercs[2].cost, 20, "other slot untouched"); T.eq(engine.rerolls, 0, "no reroll")
  end,
  apply_in_place_fixes_a_vanilla_cost = function()
    local engine = setup({ list = { entry("a", { cost = 0 }) } })
    list.applyInPlace(engine, { vanilla("a", 10, true) })
    T.eq(engine.availableMercs[1].cost, 10, "table cost restored")
  end,
  apply_in_place_moves_a_kept_vanilla_with_its_arrival = function()
    local engine, rows, regions = setup({ list = {
      entry("b", { cost = 20, arrivesIn = 33, units = F.array({ "inf", "bow" }), traits = F.array({ "Looters" }) }),
      entry("c", { cost = 30, arrivesIn = 12, units = F.array({ "spear" }) }),
      entry("gone", { arrivesIn = 7 }),
    } })
    engine.availableMercs[1].arrivalRegion = regions.nus
    engine.availableMercs[2].arrivalRegion = regions.gold
    list.applyInPlace(engine, { custom("토이박스"), vanilla("b", 20, true), vanilla("c", 30, true) })
    T.eq(listNames(engine), "토이박스,b,c", "customs take the front slots")
    local b, c = engine.availableMercs[2], engine.availableMercs[3]
    T.eq(b.arrivesIn, 33, "b keeps its arrival days"); T.eq(b.arrivalRegion, regions.nus, "b keeps its region")
    T.eq(table.concat(b.units, ","), "inf,bow", "b units"); T.eq(table.concat(b.traits, ","), "Looters", "b traits"); T.eq(b.banner, rows.b.banner, "b banner")
    T.eq(c.arrivesIn, 12, "c keeps its arrival days"); T.eq(c.arrivalRegion, regions.gold, "c keeps its region")
    T.eq(engine.rerolls, 0, "no reroll")
  end,
  apply_custom_costs_writes_only_the_cost_of_listed_customs = function()
    local engine = setup({ list = {
      entry("토이박스", { cost = 3000, units = F.array({ "inf", "bow" }) }),
      entry("c", { cost = 1 }),
    } })
    local units = engine.availableMercs[1].units
    list.applyCustomCosts(engine, { custom("토이박스"), custom("궁수대"), vanilla("c", 30, true) })
    T.eq(engine.availableMercs[1].cost, plan.LOCK_COST, "custom cost synced"); T.eq(engine.availableMercs[1].units, units, "nothing else written")
    T.eq(engine.availableMercs[2].cost, 1, "vanilla slots wait for the rebuild"); T.eq(engine.rerolls, 0, "no reroll")
  end,
  -- 실제 merc_list 와 mercenaries.tick 을 함께 돌린다: 고용 직후 재구성을 기다리는 동안에도 AI 잠금이 돌아와야 한다
  the_lock_returns_at_once_after_a_hire_while_the_rebuild_waits = function()
    local merc = require("features.mercenaries")
    local engine = setup()
    engine.squads = F.array({})
    local find = datatable.find
    datatable.find = function(p)
      if p == datatable.PATHS.unitTemplates then return F.datatable({ inf = {}, bow = {} }) end
      return find(p)
    end
    local pawn = F.object({})
    game.pawn = function() return pawn end
    game.engine = function() return engine end
    game.cheat = function() return nil end
    game.regionList = function() return { { key = "nus", name = "Haderwand" } } end
    local open = true
    local screen = F.object({
      IsVisible = function() return open end, updateCompanies = function() end,
      HireConfirmation = F.object({ IsVisible = function() return false end }),
    })
    game.mercScreen = function() return screen end
    local now = 100
    merc.clock = function() return now end
    merc.pick = function(candidates, count)
      local out = {}
      for i = 1, count do out[i] = candidates[i] end
      return out
    end
    local settings = { enabled = true, refund = false, lockFromAi = true,
      companies = { { name = "토이박스", units = { "inf", "bow" }, cost = 3000, region = "nus", enabled = true } } }
    local state = {}
    merc.enable(state, settings)
    merc.tick(state, settings)
    T.eq(listNames(engine), "토이박스,a,b", "rebuilt with the screen open"); T.eq(engine.availableMercs[1].cost, 3000, "unlocked while open")
    -- 순정 카드를 하나 고용해 칸이 줄고, 1초 뒤 창을 닫는다. 다음 재구성은 아직 기다려야 한다
    table.remove(engine.availableMercs, 2)
    now = now + 1
    open = false
    merc.tick(state, settings)
    T.eq(engine.rerolls, 1, "the rebuild waits"); T.eq(engine.availableMercs[1].cost, plan.LOCK_COST, "but the custom card is locked again")
    now = now + merc.REBUILD_MIN_INTERVAL
    merc.tick(state, settings)
    T.eq(engine.rerolls, 2, "rebuilt after the interval"); T.eq(#engine.availableMercs, 3, "three slots again")
    T.eq(engine.availableMercs[1].cost, plan.LOCK_COST, "still locked")
  end,
  screen_state_reads_the_hire_screen_and_confirmation = function()
    setup()
    game.mercScreen = function() return nil end
    local s = list.screenState()
    T.eq(s.open, false, "no widget"); T.eq(s.confirming, false, "no widget 2")
    local visible, confirm = true, true
    local screen = F.object({ IsVisible = function() return visible end, HireConfirmation = F.object({ IsVisible = function() return confirm end }) })
    game.mercScreen = function() return screen end
    s = list.screenState(); T.eq(s.open, true, "open"); T.eq(s.confirming, true, "confirming")
    confirm = false
    s = list.screenState(); T.eq(s.confirming, false, "open without a confirmation")
    visible, confirm = false, true
    s = list.screenState(); T.eq(s.open, false, "closed"); T.eq(s.confirming, false, "a hidden screen is never confirming")
  end,
  refresh_screen_updates_only_an_open_screen = function()
    local updates, visible = 0, false
    local screen = F.object({ IsVisible = function() return visible end, updateCompanies = function() updates = updates + 1 end })
    game.mercScreen = function() return screen end
    list.refreshScreen(); T.eq(updates, 0, "closed")
    visible = true
    list.refreshScreen(); T.eq(updates, 1, "open")
    game.mercScreen = function() return nil end
    list.refreshScreen(); T.eq(updates, 1, "no widget")
  end,
})
