local BulletSystem = dofile("Asset/Content/Scripts/BulletSystem.lua")
BulletSystem.BindWorld(World, "Monster")

local PatrolAgent = nil
local PatrolTargets = {}
local CurrentTargetIndex = 1
local bPatrolFinished = false
local ShotTimer = 0.0
local MaxHP = 2
local HP = MaxHP
local BaseRotation = Vector.new(0.0, 0.0, 0.0)

local AttackRange = 25.0
local ShotCooldown = 1.5
local BulletSpawnHeight = 1.5
local BulletSpawnForwardOffset = 2.0
local PlayerAimOffset = Vector.new(0.0, 0.0, -1.0)
local DamagedEffectMaterial = "Asset/Content/Materials/subUV_Damaged.json"
local DamagedEffectLifeTime = 0.5
local DamagedEffectRow = 1
local DamagedEffectColumn = 1
local DestroyEffectMaterial = "Asset/Content/Materials/subUV_sample.json"
local DestroyEffectLifeTime = 1.2
local DestroyEffectRow = 6
local DestroyEffectColumn = 6
local SoundManager = nil
local SoundsLoaded = false

local function loadMonsterSounds()
    SoundManager = GetSoundManager()
    if SoundManager == nil or not SoundManager:IsInitialized() then
        SoundsLoaded = false
        return false
    end

    local shootLoaded = SoundManager:LoadSound("NimoShootSFX", "Asset/Content/Sounds/SoundCollection/NimoShootSFX.mp3", false)
    local killedLoaded = SoundManager:LoadSound("NimoKilledSFX", "Asset/Content/Sounds/SoundCollection/NimoKilledSFX.mp3", false)
    local hitLoaded = SoundManager:LoadSound("HitEnemySFX", "Asset/Content/Sounds/SoundCollection/HitEnemySFX.mp3", false)
    SoundsLoaded = shootLoaded and killedLoaded and hitLoaded
    return SoundsLoaded
end

local function playMonsterSound(sound_id)
    if not SoundsLoaded and not loadMonsterSounds() then
        return
    end

    SoundManager:PlaySFX(sound_id)
end

local function spawnEffect(location, materialPath, lifeTime, row, column)
    local effectActor = World.SpawnActor("SubUVActor")
    if effectActor == nil or not effectActor:IsValid() then
        return
    end

    local script = effectActor:AddComponent("LuaScriptComponent")
    if script == nil or not script:IsValid() then
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

local function lookAtDirection(direction)
    local planarDirection = Vector.new(direction.X, direction.Y, 0.0)
    if planarDirection:LengthSquared() <= 0.0001 then
        return
    end

    local yaw = math.deg(atan2(planarDirection.Y, planarDirection.X))
    obj.Rotation = Vector.new(BaseRotation.X, BaseRotation.Y, BaseRotation.Z + yaw)
end

local function findPlayer()
    local player = World.FindPlayer()
    if player ~= nil and player:IsValid() then
        return player
    end

    return nil
end

local function getDirectionToPlayer(player)
    if player == nil or not player:IsValid() then
        return nil
    end

    local toPlayer = player.Location - obj.Location
    if toPlayer:LengthSquared() <= 0.0001 then
        return nil
    end

    return toPlayer
end

local function fireAtPlayer(player)
    if ShotTimer > 0.0 then
        return
    end

    local targetLocation = player.Location + PlayerAimOffset
    local toPlayer = targetLocation - obj.Location
    if toPlayer:LengthSquared() <= 0.0001 then
        return
    end

    local direction = toPlayer:Normalized()
    lookAtDirection(direction)

    local spawnLocation = obj.Location + Vector.new(0.0, 0.0, BulletSpawnHeight) + direction * BulletSpawnForwardOffset

    local bullet = BulletSystem.SpawnBullet(spawnLocation, direction, "EnemyBullet", obj)
    if bullet ~= nil and bullet:IsValid() then
        playMonsterSound("NimoShootSFX")
    end
    ShotTimer = ShotCooldown
end

local function addTargetIfMatches(target, patrolGroup)
    local point = target:GetComponent("PatrolPointComponent", 0)
    if not point:IsValid() then
        return
    end

    if point:GetPatrolGroup() ~= patrolGroup then
        return
    end

    PatrolTargets[#PatrolTargets + 1] = {
        Actor = target,
        Order = point:GetPatrolOrder()
    }
end

local function collectPatrolTargets()
    PatrolTargets = {}

    if PatrolAgent == nil or not PatrolAgent:IsValid() then
        return
    end

    if World.FindActorsByTag == nil then
        return
    end

    local patrolGroup = PatrolAgent:GetPatrolGroup()
    local candidates = World.FindActorsByTag("PatrolTarget")
    for i, target in ipairs(candidates) do
        if target:IsValid() then
            addTargetIfMatches(target, patrolGroup)
        end
    end

    table.sort(PatrolTargets, function(a, b)
        return a.Order < b.Order
    end)
end

local function advanceTarget()
    CurrentTargetIndex = CurrentTargetIndex + 1

    if CurrentTargetIndex <= #PatrolTargets then
        return
    end

    if PatrolAgent:IsPatrolLoop() then
        CurrentTargetIndex = 1
    else
        CurrentTargetIndex = #PatrolTargets
        bPatrolFinished = true
    end
end

local function tickPatrol(dt)
    if bPatrolFinished then
        return
    end

    if #PatrolTargets == 0 then
        return
    end

    local targetInfo = PatrolTargets[CurrentTargetIndex]
    if targetInfo == nil or targetInfo.Actor == nil or not targetInfo.Actor:IsValid() then
        advanceTarget()
        return
    end

    local target = targetInfo.Actor
    local toTarget = target.Location - obj.Location
    local reachDistance = PatrolAgent:GetPatrolReachDistance()

    if toTarget:LengthSquared() <= reachDistance * reachDistance then
        advanceTarget()
        return
    end

    local direction = toTarget:Normalized()
    lookAtDirection(direction)

    local moveDelta = direction * PatrolAgent:GetPatrolMoveSpeed() * dt
    World.MoveActorWithBlock(obj, moveDelta, "Wall")
end

local function tickAttack(player, toPlayer)
    if toPlayer == nil then
        return
    end

    lookAtDirection(toPlayer:Normalized())
    fireAtPlayer(player)
end

function BeginPlay()
    obj:AddTag("Monster")
    PatrolAgent = obj:GetComponent("PatrolAgentComponent", 0)
    if not PatrolAgent:IsValid() then
        return
    end

    HP = MaxHP
    BaseRotation = obj.Rotation
    CurrentTargetIndex = 1
    bPatrolFinished = false
    ShotTimer = 0.0
    loadMonsterSounds()
    collectPatrolTargets()

    if #PatrolTargets == 0 then
    end
end

function OnOverlapBegin(other)
    if other == nil or not other:IsValid() then
        return
    end

    if other:HasTag("Bullet") then
    end

    if not other:HasTag("PlayerBullet") then
        return
    end

    if other:HasTag("DamageApplied") then
        return
    end

    other:AddTag("DamageApplied")
    playMonsterSound("HitEnemySFX")
    obj:ApplyDamage(1, other)
    World.DestroyActor(other)
end

function OnTakeDamage(damage, instigator)
    HP = HP - damage
    spawnEffect(obj.Location, DamagedEffectMaterial, DamagedEffectLifeTime, DamagedEffectRow, DamagedEffectColumn)

    if HP <= 0 then
        playMonsterSound("NimoKilledSFX")
        local effectLocation = obj.Location + Vector.new(0.0, 0.0, 5.0)
        spawnEffect(effectLocation, DestroyEffectMaterial, DestroyEffectLifeTime, DestroyEffectRow, DestroyEffectColumn)
        World.DestroyActor(obj)
    end
end

function Tick(dt)
    if PatrolAgent == nil or not PatrolAgent:IsValid() then
        return
    end

    ShotTimer = ShotTimer - dt
    if ShotTimer < 0.0 then
        ShotTimer = 0.0
    end

    local player = findPlayer()
    local toPlayer = getDirectionToPlayer(player)
    local playerDistanceSq = nil

    if toPlayer ~= nil then
        playerDistanceSq = toPlayer:LengthSquared()
    end

    if playerDistanceSq ~= nil and playerDistanceSq <= AttackRange * AttackRange then
        tickAttack(player, toPlayer)
        return
    end

    tickPatrol(dt)
end

function EndPlay()
    PatrolAgent = nil
    PatrolTargets = {}
    CurrentTargetIndex = 1
    bPatrolFinished = false
    ShotTimer = 0.0
    BaseRotation = Vector.new(0.0, 0.0, 0.0)
end
