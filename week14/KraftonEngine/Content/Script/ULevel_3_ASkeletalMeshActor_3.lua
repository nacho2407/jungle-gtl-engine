local mesh = nil
local swordActor = nil
local gunActor = nil
local gunParticle = nil
local beamActive = false

local HAND_BONE = "Bip001 R Hand"
local HIDDEN_LOCATION = Vector.new(0, 0, -10000)
local ENEMY_TAG = "enemy"
local ENEMY_FALLBACK_TAG = "HitTarget"
local BEAM_SOURCE_PARAMETER = "BeamSource"
local BEAM_TARGET_PARAMETER = "BeamEnd"

local EQUIPMENT_BY_MODE = {
    Sword = {
        actor = function() return swordActor end,
        offset = Vector.new(0, -0.05, 0.05),
        rotation_offset = Vector.new(0, 0, 0),
    },
    RayGun = {
        actor = function() return gunActor end,
        offset = Vector.new(0, -0.08, 0),
        rotation_offset = Vector.new(0, 0, 0),
    },
    Ki = {
        actor = function() return nil end,
        offset = Vector.new(0, 0, 0),
        rotation_offset = Vector.new(0, 0, 0),
    },
}

function BeginPlay()
    mesh = obj:GetSkeletalMesh()
    swordActor = World.FindFirstActorByTag("sword")
    gunActor = World.FindFirstActorByTag("gun")
    gunParticle = gunActor ~= nil and gunActor:GetParticleSystem() or nil

    if swordActor ~= nil then
        swordActor:SetVisible(true)
    end
    if gunActor ~= nil then
        gunActor:SetVisible(false)
        gunActor:SetActorLocation(HIDDEN_LOCATION)
    end
    if gunParticle ~= nil then
        gunParticle:SetEmitterSpawningEnabled(false)
    end
    beamActive = false
end

function EndPlay()
    print("[EndPlay] " .. obj.UUID)
end

function OnOverlap(OtherActor)
end

local function set_equipment_visible(actor, visible)
    if actor == nil then
        return
    end

    actor:SetVisible(visible)
    if not visible then
        actor:SetActorLocation(HIDDEN_LOCATION)
    end
end

local function hide_all_equipment()
    set_equipment_visible(swordActor, false)
    set_equipment_visible(gunActor, false)
end

local function attach_equipment(actor, offset, rotation_offset)
    if mesh == nil or actor == nil then
        return
    end

    local pos = mesh:GetBoneSocketLocation(HAND_BONE, offset)
    local rot = mesh:GetBoneSocketRotation(HAND_BONE, Vector.new(0, 0, 0))

    actor:SetActorLocation(pos)
    actor:SetActorRotation(rot + rotation_offset)
end

local function find_nearest_enemy(origin)
    local nearest = nil
    local nearestDistance = nil
    local enemies = World.FindActorsByTag(ENEMY_TAG)

    if enemies == nil or enemies[1] == nil then
        enemies = World.FindActorsByTag(ENEMY_FALLBACK_TAG)
    end

    if enemies == nil then
        return nil
    end

    for _, enemy in ipairs(enemies) do
        if enemy ~= nil and enemy:IsValid() then
            local distance = Vector.Distance(origin, enemy.Location)
            if nearest == nil or distance < nearestDistance then
                nearest = enemy
                nearestDistance = distance
            end
        end
    end

    return nearest
end

local function stop_raygun()
    beamActive = false
    if gunParticle ~= nil then
        gunParticle:SetEmitterSpawningEnabled(false)
    end
end

local function update_raygun_beam()
    if gunActor == nil then
        stop_raygun()
        return
    end

    if gunParticle == nil then
        gunParticle = gunActor:GetParticleSystem()
    end

    if gunParticle == nil then
        stop_raygun()
        return
    end

    local source = gunActor.Location
    local target = find_nearest_enemy(source)
    if target == nil then
        stop_raygun()
        return
    end

    gunParticle:SetVectorParameter(BEAM_SOURCE_PARAMETER, source)
    gunParticle:SetVectorParameter(BEAM_TARGET_PARAMETER, target.Location)
    if not beamActive then
        gunParticle:ResetSystem()
        gunParticle:Activate()
        beamActive = true
    end
    gunParticle:SetEmitterSpawningEnabled(true)
end

function Tick(dt)
    if mesh == nil then
        return
    end

    local mode = _G.YuiCombatMode or "Sword"
    local equipment = EQUIPMENT_BY_MODE[mode] or EQUIPMENT_BY_MODE.Sword
    local activeActor = equipment.actor()

    hide_all_equipment()

    if activeActor ~= nil then
        set_equipment_visible(activeActor, true)
        attach_equipment(activeActor, equipment.offset, equipment.rotation_offset)
    end

    if mode == "RayGun" and _G.YuiRaygunFiring then
        update_raygun_beam()
    else
        stop_raygun()
    end
end
