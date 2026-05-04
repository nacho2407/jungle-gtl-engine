local BulletSystem = _G.BulletSystem or {}

local BulletMeshPath = "Asset/Content/Models/Bullet/Bullet.obj"
local BulletSpeed = 45.0
local BulletLifeTime = 3.0
local BulletScale = Vector.new(0.15, 0.15, 0.15)
local BulletColliderRadius = 0.6
local PlayerHitRadius = 1.2
local TurretHitRadius = 2.0
local BulletMeshRollOffset = 180.0
local BulletMeshYawOffset = 180.0

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

function BulletSystem.SpawnBullet(location, direction, ownerTag)
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

    bullets[#bullets + 1] = {
        Actor = bullet,
        Mesh = mesh,
        Direction = shotDirection,
        OwnerTag = ownerTag or "Bullet",
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

local function hitPlayer(bulletInfo, player)
    if player == nil or not player:IsValid() then
        return false
    end

    local toPlayer = player.Location - bulletInfo.Actor.Location
    return toPlayer:LengthSquared() <= PlayerHitRadius * PlayerHitRadius
end

local function applyDamage(target, damage, instigator)
    if target ~= nil and target:IsValid() then
        target:ApplyDamage(damage, instigator)
    end
end

local function hitTurret(bulletInfo)
    local world = getWorld()
    if world.FindActorsByTag == nil then
        return nil
    end

    local turrets = world.FindActorsByTag("Turret")
    for i, turret in ipairs(turrets) do
        if turret:IsValid() then
            local toTurret = turret.Location - bulletInfo.Actor.Location
            if toTurret:LengthSquared() <= TurretHitRadius * TurretHitRadius then
                return turret
            end
        end
    end

    return nil
end

function BulletSystem.Tick(dt)
    local world = getWorld()
    local player = world.FindPlayer()

    for i = #bullets, 1, -1 do
        local bulletInfo = bullets[i]
        local bullet = bulletInfo.Actor

        if bullet == nil or not bullet:IsValid() then
            table.remove(bullets, i)
        else
            bulletInfo.LifeTime = bulletInfo.LifeTime - dt

            if bulletInfo.LifeTime <= 0.0 then
                destroyBullet(i)
            else
                local moveDelta = bulletInfo.Direction * BulletSpeed * dt
                local moved = world.MoveActorWithBlock(bullet, moveDelta, "Wall")

                if not moved then
                    destroyBullet(i)
                elseif bulletInfo.OwnerTag == "EnemyBullet" and hitPlayer(bulletInfo, player) then
                    if not bullet:HasTag("DamageApplied") then
                        bullet:AddTag("DamageApplied")
                        applyDamage(player, 10, bullet)
                    end

                    destroyBullet(i)
                elseif bulletInfo.OwnerTag == "PlayerBullet" then
                    local turret = hitTurret(bulletInfo)
                    if turret ~= nil and turret:IsValid() then
                        applyDamage(turret, 1, bullet)
                        destroyBullet(i)
                    end
                end
            end
        end
    end
end

_G.BulletSystem = BulletSystem

return BulletSystem
