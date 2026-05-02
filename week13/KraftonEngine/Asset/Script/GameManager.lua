local UIManager = require("UIManager")

function BeginPlay()
    UIManager.Init()
    UIManager.SetStartGameCallback(function()
        print("Start Game!")
        UIManager.Show("gameOverlay")
    end)

    UIManager.Show("gameOver")
end

function EndPlay()
end

function OnOverlap(OtherActor)
end

function Tick(dt)
end
