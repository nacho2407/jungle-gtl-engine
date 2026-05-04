local BulletSystem = _G.BulletSystem or {}

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

local function getWorld()
    return BulletSystem.World or World
end

function BulletSystem.BindWorld(world)
    if BulletSystem.World ~= world then
        BulletSystem.Clear(false)
    end

    BulletSystem.World = world
end

function BulletSystem.Clear(destroyActors)
    if destroyActors then
        local world = getWorld()
        for i = #bullets, 1, -1 do
            local bulletInfo = bullets[i]
            if bulletInfo ~= nil and bulletInfo.Actor ~= nil and bulletInfo.Actor:IsValid() then
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
    local bullet = world.SpawnActor("StaticMeshActor")
    if not bullet:IsValid() then
        return nil
    end

    local shotDirection = normalize(direction)
    bullet:AddTag("Bullet")

    if ownerTag ~= nil then
        bullet:AddTag(ownerTag)
    end

    local mesh = addBulletVisual(bullet)
    bullet.Location = location

    if mesh:IsValid() then
        mesh:SetRelativeRotation(directionToBulletRotation(shotDirection))
    end

    if not world.MoveActorWithBlock(bullet, shotDirection * BulletSpawnBlockProbeDistance, "Wall") then
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

    return bullet
end

local function destroyBullet(index)
    local world = getWorld()
    local bulletInfo = bullets[index]
    if bulletInfo ~= nil and bulletInfo.Actor ~= nil and bulletInfo.Actor:IsValid() then
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
            table.remove(bullets, i)
        elseif bullet:HasTag("DamageApplied") then
            destroyBullet(i)
        else
            bulletInfo.LifeTime = bulletInfo.LifeTime - dt

            if bulletInfo.LifeTime <= 0.0 then
                destroyBullet(i)
            else
                local moveDelta = bulletInfo.Direction * BulletSpeed * dt
                if not world.MoveActorWithBlock(bullet, moveDelta, "Wall") then
                    destroyBullet(i)
                end
            end
        end
    end
end

_G.BulletSystem = BulletSystem

return BulletSystem
