-- IntroManager.lua
-- Intro.Scene 의 매니저 액터에 attach. 책임:
--   * UIManager.Init 호출 (위젯 생성/재바인딩)
--   * IntroWidget 표시
--   * 매 프레임 UIManager.Tick 만 돌려 fade 애니메이션 유지
-- 게임 페이즈/타이머/HUD 업데이트는 Map.Scene 의 GameManager.lua 책임이라 여기선 안 함.

local UIManager = require("UIManager")

function BeginPlay()
    UIManager.Init()
    UIManager.Show("intro")
end

function EndPlay()
end

function OnOverlap(OtherActor)
end

function Tick(dt)
    UIManager.Tick(dt)
end
