local T = {}

function T.eq(actual, expected, msg)
  if actual ~= expected then
    error(string.format("%s: expected <%s> got <%s>", msg or "eq", tostring(expected), tostring(actual)), 2)
  end
end

function T.truthy(v, msg)
  if not v then error(string.format("%s: got <%s>", msg or "truthy", tostring(v)), 2) end
end

function T.run(cases)
  local names = {}
  for name in pairs(cases) do names[#names + 1] = name end
  table.sort(names)
  for _, name in ipairs(names) do
    local ok, err = pcall(cases[name])
    if not ok then error(name .. " FAILED: " .. tostring(err), 0) end
  end
  return #names
end

return T
