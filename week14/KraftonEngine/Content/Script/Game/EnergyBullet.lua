local EnergyBullet = {
    ParticlePath = "Content/Particle/EnergyBullet.uasset",
    Lifetime = 0.35,
    SourceParameter = "BeamSource",
    TargetParameter = "BeamEnd",
}

local function is_valid_actor(actor)
    if actor == nil then
        return false
    end
    if type(actor.IsValid) == "function" then
        return actor:IsValid()
    end
    return true
end

function EnergyBullet.Play(source, target)
    if source == nil or target == nil or World == nil or World.SpawnParticleSystem == nil then
        return nil
    end

    local actor = World.SpawnParticleSystem(EnergyBullet.ParticlePath, source)
    if actor == nil then
        return nil
    end

    actor:AddTag("runtime-energy-bullet")

    local particle = actor:GetParticleSystem()
    if particle ~= nil then
        particle:SetVectorParameter(EnergyBullet.SourceParameter, source)
        particle:SetVectorParameter(EnergyBullet.TargetParameter, target)
        particle:ResetSystem()
        particle:SetEmitterSpawningEnabled(true)
        particle:Activate()
    end

    return actor
end

function EnergyBullet.PlayInto(activeList, source, target)
    local actor = EnergyBullet.Play(source, target)
    if actor == nil then
        return nil
    end

    activeList[#activeList + 1] = {
        Actor = actor,
        Remaining = EnergyBullet.Lifetime,
    }
    return actor
end

function EnergyBullet.TickActive(activeList, dt)
    for index = #activeList, 1, -1 do
        local entry = activeList[index]
        if not is_valid_actor(entry.Actor) then
            table.remove(activeList, index)
        else
            entry.Remaining = entry.Remaining - dt
            if entry.Remaining <= 0.0 then
                entry.Actor:Destroy()
                table.remove(activeList, index)
            end
        end
    end
end

function EnergyBullet.DestroyActive(activeList)
    for _, entry in ipairs(activeList) do
        if is_valid_actor(entry.Actor) then
            entry.Actor:Destroy()
        end
    end

    for index = #activeList, 1, -1 do
        activeList[index] = nil
    end
end

return EnergyBullet
