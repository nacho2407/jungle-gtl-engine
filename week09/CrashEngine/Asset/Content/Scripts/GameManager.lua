local isGameOver = false

local function getPlayerHP()
    if _G.PlayerState == nil then
        return 0
    end

    return _G.PlayerState.HP or 0
end

local function getDocumentCount()
    if _G.PlayerState == nil then
        return 0
    end

    return _G.PlayerState.DocumentCount or 0
end

function GameOver()
    if isGameOver then
        return
    end

    isGameOver = true
    print("GAME OVER")
    print("Final HP : ", getPlayerHP())
    print("Final Document Count : ", getDocumentCount())
end

function BeginPlay()
    _G.GameManager = {
        GameOver = GameOver,
        GetPlayerHP = getPlayerHP,
        GetDocumentCount = getDocumentCount
    }

    print("[BeginPlay] " .. obj.UUID)
    obj:PrintLocation()
end

function EndPlay()
    if _G.GameManager ~= nil and _G.GameManager.GameOver == GameOver then
        _G.GameManager = nil
    end

    print("[EndPlay] " .. obj.UUID)
    obj:PrintLocation()
end

function OnOverlap(OtherActor)
    OtherActor:PrintLocation();
end

function Tick(dt)
end
