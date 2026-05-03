local ObjRegistry = require("ObjRegistry")
local movement = nil
local manCamera = nil

function BeginPlay()
    movement = obj:GetFloatingPawnMovement()
    manCamera = obj:GetCamera()
    if manCamera ~= nil then
        ObjRegistry.RegisterManCamera(manCamera)
    end
end

function EndPlay()
    print("[EndPlay] " .. obj.UUID)
end

function OnOverlap(OtherActor)
end

function Tick(dt)
    if movement == nil then
        return
    end

    local gs = GetGameState()
    if gs == nil then return false end

    if gs:GetPhase() ~= ECarGamePhase.CarWash and gs:GetPhase() ~= ECarGamePhase.CarGas then
        movement:SetMoveInput(0)
        movement:SetLookInput(0, 0)
        return
    end

    local move = 0
    if Input.GetKey(Key.W) then move = move + 1 end
    if Input.GetKey(Key.S) then move = move - 1 end

    local right = 0
    if Input.GetKey(Key.D) then right = right + 1 end
    if Input.GetKey(Key.A) then right = right - 1 end

    movement:SetMoveInput(move, right)
    movement:SetLookInput(Input.GetMouseDeltaX(), Input.GetMouseDeltaY())
end
