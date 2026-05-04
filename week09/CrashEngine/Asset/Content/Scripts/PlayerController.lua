local PlayerMovement = dofile("Asset/Content/Scripts/PlayerMovement.lua")
local CameraConfig = dofile("Asset/Content/Scripts/CameraConfig.lua")
local BulletSystem = dofile("Asset/Content/Scripts/BulletSystem.lua")
BulletSystem.BindWorld(World)

local moveSpeed = 25.0
local pressedKeys = {}
local movementState = nil
local PlayerShotCooldown = 0.25
local PlayerShotTimer = 0.0

local HP = 100.0
local MaxHP = 100.0
local HP_reduction  = 2.5;

local LightComponet = nil
local MeshComponent = nil
local Initial_Light_Intensity = 0.0

local DocumentCount = 0
local GameManagerLuaComponent = nil

local function ensurePlayerState()
    _G.PlayerState = _G.PlayerState or {}
    _G.PlayerState.HP = HP
    _G.PlayerState.MaxHP = MaxHP
    _G.PlayerState.DocumentCount = DocumentCount
    return _G.PlayerState
end

local function syncPlayerState()
    if _G.PlayerState == nil then
        ensurePlayerState()
        return
    end

    _G.PlayerState.HP = HP
    _G.PlayerState.MaxHP = MaxHP
    _G.PlayerState.DocumentCount = DocumentCount
end

local function syncPlayerAimState()
    _G.PlayerAimState = _G.PlayerAimState or {}

    if movementState ~= nil and movementState.visualForward ~= nil then
        _G.PlayerAimState.Forward = movementState.visualForward
    else
        _G.PlayerAimState.Forward = obj:GetForwardVector()
    end

    _G.PlayerAimState.Up = obj:GetUpVector()
end

local function takeDamage(damage)
    HP = HP - damage
    if HP < 0.0 then
        HP = 0.0
    end

    syncPlayerState()
    print("Player HP : ", HP);
end

local function callGameOver()
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

local function isZooming()
    return _G.CameraState ~= nil and _G.CameraState.IsZooming == true
end

local function getAimDirection()
    if _G.CameraState ~= nil and _G.CameraState.AimDirection ~= nil then
        return _G.CameraState.AimDirection
    end

    return obj:GetForwardVector()
end

local function getBulletSpawnLocation(aimDirection)
    return obj.Location + Vector.new(0.0, 0.0, 1.0) + aimDirection * 0.5
end

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
    print("Lua OnOverlapBegin", other.UUID);
    if other:HasTag("Document") then
        DocumentCount = DocumentCount + 1;
        syncPlayerState()
        print("Has Document : ", DocumentCount);
        World.DestroyActor(other)
    elseif other:HasTag("Battery") then
        HP = HP + 10
        syncPlayerState()
        print("Player HP : ", HP);
        World.DestroyActor(other)
    elseif other:HasTag("EnemyBullet") then
        if not other:HasTag("DamageApplied") then
            other:AddTag("DamageApplied")
            obj:ApplyDamage(10, other)
        end

        World.DestroyActor(other)
    elseif other:HasTag("Destination") then
        print("Game Finish!");
        callGameOver()
    end
end

function BeginPlay()
    pressedKeys = {}
    obj.Velocity = Vector.new(0.0, 0.0, 0.0)
    movementState = PlayerMovement.CreateState({
        forward = CameraConfig.GetMoveForward(),
        right = CameraConfig.GetMoveRight(),
        meshYawOffset = CameraConfig.GetMeshYawOffset(),
        meshBaseRotation = CameraConfig.GetPlayerMeshBaseRotation(),
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
        LightComponet:SetIntensity(HP)
    end

    MeshComponent = obj:GetComponent("StaticMeshComponent", 0)
    if MeshComponent:IsValid() then
        MeshComponent:SetRelativeRotation(PlayerMovement.MakeYawRotation(movementState))
    end
end

function Tick(dt)
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

    syncPlayerState()

    if LightComponet ~= nil and LightComponet:IsValid() then
        LightComponet:SetIntensity(HP)
    end

    if isZooming() then
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
        World.MoveActorWithBlock(obj, moveDelta, "Wall")
    end


end

function EndPlay()
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
    takeDamage(damage)
end

function OnKeyPressed(key)
    if movementKeys[key] == nil then
        return
    end

    pressedKeys[key] = true
end

function OnKeyReleased(key)
    if movementKeys[key] == nil then
        return
    end

    pressedKeys[key] = nil
end
