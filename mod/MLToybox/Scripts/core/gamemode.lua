local M = {}

-- InitGameState 훅의 context(GameMode) 전체 이름으로 인게임 여부를 판정한다.
-- 패턴은 Lua 패턴이 아니라 일반 문자열로 비교한다.
function M.isGameplay(fullName, cfg)
  if cfg.menuGameModePattern and fullName:find(cfg.menuGameModePattern, 1, true) then return false end
  if cfg.gameStateClassPattern then return fullName:find(cfg.gameStateClassPattern, 1, true) ~= nil end
  return true
end

return M
