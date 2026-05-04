local BulletSystem = dofile("Asset/Content/Scripts/BulletSystem.lua")
BulletSystem.BindWorld(World)

local DetectRange = 30.0
local ShotCooldown = 1.5
local ShotTimer = 0.0
local MaxHP = 3
local HP = MaxHP
local BaseRotation = Vector.new(0.0, 0.0, 0.0)

local DamagedEffectMaterial = "Asset/Content/Materials/subUV_Damaged.json"
local DamagedEffectLifeTime = 0.5
local DamagedEffectRow = 1
local DamagedEffectColumn = 1
local DestroyEffectMaterial = "Asset/Content/Materials/subUV_sample.json"
local DestroyEffectLifeTime = 1.2
local DestroyEffectRow = 6
local DestroyEffectColumn = 6

local function spawnEffect(location, materialPath, lifeTime, row, column)
    local effectActor = World.SpawnActor("SubUVActor")
    if effectActor == nil or not effectActor:IsValid() then
        print("Failed to spawn SubUVActor effect")
        return
    end

    local script = effectActor:AddComponent("LuaScriptComponent")
    if script == nil or not script:IsValid() then
        print("Failed to add Effect.lua")
        World.DestroyActor(effectActor)
        return
    end

    script:SetScriptPath("Effect.lua")
    script:CallFunction("InitEffect", location, lifeTime, materialPath, row, column)
end

local function atan2(y, x)
    if math.atan2 ~= nil then
        return math.atan2(y, x)
    end

    if x > 0.0 then
        return math.atan(y / x)
    elseif x < 0.0 and y >= 0.0 then
        return math.atan(y / x) + math.pi
    elseif x < 0.0 and y < 0.0 then
        return math.atan(y / x) - math.pi
    elseif x == 0.0 and y > 0.0 then
        return math.pi * 0.5
    elseif x == 0.0 and y < 0.0 then
        return -math.pi * 0.5
    end

    return 0.0
end

local function findPlayer()
    local player = World.FindPlayer()
    if player ~= nil and player:IsValid() then
        return player
    end

    return nil
end

local function lookAtDirection(direction)
    local planarDirection = Vector.new(direction.X, direction.Y, 0.0)
    if planarDirection:LengthSquared() <= 0.0001 then
        return
    end

    local yaw = math.deg(atan2(planarDirection.Y, planarDirection.X))
    obj.Rotation = Vector.new(BaseRotation.X, BaseRotation.Y, yaw)
end

local function fireAtPlayer(player)
    local targetLocation = player.Location + Vector.new(0.0, 0.0, 1.0)
    local toPlayer = targetLocation - obj.Location

    if toPlayer:LengthSquared() <= 0.0001 then
        return
    end

    local direction = toPlayer:Normalized()
    lookAtDirection(direction)

    local spawnLocation = obj.Location + Vector.new(0.0, 0.0, 1.5) + direction * 2.0
    BulletSystem.SpawnBullet(spawnLocation, direction, "EnemyBullet")
    ShotTimer = ShotCooldown
end

function BeginPlay()
    HP = MaxHP
    ShotTimer = 0.0
    BaseRotation = obj.Rotation
end

function EndPlay()
    ShotTimer = 0.0
end

function OnTakeDamage(damage, instigator)
    HP = HP - damage
    print("Turret HP : ", HP)
    spawnEffect(obj.Location, DamagedEffectMaterial, DamagedEffectLifeTime, DamagedEffectRow, DamagedEffectColumn)

    if HP <= 0 then
        print("Turret Destroyed")
        spawnEffect(obj.Location, DestroyEffectMaterial, DestroyEffectLifeTime, DestroyEffectRow, DestroyEffectColumn)
        World.DestroyActor(obj)
    end
end

function Tick(dt)
    ShotTimer = ShotTimer - dt
    if ShotTimer < 0.0 then
        ShotTimer = 0.0
    end

    local player = findPlayer()
    if player == nil then
        return
    end

    local toPlayer = player.Location - obj.Location
    if toPlayer:LengthSquared() > DetectRange * DetectRange then
        return
    end

    lookAtDirection(toPlayer:Normalized())

    if ShotTimer <= 0.0 then
        fireAtPlayer(player)
    end
end
