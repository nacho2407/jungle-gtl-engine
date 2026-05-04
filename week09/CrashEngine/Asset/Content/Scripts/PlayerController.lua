local PlayerMovement = dofile("Asset/Content/Scripts/PlayerMovement.lua")

local moveSpeed = 10.0
local pressedKeys = {}
local movementState = nil

local HP = 100.0
local HP_reduction  = 10;

local LightComponet = nil
local MeshComponent = nil
local Initial_Light_Intensity = 0.0

local DocumentCount = 0
local GameManagerLuaComponent = nil

local function ensurePlayerState()
    _G.PlayerState = _G.PlayerState or {}
    _G.PlayerState.HP = HP
    _G.PlayerState.DocumentCount = DocumentCount
    return _G.PlayerState
end

local function syncPlayerState()
    if _G.PlayerState == nil then
        ensurePlayerState()
        return
    end

    _G.PlayerState.HP = HP
    _G.PlayerState.DocumentCount = DocumentCount
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
    elseif other:HasTag("Bullet") then
        HP = HP - 10
        syncPlayerState()
        print("Player HP : ", HP);
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
        forward = Vector.new(1.0, 0.0, 0.0),
        meshYawOffset = 90.0,
        moveSpeed = moveSpeed,
        velocityInterpSpeed = 12.0,
        turnInterpSpeed = 14.0,
    })
    ensurePlayerState()

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
    if(HP > 0) then
        HP = HP - dt * HP_reduction
    else
        HP = 0
    end

    syncPlayerState()

    if LightComponet ~= nil and LightComponet:IsValid() then
        LightComponet:SetIntensity(HP)
    end

    local moveDelta = PlayerMovement.Update(movementState, pressedKeys, dt)
    obj.Velocity = movementState.velocity

    if MeshComponent ~= nil and MeshComponent:IsValid() then
        MeshComponent:SetRelativeRotation(PlayerMovement.MakeYawRotation(movementState))
    end

    if moveDelta:LengthSquared() > 0.0 then
        World.MoveActorWithBlock(obj, moveDelta, "Wall")
    end


end

function EndPlay()
    obj.Velocity = Vector.new(0.0, 0.0, 0.0)
    if _G.PlayerState ~= nil then
        _G.PlayerState.HP = HP
        _G.PlayerState.DocumentCount = DocumentCount
    end
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
