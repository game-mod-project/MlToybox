local game = require("core.game")
local native = require("core.native")
local safe = require("core.safe")

-- 영지: 가축 상인 대기일과 자원 매장지의 최소 매장량(findings "가축 상인 대기일", "자원 매장지 — 구조와 값 쓰기 조사").
-- 가축 상인 대기 없음: 게임은 가축을 주문하면 영지의 nextLivestockOrderIn 을 30 으로 하고 하루에 1 씩 줄이며, 0 보다 크면 주문을 받지 않는다
--   (화면의 "상인 방문까지"). settings.noLivestockWait 이면 내 영지의 값을 0 으로 둔다.
-- 매장량: 매장지 하나의 양이 목표보다 적으면 목표까지 채운다.
--   덩어리형(돌·물고기·장어·열매·버섯)은 덩어리(AResource)마다의 amt·capacity 가 리플렉션에 있어 여기서 채운다.
--   광물(소금·철·점토)의 남은 양은 리플렉션에 없어 네이티브 DLL 이 날짜가 넘어갈 때 채우고(native/src/features/deposits),
--   그때의 값을 native_status.json 의 nodes 로 알려 준다. 여기서는 그 값을 상태에 합치기만 한다.
-- 풍부 여부: 리플렉션에 없다. 네이티브가 광물과 돌 매장지의 풍부 여부를 매장지의 주소와 함께 알려 주고(nodes 의 node, rich),
--   settings.richDeposits 이면 풍부하지 않은 것에 표시를 켠다(findings "매장지의 무한(지하) 판정과 고갈·재생 규칙"). 여기서는 상태에 싣기만 한다.
-- 설정: { noLivestockWait, targets = { [종류] = 양 }, regionTargets = { [영지 키] = { [종류] = 양 } } }.
--   영지 목표가 있는 종류는 공통 대신 그 값을 쓴다(0 = 그 영지는 채우지 않는다).
local M = { name = "region", intervalSec = 5, MAX_TARGET = 1000000 }

-- 덩어리의 resType → 종류 이름
M.CLUMP_KINDS = { Stone = "Stone", res_fish = "Fish", res_eel = "Eel", berries = "Berries", mushrooms = "Mushrooms" }
-- 네이티브가 알려 주는 광물의 종류 번호(ENodeType) → 종류 이름
M.MINERAL_KINDS = { [1] = "Salt", [2] = "Iron", [3] = "Clay" }
M.STONE_TYPE = 7
-- 상태에 적는 순서(게임 안 창의 표와 같다)
M.ORDER = { Salt = 1, Iron = 2, Clay = 3, Stone = 4, Fish = 5, Eel = 6, Berries = 7, Mushrooms = 8 }

local function text(v)
  local ok, s = pcall(function() return v:ToString() end)
  return ok and s or nil
end

-- 표에 든 값. 0 이상의 수가 아니면(없음, 글자, 음수) nil. 네이티브 DLL 과 게임 안 창도 그런 값을 없는 것으로 읽는다
local function entry(map, kind)
  local v = type(map) == "table" and tonumber(map[kind]) or nil
  if v and v >= 0 then return v end
  return nil
end

-- 그 영지의 그 종류 목표(1 이상). 없으면 nil. 영지 목표가 있으면 그것(0 = 이 영지는 채우지 않는다), 없으면 공통 목표
local function targetFor(settings, key, kind)
  local v = entry(type(settings.regionTargets) == "table" and settings.regionTargets[key], kind)
  if v == nil then v = entry(settings.targets, kind) end
  if not v or v < 1 then return nil end
  return math.min(math.floor(v), M.MAX_TARGET)
end

-- 덩어리들의 양과 용량의 합
local function totals(clumps, count)
  local amount, capacity = 0, 0
  for j = 1, count do
    local c = clumps[j]
    if safe.valid(c) then amount = amount + c.amt; capacity = capacity + c.capacity end
  end
  return amount, capacity
end

-- 덩어리마다 목표의 고른 몫까지 채운다. 철마다 다시 차는 덩어리는 용량도 그만큼 둔다(용량보다 많은 양을 두지 않는다)
local function fill(clumps, count, target)
  local share = math.ceil(target / count)
  for j = 1, count do
    local c = clumps[j]
    if safe.valid(c) then
      if c.bSeasonal and c.capacity < share then c.capacity = share end
      if c.amt < share then c.amt = share end
    end
  end
end

-- 네이티브가 16진수 글로 적은 주소. 읽을 수 없으면 0
local function address(text) return tonumber(tostring(text), 16) or 0 end

-- 참·거짓이면 그 값, 아니면(없음) nil
local function flag(v)
  if type(v) == "boolean" then return v end
  return nil
end

local function run(state, settings)
  local entries, byAddress = {}, {}
  for _, r in ipairs(game.playerRegions()) do
    local key = game.regionKey(r)
    if key then
      if settings and settings.noLivestockWait and r.nextLivestockOrderIn > 0 then r.nextLivestockOrderIn = 0 end
      local e = { key = key, name = game.regionName(r) or key, livestockWait = r.nextLivestockOrderIn, deposits = {} }
      entries[#entries + 1] = e
      byAddress[r:GetAddress()] = e
    end
  end

  -- 네이티브가 날짜가 넘어갈 때 적어 둔 광물·돌 매장지. 영지와 매장지가 주소(16진수)로만 적혀 있어, 이 맵에 들어온 뒤에 새로 모은 것만 쓴다
  -- (다른 세이브를 불러오면 새 객체가 예전 주소에 놓일 수 있다). 모을 때마다 번호(nodesDay)가 오른다
  local day = native.nodesDay or 0
  if state.regionNodesDay == nil then state.regionNodesDay = day end   -- 이 맵에서 처음 본 번호(맵을 떠나면 registry 가 상태를 비운다)
  local nodes = day ~= state.regionNodesDay and native.nodes or {}
  local richOf = {}   -- 매장지의 주소 → 풍부 여부(네이티브가 아는 것만)
  for _, node in ipairs(nodes) do
    local at = address(node.node)
    if at ~= 0 then richOf[at] = flag(node.rich) end
  end

  -- 덩어리형 매장지. 틱마다 새로 찾는다: 게임이 플레이 중에 매장지를 없앨 수 있어, 찾은 객체를 틱 사이에 쥐고 있지 않는다
  local empty = {}   -- 덩어리가 없는 매장지의 주소(광물, 덩어리를 다 캔 풍부한 돌)
  for _, n in ipairs(game.find.all("ResourceNode")) do
    if safe.valid(n) then
      local clumps = n.resourceClumps
      local count = #clumps
      local first = count > 0 and clumps[1] or nil
      if count == 0 then empty[n:GetAddress()] = true end
      if safe.valid(first) and safe.valid(first.Region) then
        local kind, e = M.CLUMP_KINDS[text(first.resType) or ""], byAddress[first.Region:GetAddress()]
        if kind and e then
          local amount, capacity = totals(clumps, count)
          local target = settings and targetFor(settings, e.key, kind)
          if target and amount < target then
            fill(clumps, count, target)
            amount, capacity = totals(clumps, count)
          end
          e.deposits[#e.deposits + 1] = { kind = kind, amount = amount, capacity = capacity, clumps = count, rich = richOf[n:GetAddress()] }
        end
      end
    end
  end

  -- 광물 매장지의 남은 양과, 덩어리를 다 캐고도 남아 있는 돌 매장지(풍부한 돌. 덩어리가 없어 위에서는 종류를 알 수 없다)
  for _, node in ipairs(nodes) do
    local kindId, e = tonumber(node.type) or 0, byAddress[address(node.region)]
    if e and M.MINERAL_KINDS[kindId] then
      e.deposits[#e.deposits + 1] = { kind = M.MINERAL_KINDS[kindId], amount = tonumber(node.amount) or 0, rich = flag(node.rich) }
    elseif e and kindId == M.STONE_TYPE and empty[address(node.node)] then
      e.deposits[#e.deposits + 1] = { kind = "Stone", amount = 0, capacity = 0, clumps = 0, rich = flag(node.rich) }
    end
  end

  for _, e in ipairs(entries) do
    table.sort(e.deposits, function(a, b)
      if M.ORDER[a.kind] ~= M.ORDER[b.kind] then return M.ORDER[a.kind] < M.ORDER[b.kind] end
      return a.amount > b.amount
    end)
  end
  state.region = { regions = entries }
end

function M.tick(state, settings) run(state, settings) end
M.enable = M.tick
M.configure = M.tick

-- 꺼져 있는 동안 registry 가 부른다. 탭이 지금 값을 보이도록 읽기만 한다
function M.observe(state) run(state, nil) end

return M
