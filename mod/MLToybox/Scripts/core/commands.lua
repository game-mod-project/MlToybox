-- 패널이 control.json 의 commands 로 보내는 일회성 명령 처리기.
-- 같은 id 는 한 번만 실행하고, 게임 재시작 후 남아 있던 명령이 다시 실행되지 않도록 오래된 명령(issuedAt)은 무시한다.
local M = { MAX_RESULTS = 10, MAX_AGE_SEC = 60 }

-- onResult(command, result): 명령 하나를 처리할 때마다 한 번 불린다(로그용). 없어도 된다
function M.new(handlers, onResult)
  local c = { handlers = handlers, done = {}, results = {}, order = {} }

  function c:_record(id, result)
    self.results[id] = result
    self.order[#self.order + 1] = id
    while #self.order > M.MAX_RESULTS do
      local old = table.remove(self.order, 1)
      self.results[old] = nil
    end
  end

  function c:run(list, ctx)
    if type(list) ~= "table" then return end
    for _, command in ipairs(list) do
      if type(command) == "table" and type(command.id) == "string" and not self.done[command.id] then
        self.done[command.id] = true
        local result
        local issued = tonumber(command.issuedAt)
        local handler = self.handlers[command.type]
        if not issued or ctx.now - issued > M.MAX_AGE_SEC then
          -- 실패가 아니라 무시한 것이다(지난 실행 때의 명령이 파일에 남아 있었다). stale 로 가려 준다
          result = { ok = false, error = "stale command (ignored)", stale = true }
        elseif not handler then
          result = { ok = false, error = "unknown command: " .. tostring(command.type) }
        else
          local ok, r = pcall(handler, command, ctx)
          result = ok and r or { ok = false, error = tostring(r) }
        end
        self:_record(command.id, result)
        if onResult then pcall(onResult, command, result) end
      end
    end
  end

  function c:status()
    if next(self.results) then return self.results end
    return nil
  end

  return c
end

return M
