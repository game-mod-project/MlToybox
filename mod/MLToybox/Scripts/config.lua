return {
  failureThreshold = 5,
  pollIntervalMs = 1000,
  -- InitGameState 훅 context(GameMode) 전체 이름에 이 문자열이 있으면 메인 메뉴로 보고 inGame=false.
  menuGameModePattern = "MenuGameMode_ML_C",
  -- 설정하면 이 문자열이 있는 GameMode만 인게임으로 본다. nil 이면 메뉴가 아닌 모든 GameMode를 인게임으로 본다.
  gameStateClassPattern = nil,
  -- 로드할 기능 모듈 이름 (Scripts/features/<name>.lua)
  featureModules = { "resources", "lord", "build", "upgrade", "military", "mercenaries", "population", "storage", "mood", "region" },
  -- control.json 이 아직 없을 때 쓰는 기본 설정 (features 테이블과 같은 모양)
  defaults = {},
  -- 게임 안 오버레이 창(native/mltoybox_overlay.dll)을 올린다. 오버레이가 말썽이면 false 로 끈다(다른 기능은 그대로 동작)
  overlay = true,
}
