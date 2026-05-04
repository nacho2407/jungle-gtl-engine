local isGameOver = false
local Timer = 0.0
local ScoreboardLimit = 5

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

function GameOver(finalHP, finalDocumentCount)
    if isGameOver then
        return
    end

    isGameOver = true
    finalHP = finalHP or getPlayerHP()
    finalDocumentCount = finalDocumentCount or getDocumentCount()
    local score = finalHP * finalDocumentCount

    Prefs.SetNumber("LastScore", score)
    Prefs.SetNumber("LastHP", finalHP)
    Prefs.SetNumber("LastDocumentCount", finalDocumentCount)

    print("GAME OVER")
    print("Score : ", score)
    print("Final HP : ", finalHP)
    print("Final Document Count : ", finalDocumentCount)
    print("Timer : ", Timer)
    LoadScene("GameOver.Scene")
end

function ShowScoreboard(limit)
    limit = limit or ScoreboardLimit
    local scores = Scoreboard.GetTopScores(limit)

    print("SCOREBOARD")
    for i, entry in ipairs(scores) do
        print(entry.rank, entry.name, entry.score, entry.hp, entry.documentCount, entry.timestamp)
    end
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
    Timer = Timer + dt
end
