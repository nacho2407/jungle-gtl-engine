local gunActor = nil
local gunParticle = nil

local GUN_TAG = "gun"
local BEAM_TARGET_PARAMETER = "BeamEnd"

local function find_gun_particle()
    gunActor = World.FindFirstActorByTag(GUN_TAG)
    if gunActor == nil then
        gunParticle = nil
        return
    end

    gunParticle = gunActor:GetParticleSystem()
end

local function update_beam_target()
    if gunParticle == nil then
        find_gun_particle()
    end

    if gunParticle == nil then
        return
    end

    gunParticle:SetVectorParameter(BEAM_TARGET_PARAMETER, obj.Location)
end

function BeginPlay()
    find_gun_particle()
end

function EndPlay()
    print("[Enemy EndPlay] " .. obj.Name)
end

function OnOverlap(OtherActor)
    update_beam_target()
end

function OnHit(OtherActor, HitComponent, OtherComp, NormalImpulse, HitResult)
    update_beam_target()
end

function Tick(dt)
end
