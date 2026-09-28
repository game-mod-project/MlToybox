local F = {}
local nextAddr = 1000

function F.object(fields)
  nextAddr = nextAddr + 1
  local o = fields or {}
  local addr = nextAddr
  o.IsValid = o.IsValid or function() return true end
  o.GetAddress = function() return addr end
  return o
end

function F.invalid()
  return { IsValid = function() return false end, GetAddress = function() return 0 end }
end

function F.array(items)
  local a = {}
  for i, v in ipairs(items or {}) do a[i] = v end
  a.Empty = function(self) for i = #self, 1, -1 do self[i] = nil end end
  return a
end

function F.wrap(v) return { get = function() return v end } end

function F.map(entries)
  return { ForEach = function(self, fn) for _, e in ipairs(entries) do fn(F.wrap(e[1]), F.wrap(e[2])) end end }
end

function F.datatable(rows)
  local names = {}
  for name in pairs(rows) do names[#names + 1] = name end
  table.sort(names)
  return {
    IsValid = function() return true end,
    GetRowNames = function() return names end,
    FindRow = function(_, name) return rows[name] end,
  }
end

return F
