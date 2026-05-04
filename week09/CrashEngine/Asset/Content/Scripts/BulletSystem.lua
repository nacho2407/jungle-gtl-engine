local BulletSystem = _G.BulletSystem or {}
local DebugLog = dofile("Asset/Content/Scripts/DebugLog.lua")
DebugLog.SessionStart("BulletSystem")

local BulletMeshPath = "Asset/Content/Models/Bullet/Bullet.obj"
local BulletSpeed = 90.0
local BulletLifeTime = 3.0
local BulletScale = Vector.new(0.15, 0.15, 0.15)
local BulletColliderRadius = 0.4
local BulletMeshRollOffset = 0.0
local BulletMeshYawOffset = 180.0
local BulletSpawnBlockProbeDistance = 0.05

local bullets = BulletSystem.Bullets or {}
BulletSystem.Bullets = bullets

local function isWorldUsable(world)
    if world == nil or world.SpawnActor == nil then
        return false
    end

    if world.IsValid ~= nil and not world.IsValid() then
        return false
    end

    return true
end

local function getWorld()
    if isWorldUsable(BulletSystem.World) then
        return BulletSystem.World
    end

    return World
end

function BulletSystem.BindWorld(world, ownerLabel)
    ownerLabel = ownerLabel or "unknown"
    local worldChanged = BulletSystem.World ~= world
    local currentWorldValid = isWorldUsable(BulletSystem.World)
    local incomingWorldValid = isWorldUsable(world)
    local shouldBind = incomingWorldValid and (not currentWorldValid or ownerLabel == "Player" or BulletSystem.WorldOwner ~= "Player")

    print("[BulletSystem] BindWorld", "Owner:", ownerLabel, "WorldChanged:", worldChanged, "ShouldBind:", shouldBind, "BulletCount:", #bullets)
    DebugLog.Write(
        "BulletSystem.BindWorld",
        "Owner=" .. ownerLabel,
        "WorldChanged=" .. tostring(worldChanged),
        "ShouldBind=" .. tostring(shouldBind),
        "BulletCount=" .. tostring(#bullets),
        "IncomingWorldHasSpawnActor=" .. tostring(world ~= nil and world.SpawnActor ~= nil),
        "IncomingWorldValid=" .. tostring(incomingWorldValid),
        "PreviousWorldOwner=" .. tostring(BulletSystem.WorldOwner),
        "PreviousWorldHasSpawnActor=" .. tostring(BulletSystem.World ~= nil and BulletSystem.World.SpawnActor ~= nil),
        "PreviousWorldValid=" .. tostring(currentWorldValid)
    )

    if not shouldBind then
        return
    end

    if worldChanged then
        BulletSystem.Clear(false)
    end

    BulletSystem.World = world
    BulletSystem.WorldOwner = ownerLabel
end

function BulletSystem.Clear(destroyActors)
    DebugLog.Write("BulletSystem.Clear", "DestroyActors=" .. tostring(destroyActors), "BeforeCount=" .. tostring(#bullets))

    if destroyActors then
        local world = getWorld()
        for i = #bullets, 1, -1 do
            local bulletInfo = bullets[i]
            if bulletInfo ~= nil and bulletInfo.Actor ~= nil and bulletInfo.Actor:IsValid() then
                DebugLog.Write(
                    "BulletSystem.Clear.Destroy",
                    "Index=" .. tostring(i),
                    "Bullet=" .. DebugLog.Actor(bulletInfo.Actor),
                    "OwnerTag=" .. tostring(bulletInfo.OwnerTag)
                )
                world.DestroyActor(bulletInfo.Actor)
            end
        end
    end

    bullets = {}
    BulletSystem.Bullets = bullets
end

local function normalize(vector)
    if vector ~= nil and vector:LengthSquared() > 0.0001 then
        return vector:Normalized()
    end

    return Vector.new(1.0, 0.0, 0.0)
end

local function directionToBulletRotation(direction)
    local planar = Vector.new(direction.X, direction.Y, 0.0)
    if planar:LengthSquared() <= 0.0001 then
        planar = Vector.new(1.0, 0.0, 0.0)
    else
        planar = planar:Normalized()
    end

    local yaw = math.deg(math.atan2(planar.Y, planar.X)) + BulletMeshYawOffset
    return Vector.new(BulletMeshRollOffset, 0.0, yaw)
end

local function addBulletVisual(actor)
    local mesh = actor:AddComponent("StaticMeshComponent")
    if mesh:IsValid() then
        mesh:SetStaticMesh(BulletMeshPath)
        mesh:SetRelativeScale(BulletScale)
    end

    local collider = actor:AddComponent("SphereComponent")
    if collider:IsValid() then
        collider:SetSphereRadius(BulletColliderRadius)
        collider:SetVisible(false)
        collider:SetVisibleInEditor(false)
        collider:SetVisibleInGame(false)
    end

    return mesh
end

function BulletSystem.SpawnBullet(location, direction, ownerTag, instigator)
    local world = getWorld()
    local instigatorId = "nil"
    if instigator ~= nil then
        instigatorId = instigator.UUID
    end

    print("[BulletSystem] SpawnBullet Request", "OwnerTag:", ownerTag or "nil", "Instigator:", instigatorId, "Location:", location, "Direction:", direction)
    DebugLog.Write(
        "BulletSystem.Spawn.Request",
        "OwnerTag=" .. tostring(ownerTag),
        "Instigator=" .. DebugLog.Actor(instigator),
        "Location=" .. DebugLog.Vector(location),
        "Direction=" .. DebugLog.Vector(direction),
        "WorldHasSpawnActor=" .. tostring(world ~= nil and world.SpawnActor ~= nil),
        "WorldHasMoveActorWithBlock=" .. tostring(world ~= nil and world.MoveActorWithBlock ~= nil),
        "BulletCountBefore=" .. tostring(#bullets)
    )

    if world == nil or world.SpawnActor == nil then
        print("[BulletSystem] SpawnBullet Failed: invalid world")
        DebugLog.Write("BulletSystem.Spawn.Fail", "Reason=InvalidWorld", "OwnerTag=" .. tostring(ownerTag), "Instigator=" .. DebugLog.Actor(instigator))
        return nil
    end

    local bullet = world.SpawnActor("StaticMeshActor")
    if bullet == nil or not bullet:IsValid() then
        print("[BulletSystem] SpawnBullet Failed: SpawnActor returned invalid actor")
        DebugLog.Write("BulletSystem.Spawn.Fail", "Reason=InvalidSpawnActorResult", "OwnerTag=" .. tostring(ownerTag), "Instigator=" .. DebugLog.Actor(instigator), "Bullet=" .. DebugLog.Actor(bullet))
        return nil
    end

    local shotDirection = normalize(direction)
    DebugLog.Write(
        "BulletSystem.Spawn.ActorCreated",
        "Bullet=" .. DebugLog.Actor(bullet),
        "ShotDirection=" .. DebugLog.Vector(shotDirection),
        "Scale=" .. DebugLog.Vector(BulletScale),
        "ColliderRadius=" .. tostring(BulletColliderRadius),
        "ProbeDistance=" .. tostring(BulletSpawnBlockProbeDistance)
    )
    bullet:AddTag("Bullet")

    if ownerTag ~= nil then
        bullet:AddTag(ownerTag)
    end

    local mesh = addBulletVisual(bullet)
    bullet.Location = location
    DebugLog.Write(
        "BulletSystem.Spawn.VisualReady",
        "Bullet=" .. DebugLog.Actor(bullet),
        "MeshValid=" .. tostring(mesh ~= nil and mesh:IsValid()),
        "BulletLocation=" .. DebugLog.Vector(bullet.Location),
        "OwnerTag=" .. tostring(ownerTag)
    )

    if mesh:IsValid() then
        mesh:SetRelativeRotation(directionToBulletRotation(shotDirection))
    end

    if not world.MoveActorWithBlock(bullet, shotDirection * BulletSpawnBlockProbeDistance, "Wall") then
        print("[BulletSystem] SpawnBullet Failed: spawn probe blocked by Wall", "Bullet:", bullet.UUID, "Location:", location, "Direction:", shotDirection)
        DebugLog.Write(
            "BulletSystem.Spawn.Fail",
            "Reason=SpawnProbeBlockedByWall",
            "Bullet=" .. DebugLog.Actor(bullet),
            "Location=" .. DebugLog.Vector(location),
            "ShotDirection=" .. DebugLog.Vector(shotDirection),
            "ProbeDelta=" .. DebugLog.Vector(shotDirection * BulletSpawnBlockProbeDistance),
            "OwnerTag=" .. tostring(ownerTag),
            "Instigator=" .. DebugLog.Actor(instigator)
        )
        world.DestroyActor(bullet)
        return nil
    end

    bullets[#bullets + 1] = {
        Actor = bullet,
        Mesh = mesh,
        Direction = shotDirection,
        OwnerTag = ownerTag or "Bullet",
        Instigator = instigator,
        LifeTime = BulletLifeTime
    }

    print("[BulletSystem] SpawnBullet Success", "Bullet:", bullet.UUID, "OwnerTag:", ownerTag or "Bullet")
    DebugLog.Write(
        "BulletSystem.Spawn.Success",
        "Bullet=" .. DebugLog.Actor(bullet),
        "OwnerTag=" .. tostring(ownerTag or "Bullet"),
        "FinalLocation=" .. DebugLog.Vector(bullet.Location),
        "Direction=" .. DebugLog.Vector(shotDirection),
        "BulletCountAfter=" .. tostring(#bullets)
    )
    return bullet
end

local function destroyBullet(index)
    local world = getWorld()
    local bulletInfo = bullets[index]
    if bulletInfo ~= nil and bulletInfo.Actor ~= nil and bulletInfo.Actor:IsValid() then
        DebugLog.Write(
            "BulletSystem.DestroyBullet",
            "Index=" .. tostring(index),
            "Bullet=" .. DebugLog.Actor(bulletInfo.Actor),
            "OwnerTag=" .. tostring(bulletInfo.OwnerTag),
            "Location=" .. DebugLog.Vector(bulletInfo.Actor.Location),
            "RemainingLife=" .. tostring(bulletInfo.LifeTime)
        )
        world.DestroyActor(bulletInfo.Actor)
    end

    table.remove(bullets, index)
end

function BulletSystem.Tick(dt)
    local world = getWorld()

    for i = #bullets, 1, -1 do
        local bulletInfo = bullets[i]
        local bullet = bulletInfo.Actor

        if bullet == nil or not bullet:IsValid() then
            print("[BulletSystem] Tick removed invalid bullet", "Index:", i, "OwnerTag:", bulletInfo.OwnerTag or "nil")
            DebugLog.Write("BulletSystem.Tick.Remove", "Reason=InvalidBullet", "Index=" .. tostring(i), "OwnerTag=" .. tostring(bulletInfo.OwnerTag))
            table.remove(bullets, i)
        elseif bullet:HasTag("DamageApplied") then
            print("[BulletSystem] Tick destroy bullet: damage applied", "Bullet:", bullet.UUID, "OwnerTag:", bulletInfo.OwnerTag or "nil")
            DebugLog.Write("BulletSystem.Tick.Remove", "Reason=DamageApplied", "Bullet=" .. DebugLog.Actor(bullet), "OwnerTag=" .. tostring(bulletInfo.OwnerTag), "Location=" .. DebugLog.Vector(bullet.Location))
            destroyBullet(i)
        else
            bulletInfo.LifeTime = bulletInfo.LifeTime - dt

            if bulletInfo.LifeTime <= 0.0 then
                print("[BulletSystem] Tick destroy bullet: lifetime expired", "Bullet:", bullet.UUID, "OwnerTag:", bulletInfo.OwnerTag or "nil")
                DebugLog.Write("BulletSystem.Tick.Remove", "Reason=LifeTimeExpired", "Bullet=" .. DebugLog.Actor(bullet), "OwnerTag=" .. tostring(bulletInfo.OwnerTag), "Location=" .. DebugLog.Vector(bullet.Location))
                destroyBullet(i)
            else
                local moveDelta = bulletInfo.Direction * BulletSpeed * dt
                if not world.MoveActorWithBlock(bullet, moveDelta, "Wall") then
                    print("[BulletSystem] Tick destroy bullet: blocked by Wall", "Bullet:", bullet.UUID, "OwnerTag:", bulletInfo.OwnerTag or "nil", "Location:", bullet.Location, "MoveDelta:", moveDelta)
                    DebugLog.Write(
                        "BulletSystem.Tick.Remove",
                        "Reason=MoveBlockedByWall",
                        "Bullet=" .. DebugLog.Actor(bullet),
                        "OwnerTag=" .. tostring(bulletInfo.OwnerTag),
                        "Location=" .. DebugLog.Vector(bullet.Location),
                        "Direction=" .. DebugLog.Vector(bulletInfo.Direction),
                        "MoveDelta=" .. DebugLog.Vector(moveDelta),
                        "DeltaTime=" .. tostring(dt)
                    )
                    destroyBullet(i)
                end
            end
        end
    end
end

_G.BulletSystem = BulletSystem

return BulletSystem
