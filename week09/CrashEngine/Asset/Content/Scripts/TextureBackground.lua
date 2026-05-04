local ui_document = nil

local selected_index = 1

local menu_items = {
    {
        label_id = "game_start_label",
        text = "Game Start",
        left = "132px"
    },
    {
        label_id = "credit_label",
        text = "Credit",
        left = "166px"
    },
    {
        label_id = "scoreboard_label",
        text = "Scoreboard",
        left = "118px"
    }
}

local function refreshMenuHighlight()
    if ui_document == nil or not ui_document:IsValid() then
        return
    end

    for index, item in ipairs(menu_items) do
        if index == selected_index then
            ui_document:SetTextColor(item.label_id, item.text, 255, 220, 64)
        else
            ui_document:SetTextColor(item.label_id, item.text, 255, 255, 255)
        end

        ui_document:SetProperty(item.label_id, "top", "5px")
        ui_document:SetProperty(item.label_id, "left", item.left)
    end
end

function BeginPlay()
    ui_document = UI.Load("UI/TextureBackground.rml", "texture_background")

    if ui_document == nil or not ui_document:IsValid() then
        return
    end

    ui_document:SetPosition(0, 0)
    ui_document:SetZOrder(0)
    ui_document:SetTexture("background", "Textures/background.jpg")

    ui_document:SetProperty("game_start_button", "background-color", "rgb(0, 0, 0)")
    ui_document:SetProperty("credit_button", "background-color", "rgb(0, 0, 0)")
    ui_document:SetProperty("scoreboard_button", "background-color", "rgb(0, 0, 0)")

    ui_document:SetProperty("game_start_button", "top", "235px")
    ui_document:SetProperty("credit_button", "top", "313px")
    ui_document:SetProperty("scoreboard_button", "top", "391px")

    selected_index = 1
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
    end
end
