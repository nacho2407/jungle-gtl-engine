-- PlayerController는 플레이어의 입력, 이동, HP, 아이템 획득, 발사를 담당한다.
-- 이동 계산은 PlayerMovement.lua, 카메라 기준 방향은 CameraConfig.lua, 총알 관리는 BulletSystem.lua에 나눠져 있다.
local PlayerMovement = dofile("Asset/Content/Scripts/PlayerMovement.lua")
local CameraConfig = dofile("Asset/Content/Scripts/CameraConfig.lua")
local BulletSystem = dofile("Asset/Content/Scripts/BulletSystem.lua")
BulletSystem.BindWorld(World)

-- local 변수는 이 파일 안에서만 쓰이는 상태다.
local moveSpeed = 20.0
local pressedKeys = {}
local movementState = nil
local PlayerShotCooldown = 1
local PlayerShotTimer = 0.0
local bGameOverRequested = false

-- Player의 생존/점수 관련 상태.
local HP = 100.0
local bDestroyEffectSpawned = false
local MaxHP = 100.0
local HP_reduction  = 1;

-- Scene에 붙어있는 Component를 캐싱해두면 Tick마다 다시 찾지 않아도 된다.
local LightComponet = nil
local MeshComponent = nil
local Initial_Light_Intensity = 0.0
local Initial_Light_Radius = 10.0
local BatteryLightFlashTime = 0.0
local BatteryLightFlashDuration = 1.0
local CurrentLightR = 1.0
local CurrentLightG = 1.0
local CurrentLightB = 1.0

local DocumentCount = 0
local GameManagerLuaComponent = nil

local EffectLocationOffset = Vector.new(0.0, 0.0, 3.0)
local EffectScale = Vector.new(1.8, 1.8, 1.8)

local DestroyEffectMaterial = "Asset/Content/Materials/subUV_sample.json"
local DestroyEffectLifeTime = 1.2
local DestroyEffectRow = 6
local DestroyEffectColumn = 6
local HealingEffectMaterial = "Asset/Content/Materials/subUV_healing.json"
local HealingEffectLifeTime = 1.0
local HealingEffectRow = 4
local HealingEffectColumn = 5
local DocumentEffectMaterial = "Asset/Content/Materials/subUV_Document.json"
local DocumentEffectLifeTime = 1.0
local DocumentEffectRow = 2
local DocumentEffectColumn = 6
local DamagedEffectMaterial = "Asset/Content/Materials/subUV_Damaged.json"
local DamagedEffectLifeTime = 0.5
local DamagedEffectRow = 1
local DamagedEffectColumn = 1

-- PlayerState는 GameManager 같은 다른 Lua 파일이 읽을 수 있게 _G에 공유한다.
local function ensurePlayerState()
    _G.PlayerState = _G.PlayerState or {}
    _G.PlayerState.HP = HP
    _G.PlayerState.MaxHP = MaxHP
    _G.PlayerState.DocumentCount = DocumentCount
    return _G.PlayerState
end

-- HP나 DocumentCount가 바뀔 때마다 전역 상태와 동기화한다.
local function syncPlayerState()
    if _G.PlayerState == nil then
        ensurePlayerState()
        return
    end

    _G.PlayerState.HP = HP
    _G.PlayerState.MaxHP = MaxHP
    _G.PlayerState.DocumentCount = DocumentCount
end

-- CameraLua가 조준 방향을 만들 때 사용할 플레이어의 현재 시각적 forward/up 방향.
local function syncPlayerAimState()
    _G.PlayerAimState = _G.PlayerAimState or {}

    if movementState ~= nil and movementState.visualForward ~= nil then
        _G.PlayerAimState.Forward = movementState.visualForward
    else
        _G.PlayerAimState.Forward = obj:GetForwardVector()
    end

    _G.PlayerAimState.Up = obj:GetUpVector()
end

-- 데미지는 한 함수로 모아두면 총알, 함정 등 여러 원인에서 같은 규칙을 재사용할 수 있다.
local function takeDamage(damage)
    HP = HP - damage
    if HP < 0.0 then
        HP = 0.0
    end

    syncPlayerState()
    print("Player HP : ", HP);
end

local function clamp01(value)
    if value < 0.0 then
        return 0.0
    end

    if value > 1.0 then
        return 1.0
    end

    return value
end

local function lerp(a, b, t)
    return a + (b - a) * t
end

local function updatePlayerLight(dt)
    if LightComponet == nil or not LightComponet:IsValid() then
        return
    end

    local hpPercent = clamp01(HP / MaxHP)
    LightComponet:SetIntensity(HP)
    LightComponet:SetAttenuationRadius(Initial_Light_Radius * math.sqrt(hpPercent))

    if BatteryLightFlashTime > 0.0 then
        BatteryLightFlashTime = BatteryLightFlashTime - dt
        local t = clamp01(1.0 - BatteryLightFlashTime / BatteryLightFlashDuration)
        CurrentLightR = lerp(0.0, 1.0, t)
        CurrentLightG = 1.0
        CurrentLightB = lerp(0.0, 1.0, t)
    else
        local danger = clamp01((0.1 - hpPercent) / 0.1)
        local targetR = 1.0
        local targetG = lerp(1.0, 0.0, danger)
        local targetB = lerp(1.0, 0.0, danger)
        local colorInterp = clamp01(dt * 3.0)
        CurrentLightR = lerp(CurrentLightR, targetR, colorInterp)
        CurrentLightG = lerp(CurrentLightG, targetG, colorInterp)
        CurrentLightB = lerp(CurrentLightB, targetB, colorInterp)
    end

    LightComponet:SetLightColor(CurrentLightR, CurrentLightG, CurrentLightB, 1.0)
end

local function spawnEffect(location, materialPath, lifeTime, row, column)
    local effectActor = World.SpawnActor("SubUVActor")
    if effectActor == nil or not effectActor:IsValid() then
        print("Failed to spawn SubUVActor effect")
        return
    end

    effectActor.Location = location + EffectLocationOffset
    effectActor.Scale = EffectScale

    local script = effectActor:AddComponent("LuaScriptComponent")
    if script == nil or not script:IsValid() then
        print("Failed to add Effect.lua")
        World.DestroyActor(effectActor)
        return
    end

    if not script:SetScriptPath("Effect.lua") then
        print("Failed to set Effect.lua")
        World.DestroyActor(effectActor)
        return
    end

    if not script:CallFunction("InitEffect", effectActor.Location, lifeTime, materialPath, row, column) then
        print("Failed to initialize effect")
        World.DestroyActor(effectActor)
    end
end

local function spawnDestroyEffectOnce()
    if bDestroyEffectSpawned then
        return
    end

    bDestroyEffectSpawned = true
    spawnEffect(obj.Location, DestroyEffectMaterial, DestroyEffectLifeTime, DestroyEffectRow, DestroyEffectColumn)
    print("Effect!")
end

-- GameManager Actor의 LuaScriptComponent를 직접 호출하고,
-- 실패하면 _G.GameManager 전역 함수로 fallback한다.
local function callGameOver()
    if bGameOverRequested then
        return
    end

    bGameOverRequested = true

    local gameManager = World.FindActorByTag("GameManager")
    if gameManager:IsValid() then
        GameManagerLuaComponent = gameManager:GetComponent("LuaScriptComponent", 0)

        if GameManagerLuaComponent:IsValid() then
            if GameManagerLuaComponent:CallFunction("GameOver", HP, DocumentCount) then
                return
            end
        end
    end

    if _G.GameManager ~= nil and _G.GameManager.GameOver ~= nil then
        _G.GameManager.GameOver(HP, DocumentCount)
    else
        print("GameManager is not ready")
    end
end

-- CameraLua가 _G.CameraState에 올려둔 줌 상태를 읽는다.
local function isZooming()
    return _G.CameraState ~= nil and _G.CameraState.IsZooming == true
end

-- 줌 중이면 카메라 조준 방향을 사용하고, 아니면 Actor forward를 사용한다.
local function getAimDirection()
    if _G.CameraState ~= nil and _G.CameraState.AimDirection ~= nil then
        return _G.CameraState.AimDirection
    end

    return obj:GetForwardVector()
end

-- 총알이 플레이어 중심에서 바로 나오면 모델과 겹치므로 살짝 앞/위에서 생성한다.
local function getBulletSpawnLocation(aimDirection)
    return obj.Location + Vector.new(0.0, 0.0, 1.0) + aimDirection * 0.5
end

-- 좌클릭 발사. 쿨타임 중이면 아무 것도 하지 않는다.
local function firePlayerBullet()
    if PlayerShotTimer > 0.0 then
        return
    end

    local aimDirection = getAimDirection()
    local spawnLocation = getBulletSpawnLocation(aimDirection)
    print("Player Fire Direction : ", aimDirection)
    print("Player Fire Location : ", spawnLocation)
    BulletSystem.SpawnBullet(spawnLocation, aimDirection, "PlayerBullet")
    PlayerShotTimer = PlayerShotCooldown
end

-- 키 이벤트에서 movement 관련 키만 pressedKeys에 저장한다.
local movementKeys = {
    W = true,
    S = true,
    A = true,
    D = true,
    Up = true,
    Down = true,
    Left = true,
    Right = true,
    Q = true,
    E = true,
    Space = true,
    Shift = true,
    Ctrl = true,
}

function OnOverlapEnd(other)
    print("Lua OnOverlapEnd", other.UUID);
end

function OnOverlapBegin(other)
    -- Overlap은 C++ CollisionManager가 감지해서 Lua로 넘겨주는 이벤트다.
    print("Lua OnOverlapBegin", other.UUID);
    if other:HasTag("Document") then
        if other:HasTag("Collected") then
            return
        end

        other:AddTag("Collected")
        DocumentCount = DocumentCount + 1;
        syncPlayerState()
        print("Has Document : ", DocumentCount);
        spawnEffect(other.Location, DocumentEffectMaterial, DocumentEffectLifeTime, DocumentEffectRow, DocumentEffectColumn)
        World.DestroyActor(other)
    elseif other:HasTag("Battery") then
        if other:HasTag("Collected") then
            return
        end

        other:AddTag("Collected")
        HP = HP + 10
        if HP > MaxHP then
            HP = MaxHP
        end
        BatteryLightFlashTime = BatteryLightFlashDuration
        syncPlayerState()
        print("Player HP : ", HP);
        spawnEffect(other.Location, HealingEffectMaterial, HealingEffectLifeTime, HealingEffectRow, HealingEffectColumn)
        World.DestroyActor(other)
    elseif other:HasTag("EnemyBullet") then
        if not other:HasTag("DamageApplied") then
            other:AddTag("DamageApplied")
            obj:ApplyDamage(10, other)
        end

        World.DestroyActor(other)
    elseif other:HasTag("Destination") then
        if DocumentCount>0 then
            print("Game Finish!");
            callGameOver()
        end
    end
end

function BeginPlay()
    -- BeginPlay는 Actor가 월드에서 플레이를 시작할 때 한 번 호출된다.
    pressedKeys = {}
    bGameOverRequested = false
    obj.Velocity = Vector.new(0.0, 0.0, 0.0)
    movementState = PlayerMovement.CreateState({
        forward = CameraConfig.GetMoveForward(),
        right = CameraConfig.GetMoveRight(),
        meshYawOffset = CameraConfig.GetMeshYawOffset(),
        moveSpeed = moveSpeed,
        velocityInterpSpeed = 12.0,
        turnInterpSpeed = 14.0,
    })
    ensurePlayerState()
    syncPlayerAimState()

    local gameManager = World.FindActorByTag("GameManager")
    if gameManager:IsValid() then
        GameManagerLuaComponent = gameManager:GetComponent("LuaScriptComponent", 0)
    end

    LightComponet = obj:GetComponent("PointLightComponent", 0)
    if LightComponet:IsValid() then
        Initial_Light_Radius = LightComponet:GetAttenuationRadius()
        if Initial_Light_Radius <= 0.0 then
            Initial_Light_Radius = 10.0
        end
        LightComponet:SetIntensity(HP)
        updatePlayerLight(0.0)
    end

    MeshComponent = obj:GetComponent("StaticMeshComponent", 0)
    if MeshComponent:IsValid() then
        MeshComponent:SetRelativeRotation(PlayerMovement.MakeYawRotation(movementState))
    end
end

function Tick(dt)
    -- BulletSystem은 전역 시스템처럼 동작하므로 Player 쪽 Tick에서 한 번씩 갱신한다.
    BulletSystem.Tick(dt)

    PlayerShotTimer = PlayerShotTimer - dt
    if PlayerShotTimer < 0.0 then
        PlayerShotTimer = 0.0
    end

    if(HP > 0) then
        HP = HP - dt * HP_reduction
    else
        HP = 0
    end

    if HP <= 0.0 then
        spawnDestroyEffectOnce()
        HP = 0.0
        syncPlayerState()
        updatePlayerLight(dt)
        callGameOver()
        return
    end

    syncPlayerState()
    updatePlayerLight(dt)

    if isZooming() then
        -- 줌 중에는 이동을 막고, 플레이어 mesh를 숨긴 뒤 좌클릭 발사만 허용한다.
        obj.Velocity = Vector.new(0.0, 0.0, 0.0)
        movementState.velocity = Vector.new(0.0, 0.0, 0.0)
        syncPlayerAimState()

        if MeshComponent ~= nil and MeshComponent:IsValid() then
            MeshComponent:SetVisible(false)
            MeshComponent:SetVisibleInGame(false)
        end

        if Input:WasMouseButtonPressed("LeftMouseButton") then
            firePlayerBullet()
        end

        return
    end

    local moveDelta = PlayerMovement.Update(movementState, pressedKeys, dt)
    obj.Velocity = movementState.velocity
    syncPlayerAimState()

    if MeshComponent ~= nil and MeshComponent:IsValid() then
        MeshComponent:SetVisible(true)
        MeshComponent:SetVisibleInGame(true)
        MeshComponent:SetRelativeRotation(PlayerMovement.MakeYawRotation(movementState))
    end

    if moveDelta:LengthSquared() > 0.0 then
        -- 실제 위치 이동은 C++ World API가 Wall Tag Actor와의 충돌을 막아준다.
        World.MoveActorWithBlock(obj, moveDelta, "Wall")
    end


end

function EndPlay()
    -- PIE 종료 시 남아있는 총알/전역 aim 상태를 정리한다.
    BulletSystem.Clear(false)

    obj.Velocity = Vector.new(0.0, 0.0, 0.0)
    if _G.PlayerState ~= nil then
        _G.PlayerState.HP = HP
        _G.PlayerState.DocumentCount = DocumentCount
    end

    if _G.PlayerAimState ~= nil then
        _G.PlayerAimState = nil
    end
end

function OnTakeDamage(damage, instigator)
    -- AActor::TakeDamage가 LuaScriptComponent를 통해 이 함수로 전달된다.
    takeDamage(damage)
    spawnEffect(obj.Location, DamagedEffectMaterial, DamagedEffectLifeTime, DamagedEffectRow, DamagedEffectColumn)
    if HP <= 0.0 then
        spawnDestroyEffectOnce()
    end
end

function OnKeyPressed(key)
    -- Key down 상태는 Tick에서 계속 이동을 계산하기 위해 테이블에 저장한다.
    if movementKeys[key] == nil then
        return
    end

    pressedKeys[key] = true
end

function OnKeyReleased(key)
    -- 손을 뗀 키는 nil로 지워서 다음 Tick부터 이동 입력에서 빠지게 한다.
    if movementKeys[key] == nil then
        return
    end

    pressedKeys[key] = nil
end
