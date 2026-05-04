-- Turret.lua는 터렛 Actor 하나에 붙는 전용 스크립트다.
-- 감지 범위 체크, 플레이어 바라보기, 발사, 체력/파괴를 담당한다.
local BulletSystem = dofile("Asset/Content/Scripts/BulletSystem.lua")
local DebugLog = dofile("Asset/Content/Scripts/DebugLog.lua")
BulletSystem.BindWorld(World, "Turret")
print("[BindWorld] Turret", obj ~= nil and obj.UUID or "nil")
DebugLog.Write("Turret.BindWorld", "Turret=" .. DebugLog.Actor(obj))

-- 터렛 튜닝 값.
local DetectRange = 30.0
local ShotCooldown = 1.5
local ShotTimer = 0.0
local MaxHP = 3
local HP = MaxHP
local BulletSpawnForwardOffset = 4.0
local BulletSpawnUpOffset = 2.2
local PlayerAimOffset = Vector.new(0.0, 0.0, -1.0)
local DetectHalfAngleCos = math.cos(math.pi * 0.25)

-- Candidate에서 받은 초기 회전의 X/Y는 유지하고, 공격할 때 Z(Yaw)만 Player 방향으로 바꾼다.
local BaseRotation = Vector.new(0.0, 0.0, 0.0)
local BaseForward = Vector.new(1.0, 0.0, 0.0)

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

local function loadTurretSounds()
    SoundManager = GetSoundManager()
    if SoundManager == nil or not SoundManager:IsInitialized() then
        SoundsLoaded = false
        return false
    end

    local enemyShootLoaded = SoundManager:LoadSound("EnemyShoot", "Asset/Content/Sounds/EnemyShoot.mp3", false)
    local enemyDieLoaded = SoundManager:LoadSound("EnemyDie", "Asset/Content/Sounds/EnemyDie.mp3", false)
    SoundsLoaded = enemyShootLoaded and enemyDieLoaded
    return SoundsLoaded
end

local function playTurretSound(sound_id)
    if not SoundsLoaded and not loadTurretSounds() then
        return
    end

    SoundManager:PlaySFX(sound_id)
end

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

-- Lua 런타임에 math.atan2가 없을 수도 있어서 직접 fallback을 둔다.
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

-- World.FindPlayer() 결과가 유효할 때만 Player로 사용한다.
local function findPlayer()
    local player = World.FindPlayer()
    if player ~= nil and player:IsValid() then
        return player
    end

    return nil
end

local function normalizePlanar(vector)
    local planar = Vector.new(vector.X, vector.Y, 0.0)
    if planar:LengthSquared() <= 0.0001 then
        return Vector.new(1.0, 0.0, 0.0)
    end

    return planar:Normalized()
end

local function isInsideSpawnFacingArc(toTarget)
    local targetDirection = normalizePlanar(toTarget)
    local facingDirection = normalizePlanar(BaseForward)
    local dot = facingDirection.X * targetDirection.X + facingDirection.Y * targetDirection.Y
    return dot >= DetectHalfAngleCos
end

-- direction을 수평면 XY로 투영한 뒤 yaw를 계산해서 터렛을 그 방향으로 돌린다.
local function lookAtDirection(direction)
    local planarDirection = Vector.new(direction.X, direction.Y, 0.0)
    if planarDirection:LengthSquared() <= 0.0001 then
        return
    end

    local yaw = math.deg(atan2(planarDirection.Y, planarDirection.X))
    obj.Rotation = Vector.new(BaseRotation.X, BaseRotation.Y, yaw)
end

-- Player 위치를 향해 총알을 생성하고 쿨타임을 다시 채운다.
local function fireAtPlayer(player)
    local targetLocation = player.Location + PlayerAimOffset
    local lookDirection = targetLocation - obj.Location
    if lookDirection:LengthSquared() <= 0.0001 then
        DebugLog.Write("Turret.Fire.Skip", "Reason=ZeroLookDirection", "Turret=" .. DebugLog.Actor(obj), "Player=" .. DebugLog.Actor(player))
        return
    end

    lookAtDirection(lookDirection:Normalized())
    local turretForward = obj:GetForwardVector()
    local turretUp = obj:GetUpVector()
    local spawnLocation = obj.Location + turretForward * BulletSpawnForwardOffset + turretUp * BulletSpawnUpOffset
    local shotDirection = targetLocation - spawnLocation

    if shotDirection:LengthSquared() <= 0.0001 then
        DebugLog.Write("Turret.Fire.Skip", "Reason=ZeroShotDirection", "Turret=" .. DebugLog.Actor(obj), "Player=" .. DebugLog.Actor(player), "SpawnLocation=" .. DebugLog.Vector(spawnLocation))
        return
    end

    local direction = shotDirection:Normalized()
    print("[Turret] Fire EnemyBullet", "Turret:", obj.UUID, "Spawn:", spawnLocation, "Direction:", direction)
    DebugLog.Write(
        "Turret.Fire.Request",
        "Turret=" .. DebugLog.Actor(obj),
        "Player=" .. DebugLog.Actor(player),
        "TurretLocation=" .. DebugLog.Vector(obj.Location),
        "PlayerLocation=" .. DebugLog.Vector(player.Location),
        "TargetLocation=" .. DebugLog.Vector(targetLocation),
        "Forward=" .. DebugLog.Vector(turretForward),
        "Up=" .. DebugLog.Vector(turretUp),
        "SpawnLocation=" .. DebugLog.Vector(spawnLocation),
        "Direction=" .. DebugLog.Vector(direction),
        "ShotTimerBefore=" .. tostring(ShotTimer)
    )

    local bullet = BulletSystem.SpawnBullet(spawnLocation, direction, "EnemyBullet", obj)
    DebugLog.Write(
        "Turret.Fire.Result",
        "Turret=" .. DebugLog.Actor(obj),
        "Bullet=" .. DebugLog.Actor(bullet),
        "SoundWillPlay=true",
        "CooldownWillStart=true"
    )
    playTurretSound("EnemyShoot")
    ShotTimer = ShotCooldown
end

function BeginPlay()
    -- Actor가 스폰된 직후 한 번 호출된다.
    -- GameManager가 Candidate 회전을 복사해둔 뒤라 여기서 그 값을 기준 회전으로 저장할 수 있다.
    HP = MaxHP
    ShotTimer = 0.0
    BaseRotation = obj.Rotation
    BaseForward = normalizePlanar(obj:GetForwardVector())
    loadTurretSounds()
end

function EndPlay()
    print("[Turret] EndPlay", obj ~= nil and obj.UUID or "nil")
    DebugLog.Write("Turret.EndPlay", "Turret=" .. DebugLog.Actor(obj), "HP=" .. tostring(HP), "ShotTimer=" .. tostring(ShotTimer))
    ShotTimer = 0.0
end

function OnOverlapBegin(other)
    if other == nil or not other:IsValid() then
        return
    end

    if other:HasTag("Bullet") then
        print("[Turret] Overlap bullet", "Turret:", obj.UUID, "Bullet:", other.UUID, "PlayerBullet:", other:HasTag("PlayerBullet"), "EnemyBullet:", other:HasTag("EnemyBullet"))
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
    -- BulletSystem.ApplyDamage -> Actor.TakeDamage -> 이 함수 순서로 호출된다.
    HP = HP - damage
    print("Turret HP : ", HP)
    DebugLog.Write("Turret.TakeDamage", "Turret=" .. DebugLog.Actor(obj), "Damage=" .. tostring(damage), "Instigator=" .. DebugLog.Actor(instigator), "HP=" .. tostring(HP))
    spawnEffect(obj.Location, DamagedEffectMaterial, DamagedEffectLifeTime, DamagedEffectRow, DamagedEffectColumn)

    if HP <= 0 then
        print("Turret Destroyed")
        DebugLog.Write("Turret.Destroyed", "Turret=" .. DebugLog.Actor(obj), "Location=" .. DebugLog.Vector(obj.Location))
        playTurretSound("EnemyDie")
        local effectLocation = obj.Location + Vector.new(0.0, 0.0, 5.0)
        spawnEffect(effectLocation, DestroyEffectMaterial, DestroyEffectLifeTime, DestroyEffectRow, DestroyEffectColumn)
        World.DestroyActor(obj)
    end
end

function Tick(dt)
    -- 쿨타임은 매 프레임 dt만큼 줄인다.
    ShotTimer = ShotTimer - dt
    if ShotTimer < 0.0 then
        ShotTimer = 0.0
    end

    local player = findPlayer()
    if player == nil then
        return
    end

    local toPlayer = player.Location + PlayerAimOffset - obj.Location
    if toPlayer:LengthSquared() > DetectRange * DetectRange then
        return
    end

    if not isInsideSpawnFacingArc(toPlayer) then
        return
    end

    -- 사정거리 안에 있으면 발사 쿨타임과 별개로 계속 Player를 바라본다.
    lookAtDirection(toPlayer:Normalized())

    if ShotTimer <= 0.0 then
        fireAtPlayer(player)
    end
end
