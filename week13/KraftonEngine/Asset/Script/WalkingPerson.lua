local ObjRegistry = require("ObjRegistry")

local root = nil
local isMoving = true

local SPEED = 2.5
local TURN_INTERVAL = 30.0

local function GetFlatForward()
    local forward = obj.Forward
    forward.Z = 0

    if forward:Length() <= 0.001 then
        return Vector.Forward()
    end

    return forward:Normalized()
end

function BeginPlay()
    root = obj:GetRootPrimitiveComponent()
    if root == nil then
        root = obj:GetPrimitiveComponent()
    end

    if root ~= nil then
        root:SetSimulatePhysics(true)
    end

    StartCoroutine(function()
        while isMoving do
            Wait(TURN_INTERVAL)
            if isMoving then
                local rotation = obj.Rotation
                obj.Rotation = Vector.new(rotation.X, rotation.Y, rotation.Z + 180.0)
            end
        end
    end)
end

function EndPlay()
end

function OnOverlap(OtherActor)
end

function OnHit(OtherActor, HitComponent, OtherComp, NormalImpulse, Hit)
    if ObjRegistry.car == nil or OtherActor == nil or OtherActor.UUID ~= ObjRegistry.car.UUID then
        return
    end

    isMoving = false

    if root ~= nil then
        root:SetLinearVelocity(Vector.Zero())
        root:SetAngularVelocity(Vector.Zero())
    end
end

function Tick(dt)
    UpdateCoroutines(dt)

    if root == nil or not isMoving then
        return
    end

    local forward = GetFlatForward()

    local velocity = root:GetLinearVelocity()
    root:SetLinearVelocity(Vector.new(forward.X * SPEED, forward.Y * SPEED, velocity.Z))
    root:SetAngularVelocity(Vector.Zero())
end
