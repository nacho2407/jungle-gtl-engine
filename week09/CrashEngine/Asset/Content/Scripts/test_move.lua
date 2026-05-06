-- test_move.lua
-- Spring Arm / Camera Boom 테스트용 단순 이동 스크립트입니다.
--
-- 사용 방법:
-- 1. 테스트용 Cube Actor에 LuaScriptComponent를 붙입니다.
-- 2. Script Path를 "test_move.lua" 또는 "Asset/Content/Scripts/test_move.lua"로 지정합니다.
-- 3. Spring Arm이 이 Cube를 따라가게 하려면, 이 Actor가 "Player" 태그를 가지면 됩니다.
--    이 스크립트는 BeginPlay에서 자동으로 "Player" 태그를 붙입니다.
--
-- 조작:
-- W / S : +X / -X 이동
-- D / A : +Y / -Y 이동
-- Shift : 빠르게 이동
-- Q / E : Z축 회전 테스트
-- R     : 시작 위치로 리셋

local MoveSpeed = 20.0
local RunMultiplier = 3.0
local TurnSpeedDegrees = 120.0
local bAutoAddPlayerTag = true

local InitialLocation = nil
local InitialRotation = nil

local function safeNormalizePlanar(v)
    local planar = Vector.new(v.X, v.Y, 0.0)

    if planar:LengthSquared() > 0.0001 then
        return planar:Normalized()
    end

    return Vector.new(0.0, 0.0, 0.0)
end

local function getMoveInput()
    local direction = Vector.new(0.0, 0.0, 0.0)

    if Input:IsKeyDown("W") or Input:IsKeyDown("Up") then
        direction = direction + Vector.new(1.0, 0.0, 0.0)
    end

    if Input:IsKeyDown("S") or Input:IsKeyDown("Down") then
        direction = direction + Vector.new(-1.0, 0.0, 0.0)
    end

    if Input:IsKeyDown("D") or Input:IsKeyDown("Right") then
        direction = direction + Vector.new(0.0, 1.0, 0.0)
    end

    if Input:IsKeyDown("A") or Input:IsKeyDown("Left") then
        direction = direction + Vector.new(0.0, -1.0, 0.0)
    end

    return safeNormalizePlanar(direction)
end

local function getSpeed()
    if Input:IsKeyDown("Shift") then
        return MoveSpeed * RunMultiplier
    end

    return MoveSpeed
end

local function updateRotation(dt)
    local rotation = obj.Rotation
    local yawDelta = 0.0

    if Input:IsKeyDown("Q") then
        yawDelta = yawDelta - TurnSpeedDegrees * dt
    end

    if Input:IsKeyDown("E") then
        yawDelta = yawDelta + TurnSpeedDegrees * dt
    end

    if yawDelta ~= 0.0 then
        obj.Rotation = Vector.new(rotation.X, rotation.Y, rotation.Z + yawDelta)
    end
end

function BeginPlay()
    InitialLocation = obj.Location
    InitialRotation = obj.Rotation
    obj.Velocity = Vector.new(0.0, 0.0, 0.0)

    if bAutoAddPlayerTag and not obj:HasTag("Player") then
        obj:AddTag("Player")
    end
end

function Tick(dt)
    if Input:WasKeyPressed("R") then
        obj.Location = InitialLocation
        obj.Rotation = InitialRotation
        obj.Velocity = Vector.new(0.0, 0.0, 0.0)
        return
    end

    local moveDirection = getMoveInput()
    local velocity = moveDirection * getSpeed()
    local moveDelta = velocity * dt

    obj.Velocity = velocity

    if moveDelta:LengthSquared() > 0.0 then
        obj.Location = obj.Location + moveDelta
    end

    updateRotation(dt)
end

function EndPlay()
    obj.Velocity = Vector.new(0.0, 0.0, 0.0)
end