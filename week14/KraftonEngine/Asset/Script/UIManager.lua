local whiteBox = nil
local settingPanel = nil
local settingPanelVisible = false

function BeginPlay()
    whiteBox = UI.CreateWidget("Asset/UI/PIEWhiteBox.rml")
    if whiteBox ~= nil then
        whiteBox:show()
    end

    settingPanel = UI.CreateWidget("Asset/UI/SettingPanel.rml")
    settingPanelVisible = false
end

function EndPlay()
    if settingPanel ~= nil then
        settingPanel:hide()
    end
    if whiteBox ~= nil then
        whiteBox:hide()
    end
end

function OnOverlap(OtherActor)
end

function Tick(dt)
    if Input.GetKeyDown(Key.F1) and settingPanel ~= nil then
        settingPanelVisible = not settingPanelVisible
        if settingPanelVisible then
            settingPanel:show()
        else
            settingPanel:hide()
        end
    end
end
