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
local MaxHP = 100.0
local HP_reduction  = 2.5;

-- Scene에 붙어있는 Component를 캐싱해두면 Tick마다 다시 찾지 않아도 된다.
local LightComponet = nil
local MeshComponent = nil
local Initial_Light_Intensity = 0.0

local DocumentCount = 0
local GameManagerLuaComponent = nil
local SoundManager = nil

local function loadPlayerSounds()
    SoundManager = GetSoundManager()
    SoundManager:LoadSound("Shoot", "Asset/Content/Sounds/Shoot.mp3", false)
    SoundManager:LoadSound("Charge", "Asset/Content/Sounds/Charge.mp3", false)
    SoundManager:LoadSound("Document", "Asset/Content/Sounds/Document.mp3", false)
end

local function playPlayerSound(sound_id)
    if SoundManager == nil then
        loadPlayerSounds()
    end

    SoundManager:PlaySFX(sound_id)
end

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
    playPlayerSound("Shoot")
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
        DocumentCount = DocumentCount + 1;
        syncPlayerState()
        playPlayerSound("Document")
        print("Has Document : ", DocumentCount);
        World.DestroyActor(other)
    elseif other:HasTag("Battery") then
        HP = HP + 10
        syncPlayerState()
        playPlayerSound("Charge")
        print("Player HP : ", HP);
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
    loadPlayerSounds()

    local gameManager = World.FindActorByTag("GameManager")
    if gameManager:IsValid() then
        GameManagerLuaComponent = gameManager:GetComponent("LuaScriptComponent", 0)
    end

    LightComponet = obj:GetComponent("PointLightComponent", 0)
    if LightComponet:IsValid() then
        LightComponet:SetIntensity(HP)
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
        HP = 0.0
        syncPlayerState()
        callGameOver()
        return
    end

    syncPlayerState()

    if LightComponet ~= nil and LightComponet:IsValid() then
        LightComponet:SetIntensity(HP)
    end

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
