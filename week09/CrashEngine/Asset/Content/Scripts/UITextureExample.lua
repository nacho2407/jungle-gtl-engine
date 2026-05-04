local ui_document = nil

function BeginPlay()
    ui_document = UI.Load("UI/LuaTextureExample.rml", "lua_texture_example")

    if ui_document == nil or not ui_document:IsValid() then
        return
    end

    ui_document:SetPosition(0, 0)
    ui_document:SetZOrder(1000)
    ui_document:SetTexture("preview", "Textures/brick_diff-512.png")
    ui_document:SetText("greeting", "안녕하세요")
    ui_document:Show()
end

function EndPlay()
    if ui_document ~= nil and ui_document:IsValid() then
        ui_document:Close()
    end

    ui_document = nil
end
