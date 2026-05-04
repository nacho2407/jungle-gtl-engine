local ui_document = nil
local player_name = "PLAYER"
local max_name_length = 10
local score = 1000
local hp = 10
local document_count = 0
local timer = 0.0
local text_width = 560

local function setText(element_id, text, red, green, blue, font_size)
    ui_document:SetTextStyle(element_id, text, red, green, blue, font_size, true, text_width)
end

local function refreshName()
    setText("game_over_name", player_name .. "_", 255, 220, 64, 35)
end

local function refreshScore()
    local minutes = math.floor(timer / 60)
    local seconds = math.floor(timer % 60)
    local score_text = string.format("Score %d   HP %d   Doc %d   Timer %02d:%02d", score, hp, document_count, minutes, seconds)
    setText("game_over_score", score_text, 255, 255, 255, 35)
end

local function submitName()
    local final_name = player_name
    if final_name == "" then
        final_name = "Player"
    end

    Scoreboard.AddScore(final_name, score, hp, document_count, timer)
    LoadScene("StartScene.Scene")
end

function BeginPlay()
    ui_document = UI.Load("UI/GameOver.rml", "game_over")
    if ui_document == nil or not ui_document:IsValid() then
        return
    end

    if Prefs.HasKey("LastScore") and Prefs.HasKey("LastHP") and Prefs.HasKey("LastDocumentCount") then
        score = math.floor(Prefs.GetNumber("LastScore", score) + 0.5)
        hp = math.floor(Prefs.GetNumber("LastHP", hp) + 0.5)
        document_count = math.floor(Prefs.GetNumber("LastDocumentCount", document_count) + 0.5)
        timer = Prefs.GetNumber("LastTimer", timer)
    end

    ui_document:SetPosition(0, 0)
    ui_document:SetZOrder(100)
    GetSoundManager():StopBGM()
    ui_document:SetTexture("background", "Textures/GameOver.jpg")

    setText("game_over_prompt", "ENTER YOUR NAME", 0, 220, 220, 35)
    refreshScore()
    refreshName()

    ui_document:Show()
end

function EndPlay()
    if ui_document ~= nil and ui_document:IsValid() then
        ui_document:Close()
    end

    ui_document = nil
end

function OnKeyPressed(key)
    if ui_document == nil or not ui_document:IsValid() then
        return
    end

    if key == "Enter" then
        submitName()
        return
    end

    if key == "Escape" or key == "Esc" then
        LoadScene("StartScene.Scene")
        return
    end

    if key == "Backspace" then
        if #player_name > 0 then
            player_name = string.sub(player_name, 1, #player_name - 1)
            refreshName()
        end
        return
    end

    if key == "Space" then
        key = " "
    end

    if #key ~= 1 or #player_name >= max_name_length then
        return
    end

    player_name = player_name .. key
    refreshName()
end
