local UIManager = require("UIManager")
local car = nil
local wasFirstPersonView = false

function BeginPlay()
    car = obj:AsCarPawn()
    if car == nil then
        return
    end
end

function EndPlay()
end

function Tick(dt)
    if Input.GetKeyDown(Key.F2) then
        CameraManager.ToggleOwnerCamera(obj)
    end

    if car == nil then
        return
    end

    local isFirstPersonView = car:IsFirstPersonView()
    if isFirstPersonView and not wasFirstPersonView then
        UIManager.Show("gasWidget")
    elseif not isFirstPersonView and wasFirstPersonView then
        UIManager.Hide("gasWidget")
    end

    wasFirstPersonView = isFirstPersonView
end
