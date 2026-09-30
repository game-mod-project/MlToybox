-- 개발용: 용병 고용 창을 열거나 닫는다. __ACTION__ = open | close
local ACTION = "__ACTION__"
local screen
for _, x in ipairs(FindAllOf("mercenaryScreen_C") or {}) do
  if x:IsValid() and x:GetFullName():find("/Engine/Transient", 1, true) then screen = x end
end
if not screen then print("NO_SCREEN") return end
if ACTION == "open" then screen:Open() elseif ACTION == "close" then screen:Close() else print("BAD_ACTION", ACTION) return end
print("screen", ACTION, "visible=" .. tostring(screen:IsVisible()))
