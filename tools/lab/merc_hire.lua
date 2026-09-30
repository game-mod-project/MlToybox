-- 개발용: 고용 창에서 용병단 이름이 __NAME__ 인 카드를 게임의 고용 경로(카드 버튼 -> 확인 창의 확인)로 고용한다.
-- 고용 창은 먼저 merc_screen.lua 로 열어 둔다(AI 잠금이 풀린 가격으로 고용하려면 연 뒤 2초 기다린다).
local NAME = "__NAME__"
local function s(x) local ok, v = pcall(function() return x:ToString() end); return ok and v or tostring(x) end
local engine = FindFirstOf("MyRTSMultiEngineCPP_BP_C")
local screen
for _, x in ipairs(FindAllOf("mercenaryScreen_C") or {}) do
  if x:IsValid() and x:GetFullName():find("/Engine/Transient", 1, true) then screen = x end
end
if not screen then print("NO_SCREEN") return end
local function hired()
  local out = {}
  engine.hiredMercs:ForEach(function(k, v) out[#out + 1] = k:get() .. "=" .. s(v:get().Name) end)
  table.sort(out)
  return table.concat(out, ", ")
end
screen:updateCompanies()
local hb = screen.mercenary_companies_HB
local card
for i = 0, hb:GetChildrenCount() - 1 do
  local k = hb:GetChildAt(i)
  if s(k.MercenaryCompany.Name) == NAME then card = k end
end
if not card then print("NO_CARD", NAME) return end
print("card", NAME, "cost=" .. tostring(card.MercenaryCompany.cost), "buttonEnabled=" .. tostring(card.Button_76:GetIsEnabled()))
local before = hired()
card["BndEvt__Button_76_K2Node_ComponentBoundEvent_0_OnButtonReleasedEvent__DelegateSignature"](card)
local conf = screen.HireConfirmation
conf["BndEvt__HireConfirmation_menuButton_K2Node_ComponentBoundEvent_2_onReleased__DelegateSignature"](conf)
print("before", before)
print("after", hired())
print(hired() ~= before and "HIRED" or "NOT_HIRED")
