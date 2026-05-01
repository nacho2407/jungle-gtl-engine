local whiteBox = nil
local contributorWidget = nil
local contributorWidgetVisible = false

function BeginPlay()
    whiteBox = UI.CreateWidget("Asset/UI/PIEWhiteBox.rml")
    if whiteBox ~= nil then
        whiteBox:show()
    end

    contributorWidget = UI.CreateWidget("Asset/UI/ContributorWidget.rml")
    if contributorWidget ~= nil then
        contributorWidget:bind_click("contributor-close-button", function()
            contributorWidgetVisible = false
            contributorWidget:hide()
        end)
    end
    contributorWidgetVisible = false
end

function EndPlay()
    -- if contributorWidget ~= nil then
    --     contributorWidget:hide()
    -- end
    -- if whiteBox ~= nil then
    --     whiteBox:hide()
    -- end
end

function OnOverlap(OtherActor)
end

function Tick(dt)
    if Input.GetKeyDown(Key.F1) and contributorWidget ~= nil then
        contributorWidgetVisible = not contributorWidgetVisible
        if contributorWidgetVisible then
            contributorWidget:show()
        else
            contributorWidget:hide()
        end
    end
end
