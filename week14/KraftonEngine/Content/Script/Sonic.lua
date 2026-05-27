local hitParticle = nil
local HIT_SPAWN_SECONDS = 2.0
local hitSpawnRemaining = 0.0

local function cache_hit_particle()
    if hitParticle == nil then
        hitParticle = obj:GetParticleSystem()
    end

    return hitParticle
end

local function stop_hit_particle()
    local particle = cache_hit_particle()
    if particle == nil then
        return
    end

    particle:SetEmitterSpawningEnabled(false)
    particle:Deactivate()
end

local function stop_hit_particle_spawning()
    local particle = cache_hit_particle()
    if particle == nil then
        return
    end

    particle:SetEmitterSpawningEnabled(false)
end

local function play_hit_particle()
    local particle = cache_hit_particle()
    if particle == nil then
        return
    end

    particle:ResetSystem()
    particle:Activate()
    particle:SetEmitterSpawningEnabled(true)
end

function BeginPlay()
    print("[BeginPlay] " .. obj.UUID)
    stop_hit_particle()
end

function EndPlay()
    print("[EndPlay] " .. obj.UUID)
    stop_hit_particle()
end

function OnOverlap(OtherActor)
    if hitSpawnRemaining > 0.0 then
        return
    end

    hitSpawnRemaining = HIT_SPAWN_SECONDS
    play_hit_particle()
end

function Tick(dt)
    if hitSpawnRemaining > 0.0 then
        hitSpawnRemaining = hitSpawnRemaining - dt
        if hitSpawnRemaining <= 0.0 then
            hitSpawnRemaining = 0.0
            stop_hit_particle_spawning()
        end
    end
end
