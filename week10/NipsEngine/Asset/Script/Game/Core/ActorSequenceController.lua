local ActorSequenceController = {}

local CUT_SEQUENCE_BOUND_TAG = "TitleCutSequenceBound"

local function asSequenceComponent(component)
    if component == nil or component.AsActorSequenceComponent == nil then
        return nil
    end
    return component:AsActorSequenceComponent()
end

function ActorSequenceController.GetComponent(actor)
    if actor == nil then
        return nil
    end

    local component = actor:GetComponent("ActorSequenceComponent")
    return asSequenceComponent(component)
end

function ActorSequenceController.EnsureComponent(actor)
    local component = ActorSequenceController.GetComponent(actor)
    if component ~= nil then
        return component
    end

    if actor == nil or actor.AddComponent == nil then
        return nil
    end

    return asSequenceComponent(actor:AddComponent("ActorSequenceComponent", false))
end

function ActorSequenceController.FindActorByName(name)
    if name == nil then
        return nil
    end
    return Engine.API.World.FindActorByName(name)
end

function ActorSequenceController.FindActorByTag(tag)
    if tag == nil then
        return nil
    end
    return Engine.API.World.FindActorByTag(tag)
end

function ActorSequenceController.FindActorsByType(typeName)
    if typeName == nil then
        return {}
    end
    return Engine.API.World.FindActorsByType(typeName) or {}
end

function ActorSequenceController.Play(actor)
    local component = ActorSequenceController.GetComponent(actor)
    if component == nil then
        return nil
    end

    component:Stop()
    local player = component:GetSequencePlayer()
    if player ~= nil then
        player:SetCurrentTime(0.0)
    end
    component:Play()
    return component
end

function ActorSequenceController.PlayCutTrigger(actor)
    if actor == nil then
        return nil
    end

    local sequence = ActorSequenceController.EnsureComponent(actor)
    local triggerComponent = actor:GetComponent("MainSceneDestructibleComponent")
    if sequence == nil or triggerComponent == nil then
        return nil
    end
    if sequence.SetPauseAtEnd ~= nil then
        sequence:SetPauseAtEnd(true)
    end

    if not actor:HasTag(CUT_SEQUENCE_BOUND_TAG) then
        local added = sequence:AddFloatTrack({
            targetComponent = triggerComponent:GetName(),
            targetPropertyPath = "Presentation Trigger",
            curveAssetPath = "Asset/Curve/TitleCutTrigger.curve",
            startTime = 0.0,
            duration = 1.0,
            playRate = 1.0,
            loop = false,
            applyMode = "Absolute",
            timeMappingMode = "NormalizedTime"
        })
        if not added then
            return nil
        end
        actor:AddTag(CUT_SEQUENCE_BOUND_TAG)
    end

    sequence:Stop()
    local player = sequence:GetSequencePlayer()
    if player ~= nil then
        player:SetCurrentTime(0.0)
    end
    sequence:Play()
    return sequence
end

function ActorSequenceController.PlayCutTriggersByType(typeName)
    local result = {}
    for _, actor in ipairs(ActorSequenceController.FindActorsByType(typeName)) do
        local component = ActorSequenceController.PlayCutTrigger(actor)
        if component ~= nil then
            table.insert(result, component)
        end
    end
    return result
end

function ActorSequenceController.PlayByName(name)
    return ActorSequenceController.Play(ActorSequenceController.FindActorByName(name))
end

function ActorSequenceController.PlayByTag(tag)
    return ActorSequenceController.Play(ActorSequenceController.FindActorByTag(tag))
end

function ActorSequenceController.PlayAllByType(typeName)
    local result = {}
    for _, actor in ipairs(ActorSequenceController.FindActorsByType(typeName)) do
        local component = ActorSequenceController.Play(actor)
        if component ~= nil then
            table.insert(result, component)
        end
    end
    return result
end

function ActorSequenceController.IsPlaying(component)
    local player = component and component:GetSequencePlayer()
    return player ~= nil and player:IsPlaying()
end

function ActorSequenceController.AreAllFinished(components)
    if components == nil then
        return true
    end

    for _, component in ipairs(components) do
        if ActorSequenceController.IsPlaying(component) then
            return false
        end
    end
    return true
end

return ActorSequenceController
