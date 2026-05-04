local ui_document = nil

local selected_index = 1
local current_screen = "menu"
local main_background_texture = "Textures/background.jpg"
local sub_background_texture = "Textures/Credit.jpg"

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
    font_size = 20,
    bold = true,
    center_width = 300
}

local scoreboard_limit = 5

local scoreboard_text_style = {
    red = 255,
    green = 220,
    blue = 64,
    font_size = 18,
    bold = true,
    center_width = 520
}

local scoreboard_items = {
    {
        line_id = "scoreboard_line_1",
        label_id = "scoreboard_text_1"
    },
    {
        line_id = "scoreboard_line_2",
        label_id = "scoreboard_text_2"
    },
    {
        line_id = "scoreboard_line_3",
        label_id = "scoreboard_text_3"
    },
    {
        line_id = "scoreboard_line_4",
        label_id = "scoreboard_text_4"
    },
    {
        line_id = "scoreboard_line_5",
        label_id = "scoreboard_text_5"
    }
}

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

local function setScoreboardText(item, text)
    ui_document:SetTextStyle(
        item.label_id,
        text,
        scoreboard_text_style.red,
        scoreboard_text_style.green,
        scoreboard_text_style.blue,
        scoreboard_text_style.font_size,
        scoreboard_text_style.bold,
        scoreboard_text_style.center_width
    )
end

-- scoreboard
local function formatScoreEntry(entry)
    if entry == nil then
        return "--"
    end

    local rank = entry.rank or 0
    local name = entry.name or "Player"
    local score = math.floor((entry.score or 0) + 0.5)
    local hp = math.floor((entry.hp or 0) + 0.5)
    local document_count = entry.documentCount or 0

    return string.format("%d. %s  Score %d  HP %d  Doc %d", rank, name, score, hp, document_count)
end

local function refreshScoreboard()
    local scores = Scoreboard.GetTopScores(scoreboard_limit)

    for index, item in ipairs(scoreboard_items) do
        local entry = scores[index]
        ui_document:SetProperty(item.line_id, "background-color", "transparent")
        ui_document:SetProperty(item.line_id, "border-width", "0px")
        ui_document:SetProperty(item.line_id, "top", tostring(250 + (index - 1) * 30) .. "px")
        ui_document:SetProperty(item.line_id, "right", tostring(250 + (index - 1) * 30) .. "px")
        setScoreboardText(item, formatScoreEntry(entry))
    end
end

local function setMainMenuVisible(visible)
    if visible then
        ui_document:SetTexture("background", main_background_texture)
        ui_document:SetProperty("menu", "display", "block")
        ui_document:SetProperty("credit_view", "display", "none")
        ui_document:SetProperty("scoreboard_view", "display", "none")
        current_screen = "menu"
    else
        ui_document:SetTexture("background", sub_background_texture)
        ui_document:SetProperty("menu", "display", "none")
        ui_document:SetProperty("credit_view", "display", "block")
        ui_document:SetProperty("scoreboard_view", "display", "none")
        current_screen = "credit"
    end
end

local function setScoreboardVisible()
    ui_document:SetTexture("background", sub_background_texture)
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

function BeginPlay()
    ui_document = UI.Load("UI/TextureBackground.rml", "texture_background")

    if ui_document == nil or not ui_document:IsValid() then
        return
    end

    ui_document:SetPosition(0, 0)
    ui_document:SetZOrder(0)
    ui_document:SetTexture("background", main_background_texture)

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
        ui_document:SetProperty(item.line_id, "top", tostring(250 + (index - 1) * 30) .. "px")
        setCreditText(item)
    end

    refreshScoreboard()

    ui_document:SetProperty("game_start_button", "top", "230px")
    ui_document:SetProperty("credit_button", "top", "288px")
    ui_document:SetProperty("scoreboard_button", "top", "346px")

    selected_index = 1
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

function OnKeyPressed(key)
    if current_screen == "credit" or current_screen == "scoreboard" then
        if key == "Escape" or key == "Esc" or key == "Enter" then
            setMainMenuVisible(true)
            refreshMenuHighlight()
        end

        return
    end

    if key == "Up" then
        selected_index = selected_index - 1
        if selected_index < 1 then
            selected_index = #menu_items
        end

        refreshMenuHighlight()
    elseif key == "Down" then
        selected_index = selected_index + 1
        if selected_index > #menu_items then
            selected_index = 1
        end

        refreshMenuHighlight()
    elseif key == "Enter" then
        if selected_index == 1 then
            LoadScene("PlayerTest.Scene")
        elseif selected_index == 2 then
            setMainMenuVisible(false)
        elseif selected_index == 3 then
            setScoreboardVisible()
        end
    end
end
