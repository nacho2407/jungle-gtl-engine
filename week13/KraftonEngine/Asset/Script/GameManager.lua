local UIManager = require("UIManager")
local ObjRegistry = require("ObjRegistry")
local gameState = nil

local function OnPhaseChanged(phase)
    print("Phase changed: " .. tostring(phase))

    if phase == ECarGamePhase.None then
        return
    end

    if phase == ECarGamePhase.CarWash then
        UIManager.FadeOut(0.5, function()
            CameraManager.PossessCamera(ObjRegistry.manCamera)

            UIManager.FadeIn(0.5)
        end)
        print("Run Lua logic for CarWash")
    elseif phase == ECarGamePhase.CarGas then
        print("Run Lua logic for CarGas")
    elseif phase == ECarGamePhase.EscapePolice then
        print("Run Lua logic for EscapePolice")
    elseif phase == ECarGamePhase.DodgeMeteor then
        print("Run Lua logic for DodgeMeteor")
    elseif phase == ECarGamePhase.Finished then
        print("Run Lua logic for Finished")
    end
end

function BeginPlay()
    UIManager.Init()
    gameState = GetGameState()
    if gameState ~= nil then
        gameState:BindPhaseChanged(OnPhaseChanged)
    end

    UIManager.SetStartGameCallback(function()
        print("Start Game!")
        UIManager.Show("gameOverlay")
    end)

    UIManager.Show("intro")
end

function EndPlay()
end

function OnOverlap(OtherActor)
end

function Tick(dt)
    UIManager.Tick(dt)
end
