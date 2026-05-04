local BulletSystem = dofile("Asset/Content/Scripts/BulletSystem.lua")

local PatrolAgent = nil
local PatrolTargets = {}
local CurrentTargetIndex = 1
local bPatrolFinished = false
local ShotTimer = 0.0
local MaxHP = 2
local HP = MaxHP

local AttackRange = 25.0
local ShotCooldown = 1.5
local BulletSpawnHeight = 1.5
local BulletSpawnForwardOffset = 2.0
local PlayerAimOffset = Vector.new(0.0, 0.0, -1.0)

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
    obj.Rotation = Vector.new(0.0, 0.0, yaw)
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
    print("[Monster] Fire EnemyBullet", "Monster:", obj.UUID, "Spawn:", spawnLocation, "Direction:", direction)
    local bullet = BulletSystem.SpawnBullet(World, spawnLocation, direction, "EnemyBullet", obj)
    if bullet == nil or not bullet:IsValid() then
        print("[Monster] Fire failed", "Monster:", obj.UUID)
        return
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
        print("World.FindActorsByTag is not registered")
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
        print("MonsterPatrol requires PatrolAgentComponent")
        return
    end

    HP = MaxHP
    CurrentTargetIndex = 1
    bPatrolFinished = false
    ShotTimer = 0.0
    collectPatrolTargets()

    if #PatrolTargets == 0 then
        print("MonsterPatrol found no PatrolTarget for group : ", PatrolAgent:GetPatrolGroup())
    end
end

function OnOverlapBegin(other)
    if other == nil or not other:IsValid() then
        return
    end

    if other:HasTag("Bullet") then
        print("[Monster] Overlap bullet", "Monster:", obj.UUID, "Bullet:", other.UUID, "PlayerBullet:", other:HasTag("PlayerBullet"), "EnemyBullet:", other:HasTag("EnemyBullet"))
    end

    if not other:HasTag("PlayerBullet") then
        return
    end

    if other:HasTag("DamageApplied") then
        return
    end

    other:AddTag("DamageApplied")
    obj:ApplyDamage(1, other)
    World.DestroyActor(other)
end

function OnTakeDamage(damage, instigator)
    HP = HP - damage
    print("Monster HP : ", HP)

    if HP <= 0 then
        print("Monster Destroyed")
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
end
