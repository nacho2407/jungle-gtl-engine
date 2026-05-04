-- BulletSystem은 Player와 Turret이 함께 쓰는 총알 전역 시스템이다.
-- dofile로 여러 번 불려도 _G.BulletSystem을 재사용해서 bullets 목록이 끊기지 않게 한다.
local BulletSystem = _G.BulletSystem or {}

-- 총알 모델/속도/수명/피격 반경 같은 튜닝 값.
local BulletMeshPath = "Asset/Content/Models/Bullet/Bullet.obj"
local BulletSpeed = 90.0
local BulletLifeTime = 3.0
local BulletScale = Vector.new(0.15, 0.15, 0.15)
local BulletColliderRadius = 0.6
local PlayerHitRadius = 1.2
local TurretHitRadius = 2.0
local BulletMeshRollOffset = 0.0
local BulletMeshYawOffset = 180.0

-- 현재 월드에 살아있는 총알들의 런타임 정보.
local bullets = BulletSystem.Bullets or {}
BulletSystem.Bullets = bullets

-- BindWorld가 되어 있으면 그 World를 사용하고, 아니면 현재 스크립트의 World 전역을 사용한다.
local function getWorld()
    return BulletSystem.World or World
end

-- PlayerController/Turret 등 호출자가 자기 World를 등록한다.
-- 다른 World로 바뀌면 이전 총알 목록은 더 이상 안전하지 않으므로 비운다.
function BulletSystem.BindWorld(world)
    if BulletSystem.World ~= world then
        BulletSystem.Clear(false)
    end

    BulletSystem.World = world
end

-- bullets 테이블을 비운다. destroyActors가 true면 실제 Actor도 월드에서 제거한다.
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

-- nil/zero direction이 들어와도 총알이 터지지 않게 기본 +X 방향으로 보정한다.
local function normalize(vector)
    if vector ~= nil and vector:LengthSquared() > 0.0001 then
        return vector:Normalized()
    end

    return Vector.new(1.0, 0.0, 0.0)
end

-- 이동 방향을 총알 mesh 회전으로 변환한다.
-- Bullet.obj의 기본 축이 엔진 +X와 다르면 offset 값으로 맞춘다.
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

-- 총알 Actor에 StaticMesh와 SphereCollider를 붙인다.
-- 실제 충돌 판정은 아래 거리 체크와 MoveActorWithBlock을 같이 사용한다.
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

-- 외부에서 호출하는 총알 생성 함수.
-- ownerTag로 PlayerBullet/EnemyBullet을 붙여서 피격 대상을 구분한다.
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

-- bullets 배열에서 제거하면서 실제 Actor도 파괴 요청한다.
local function destroyBullet(index)
    local world = getWorld()
    local bulletInfo = bullets[index]
    if bulletInfo ~= nil and bulletInfo.Actor ~= nil and bulletInfo.Actor:IsValid() then
        world.DestroyActor(bulletInfo.Actor)
    end

    table.remove(bullets, index)
end

-- 적 총알이 Player 근처에 왔는지 간단한 거리 기반으로 판정한다.
local function hitPlayer(bulletInfo, player)
    if player == nil or not player:IsValid() then
        return false
    end

    local toPlayer = player.Location - bulletInfo.Actor.Location
    return toPlayer:LengthSquared() <= PlayerHitRadius * PlayerHitRadius
end

-- 데미지 적용은 Actor의 OnTakeDamage 이벤트로 이어진다.
local function applyDamage(target, damage, instigator)
    if target ~= nil and target:IsValid() then
        target:ApplyDamage(damage, instigator)
    end
end

-- 플레이어 총알이 맞출 수 있는 Turret을 찾는다.
-- 현재는 모든 Turret 태그 Actor와 거리 비교하는 방식이다.
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

-- 매 프레임 총알을 이동시키고, 벽/플레이어/터렛 피격을 처리한다.
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

-- 다음 dofile 호출에서도 같은 시스템 테이블을 재사용하게 전역에 저장한다.
_G.BulletSystem = BulletSystem

return BulletSystem
