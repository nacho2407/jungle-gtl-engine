local targetActor = nil
local gunParticle = nil

local PLAYER_TAG = "player"
local GUN_TAG = "gun"
local BEAM_TARGET_PARAMETER = "BeamEnd"

local FIRE_INTERVAL = 1.0
local BEAM_VISIBLE_TIME = 0.08
local MAX_RANGE = 200.0

local fireTimer = 0.0
local beamTimer = 0.0

local function find_target()
    targetActor = World.FindFirstActorByTag(PLAYER_TAG)
end

local function find_gun_particle()
    gunParticle = obj:GetParticleSystem()
    if gunParticle ~= nil then
        return
    end

    local gunActor = World.FindFirstActorByTag(GUN_TAG)
    if gunActor ~= nil then
        gunParticle = gunActor:GetParticleSystem()
    end
end

local function get_fire_start_location()
    return obj.Location
end

local function fire()
    if gunParticle == nil then
        find_gun_particle()
    end

    if targetActor == nil or not targetActor:IsValid() then
        find_target()
    end

    if gunParticle == nil or targetActor == nil then
        return
    end

    local startPos = get_fire_start_location()
    local targetPos = targetActor.Location
    local fireDir = targetPos - startPos
    fireDir:Normalize()

    local endPos = targetPos
    local wallHit = World.RaycastWorldStatic(startPos, fireDir, MAX_RANGE, obj)
    if wallHit ~= nil then
        endPos = wallHit.WorldHitLocation
    end

    gunParticle:SetVectorParameter(BEAM_TARGET_PARAMETER, endPos)
    gunParticle:ResetSystem()
    gunParticle:Activate()
    beamTimer = BEAM_VISIBLE_TIME
end

function BeginPlay()
    find_target()
    find_gun_particle()

    fireTimer = 0.0
    beamTimer = 0.0

    if gunParticle ~= nil then
        gunParticle:Deactivate()
    end
end

function EndPlay()
    if gunParticle ~= nil then
        gunParticle:Deactivate()
    end
end

function OnOverlap(OtherActor)
end

function OnHit(OtherActor, HitComponent, OtherComp, NormalImpulse, HitResult)
end

function Tick(dt)
    fireTimer = fireTimer - dt
    if fireTimer <= 0.0 then
        fire()
        fireTimer = FIRE_INTERVAL
    end

    if beamTimer > 0.0 then
        beamTimer = beamTimer - dt
        if beamTimer <= 0.0 and gunParticle ~= nil then
            gunParticle:Deactivate()
        end
    end
end
