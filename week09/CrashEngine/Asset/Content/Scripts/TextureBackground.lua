local ui_document = nil

local selected_index = 1
local current_screen = "menu"
local gamepad_controller_id = 0
local gamepad_stick_threshold = 0.55
local gamepad_stick_repeat_delay = 0.22
local gamepad_stick_repeat_timer = 0.0
local gamepad_stick_direction = 0

local menu_items = {
    {
        label_id = "game_start_label",
        text = "Game Start"
    },
    {
        label_id = "credit_label",
        text = "Credit"
    },
    {
        label_id = "scoreboard_label",
        text = "Scoreboard"
    }
}

local credit_items = {
    {
        line_id = "credit_line_1",
        label_id = "credit_text_1",
        text = "Team 8"
    },
    {
        line_id = "credit_line_2",
        label_id = "credit_text_2",
        text = "장민준"
    },
    {
        line_id = "credit_line_3",
        label_id = "credit_text_3",
        text = "오준혁"
    },
    {
        line_id = "credit_line_4",
        label_id = "credit_text_4",
        text = "권현수"
    },
    {
        line_id = "credit_line_5",
        label_id = "credit_text_5",
        text = "이호진"
    }
}

local menu_text_style = {
    normal_red = 255,
    normal_green = 255,
    normal_blue = 255,
    highlight_red = 255,
    highlight_green = 220,
    highlight_blue = 64,
    font_size = 32,
    bold = true,
    center_width = 300
}

local credit_text_style = {
    red = 255,
    green = 220,
    blue = 64,
    font_size = 32,
    bold = true,
    center_width = 300
}

local scoreboard_limit = 5

local scoreboard_text_style = {
    red = 255,
    green = 220,
    blue = 64,
    font_size = 25,
    bold = true,
    center_width = 120
}

local scoreboard_header_text_style = {
    red = 0,
    green = 255,
    blue = 255,
    font_size = 25,
    bold = true,
    center_width = 120
}

local scoreboard_columns = {
    {
        key = "name",
        header = "Name",
        width = 180
    },
    {
        key = "score",
        header = "Score",
        width = 120
    },
    {
        key = "hp",
        header = "HP",
        width = 80
    },
    {
        key = "doc",
        header = "Doc",
        width = 80
    },
    {
        key = "timer",
        header = "Timer",
        width = 120
    }
}

local scoreboard_items = {
    {
        line_id = "scoreboard_line_1",
        labels = {
            name = "scoreboard_name_1",
            score = "scoreboard_score_1",
            hp = "scoreboard_hp_1",
            doc = "scoreboard_doc_1",
            timer = "scoreboard_timer_1"
        }
    },
    {
        line_id = "scoreboard_line_2",
        labels = {
            name = "scoreboard_name_2",
            score = "scoreboard_score_2",
            hp = "scoreboard_hp_2",
            doc = "scoreboard_doc_2",
            timer = "scoreboard_timer_2"
        }
    },
    {
        line_id = "scoreboard_line_3",
        labels = {
            name = "scoreboard_name_3",
            score = "scoreboard_score_3",
            hp = "scoreboard_hp_3",
            doc = "scoreboard_doc_3",
            timer = "scoreboard_timer_3"
        }
    },
    {
        line_id = "scoreboard_line_4",
        labels = {
            name = "scoreboard_name_4",
            score = "scoreboard_score_4",
            hp = "scoreboard_hp_4",
            doc = "scoreboard_doc_4",
            timer = "scoreboard_timer_4"
        }
    },
    {
        line_id = "scoreboard_line_5",
        labels = {
            name = "scoreboard_name_5",
            score = "scoreboard_score_5",
            hp = "scoreboard_hp_5",
            doc = "scoreboard_doc_5",
            timer = "scoreboard_timer_5"
        }
    }
}

local function playBackgroundBGM()
    local sound_mgr = GetSoundManager()
    sound_mgr:LoadSound("backgroundbgm", "Asset/Content/Sounds/backgroundbgm.mp3", true)
    sound_mgr:PlayBGM("backgroundbgm")
end

local function formatTimer(total_seconds)
    total_seconds = math.floor((total_seconds or 0) + 0.5)
    local minutes = math.floor(total_seconds / 60)
    local seconds = total_seconds % 60

    return string.format("%02d:%02d", minutes, seconds)
end

local function setMenuText(item, highlighted)
    if highlighted then
        ui_document:SetTextStyle(
            item.label_id,
            item.text,
            menu_text_style.highlight_red,
            menu_text_style.highlight_green,
            menu_text_style.highlight_blue,
            menu_text_style.font_size,
            menu_text_style.bold,
            menu_text_style.center_width
        )
    else
        ui_document:SetTextStyle(
            item.label_id,
            item.text,
            menu_text_style.normal_red,
            menu_text_style.normal_green,
            menu_text_style.normal_blue,
            menu_text_style.font_size,
            menu_text_style.bold,
            menu_text_style.center_width
        )
    end
end

local function setCreditText(item)
    ui_document:SetTextStyle(
        item.label_id,
        item.text,
        credit_text_style.red,
        credit_text_style.green,
        credit_text_style.blue,
        credit_text_style.font_size,
        credit_text_style.bold,
        credit_text_style.center_width
    )
end

local function setScoreboardText(item, text, style)
    style = style or scoreboard_text_style

    ui_document:SetTextStyle(
        item.label_id,
        text,
        style.red,
        style.green,
        style.blue,
        style.font_size,
        style.bold,
        item.width or style.center_width
    )
end

-- scoreboard
local function getScoreEntryValues(entry)
    if entry == nil then
        return {
            name = "--",
            score = "--",
            hp = "--",
            doc = "--",
            timer = "--"
        }
    end

    local name = entry.name or "Player"
    local score = math.floor((entry.score or 0) + 0.5)
    local hp = math.floor((entry.hp or 0) + 0.5)
    local document_count = entry.documentCount or 0
    local timer = formatTimer(entry.timer)

    return {
        name = name,
        score = tostring(score),
        hp = tostring(hp),
        doc = tostring(document_count),
        timer = timer
    }
end

local function setScoreboardRow(item, values)
    for _, column in ipairs(scoreboard_columns) do
        setScoreboardText(
            {
                label_id = item.labels[column.key],
                width = column.width
            },
            values[column.key]
        )
    end
end

local function refreshScoreboard()
    local scores = Scoreboard.GetTopScores(scoreboard_limit)

    ui_document:SetProperty("scoreboard_header_line", "background-color", "transparent")
    ui_document:SetProperty("scoreboard_header_line", "border-width", "0px")
    ui_document:SetProperty("scoreboard_header_line", "top", "-38px")
    ui_document:SetProperty("scoreboard_header_line", "right", "220px")
    for _, column in ipairs(scoreboard_columns) do
        setScoreboardText(
            {
                label_id = "scoreboard_header_" .. column.key,
                width = column.width
            },
            column.header,
            scoreboard_header_text_style
        )
    end

    for index, item in ipairs(scoreboard_items) do
        local entry = scores[index]
        ui_document:SetProperty(item.line_id, "background-color", "transparent")
        ui_document:SetProperty(item.line_id, "border-width", "0px")
        ui_document:SetProperty(item.line_id, "top", tostring((index - 1) * 30) .. "px")
        ui_document:SetProperty(item.line_id, "right", tostring((index - 1) * 30) .. "px")
        setScoreboardRow(item, getScoreEntryValues(entry))
    end
end

local function setMainMenuVisible(visible)
    if visible then
        ui_document:SetProperty("menu", "display", "block")
        ui_document:SetProperty("credit_view", "display", "none")
        ui_document:SetProperty("scoreboard_view", "display", "none")
        current_screen = "menu"
    else
        ui_document:SetProperty("menu", "display", "none")
        ui_document:SetProperty("credit_view", "display", "block")
        ui_document:SetProperty("scoreboard_view", "display", "none")
        current_screen = "credit"
    end
end

local function setScoreboardVisible()
    ui_document:SetProperty("menu", "display", "none")
    ui_document:SetProperty("credit_view", "display", "none")
    ui_document:SetProperty("scoreboard_view", "display", "block")
    current_screen = "scoreboard"
    refreshScoreboard()
end

local function refreshMenuHighlight()
    if ui_document == nil or not ui_document:IsValid() then
        return
    end

    for index, item in ipairs(menu_items) do
        setMenuText(item, index == selected_index)
        ui_document:SetProperty(item.label_id, "top", "-7px")
    end
end

local function returnToMainMenu()
    setMainMenuVisible(true)
    refreshMenuHighlight()
end

local function moveSelection(delta)
    if current_screen ~= "menu" then
        return
    end

    selected_index = selected_index + delta
    if selected_index < 1 then
        selected_index = #menu_items
    elseif selected_index > #menu_items then
        selected_index = 1
    end

    refreshMenuHighlight()
end

local function activateSelected()
    if current_screen == "credit" or current_screen == "scoreboard" then
        returnToMainMenu()
        return
    end

    if selected_index == 1 then
        LoadScene("DroneLevel.Scene")
    elseif selected_index == 2 then
        setMainMenuVisible(false)
    elseif selected_index == 3 then
        setScoreboardVisible()
    end
end

local function handleBack()
    if current_screen == "credit" or current_screen == "scoreboard" then
        returnToMainMenu()
    end
end

local function isGamepadConnected()
    if Input.IsGamepadConnected == nil then
        return false
    end

    return Input:IsGamepadConnected(gamepad_controller_id)
end

local function updateGamepadStickMenu(dt)
    if current_screen ~= "menu" or not isGamepadConnected() then
        gamepad_stick_repeat_timer = 0.0
        gamepad_stick_direction = 0
        return
    end

    local y = Input:GetAxis("GamepadLeftY", gamepad_controller_id)
    local direction = 0
    if y > gamepad_stick_threshold then
        direction = -1
    elseif y < -gamepad_stick_threshold then
        direction = 1
    end

    if direction == 0 then
        gamepad_stick_repeat_timer = 0.0
        gamepad_stick_direction = 0
        return
    end

    if direction ~= gamepad_stick_direction then
        gamepad_stick_direction = direction
        gamepad_stick_repeat_timer = gamepad_stick_repeat_delay
        moveSelection(direction)
        return
    end

    gamepad_stick_repeat_timer = gamepad_stick_repeat_timer - dt
    if gamepad_stick_repeat_timer <= 0.0 then
        gamepad_stick_repeat_timer = gamepad_stick_repeat_delay
        moveSelection(direction)
    end
end

function BeginPlay()
    ui_document = UI.Load("UI/TextureBackground.rml", "texture_background")

    if ui_document == nil or not ui_document:IsValid() then
        return
    end
    World.PlayCameraEffectAsset("Asset/Content/CameraEffects/StartSceneLBStart.ceffect")
    ui_document:SetPosition(0, 0)
    ui_document:SetZOrder(0)
    ui_document:SetProperty("background", "display", "none")
    playBackgroundBGM()

    ui_document:SetProperty("game_start_button", "background-color", "rgb(0, 0, 0)")
    ui_document:SetProperty("credit_button", "background-color", "rgb(0, 0, 0)")
    ui_document:SetProperty("scoreboard_button", "background-color", "rgb(0, 0, 0)")

    ui_document:SetProperty("game_start_button", "border-width", "2px")
    ui_document:SetProperty("game_start_button", "border-color", "rgb(0, 220, 220)")
    ui_document:SetProperty("credit_button", "border-width", "2px")
    ui_document:SetProperty("credit_button", "border-color", "rgb(0, 220, 220)")
    ui_document:SetProperty("scoreboard_button", "border-width", "2px")
    ui_document:SetProperty("scoreboard_button", "border-color", "rgb(0, 220, 220)")

    for index, item in ipairs(credit_items) do
        ui_document:SetProperty(item.line_id, "background-color", "transparent")
        ui_document:SetProperty(item.line_id, "border-width", "0px")
        ui_document:SetProperty(item.line_id, "top", tostring((index - 1) * 50) .. "px")
        setCreditText(item)
    end

    refreshScoreboard()

    ui_document:SetProperty("game_start_button", "top", "230px")
    ui_document:SetProperty("credit_button", "top", "288px")
    ui_document:SetProperty("scoreboard_button", "top", "346px")

    selected_index = 1
    gamepad_stick_repeat_timer = 0.0
    gamepad_stick_direction = 0
    setMainMenuVisible(true)
    refreshMenuHighlight()

    ui_document:Show()
end

function EndPlay()
    if ui_document ~= nil and ui_document:IsValid() then
        ui_document:Close()
    end

    ui_document = nil
end

function Tick(dt)
    if ui_document == nil or not ui_document:IsValid() then
        return
    end

    updateGamepadStickMenu(dt)
end

function OnKeyPressed(key)
    if current_screen == "credit" or current_screen == "scoreboard" then
        if key == "Escape" or key == "Esc" or key == "Enter" then
            returnToMainMenu()
        end

        return
    end

    if key == "Up" then
        moveSelection(-1)
    elseif key == "Down" then
        moveSelection(1)
    elseif key == "Enter" then
        activateSelected()
    elseif key == "Escape" or key == "Esc" then
        handleBack()
    end
end

function OnGamepadButtonPressed(button, controller_id)
    if controller_id ~= nil and controller_id ~= gamepad_controller_id then
        return
    end

    if button == "DPadUp" then
        moveSelection(-1)
    elseif button == "DPadDown" then
        moveSelection(1)
    elseif button == "A" or button == "Start" then
        activateSelected()
    elseif button == "B" then
        handleBack()
    end
end
