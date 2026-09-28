return {
  failureThreshold = 5,
  pollIntervalMs = 1000,
  -- GameState 전체 이름에 이 Lua 패턴이 들어 있을 때만 인게임으로 본다. nil 이면 모든 GameState를 인게임으로 본다.
  -- Task 11 분석에서 메뉴/인게임 GameState 이름을 확인해 설정한다.
  gameStateClassPattern = nil,
  -- 로드할 기능 모듈 이름 (Scripts/features/<name>.lua). Plan 2에서 추가한다.
  featureModules = {},
  -- control.json 이 아직 없을 때 쓰는 기본 설정 (features 테이블과 같은 모양)
  defaults = {},
}
