-- 개발용: 해상도와 창 모드를 바꾼다(설정 파일에는 저장하지 않는다). 오버레이가 ResizeBuffers 를 견디는지 볼 때 쓴다.
-- lab.ps1 -Vars @{ W = 1280; H = 720; MODE = 2 }   MODE: 0 전체 화면, 1 테두리 없는 전체 창, 2 창
local gus = StaticFindObject("/Script/Engine.Default__GameUserSettings"):GetGameUserSettings()
if not gus:IsValid() then print("NO_SETTINGS") return end
local before = gus:GetScreenResolution()
print("before", before.X, before.Y, "mode=" .. tostring(gus:GetFullscreenMode()))
gus:SetScreenResolution({ X = __W__, Y = __H__ })
gus:SetFullscreenMode(__MODE__)
gus:ApplyResolutionSettings(false)
local after = gus:GetScreenResolution()
print("after", after.X, after.Y, "mode=" .. tostring(gus:GetFullscreenMode()))
