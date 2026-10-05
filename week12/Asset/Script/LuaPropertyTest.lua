local LuaPropertyTest = {}
LuaPropertyTest.__index = LuaPropertyTest

local function log(message)
    if Engine and Engine.API and Engine.API.Debug then
        Engine.API.Debug.Log(message)
    elseif Log then
        Log(message)
    end
end

local function log_warning(message)
    if Engine and Engine.API and Engine.API.Debug and Engine.API.Debug.Warn then
        Engine.API.Debug.Warn(message)
    elseif LogWarning then
        LogWarning(message)
    else
        log(message)
    end
end

local function log_error(message)
    if Engine and Engine.API and Engine.API.Debug and Engine.API.Debug.Error then
        Engine.API.Debug.Error(message)
    elseif LogError then
        LogError(message)
    else
        log(message)
    end
end

local function vec_to_string(v)
    if v == nil then
        return "nil"
    end

    return string.format("(%.3f, %.3f, %.3f)", v.X or 0, v.Y or 0, v.Z or 0)
end

local function try_write(label, fn)
    local ok, err = pcall(fn)
    log(label .. " write ok = " .. tostring(ok))
    if not ok then
        log_warning(label .. " expected failure: " .. tostring(err))
    end
end

function LuaPropertyTest.new(script_component, properties)
    local self = setmetatable({}, LuaPropertyTest)
    self.Component = script_component
    self.Actor = script_component and script_component:GetOwner() or Actor
    self.Properties = properties
    return self
end

function LuaPropertyTest:BeginPlay()
    local actor = self.Actor
    if actor == nil then
        log_error("[LuaPropertyTest] Actor is nil")
        return
    end

    log("[LuaPropertyTest] start")

    log("Actor.Active before = " .. tostring(actor.Active))
    actor.Active = not actor.Active
    log("Actor.Active after = " .. tostring(actor.Active))

    log("Actor.Visible before = " .. tostring(actor.Visible))
    actor.Visible = not actor.Visible
    log("Actor.Visible after = " .. tostring(actor.Visible))

    local scene_component = actor:GetComponent("SceneComponent")
    local scene = scene_component and scene_component:AsSceneComponent() or nil
    if scene ~= nil then
        log("Scene.Location before = " .. vec_to_string(scene.Location))
        scene.Location = Vector(10, 20, 30)
        log("Scene.Location after = " .. vec_to_string(scene.Location))
        log("Scene.GetRelativeLocation = " .. vec_to_string(scene:GetRelativeLocation()))

        log("Scene.Scale before = " .. vec_to_string(scene.Scale))
        scene.Scale = Vector(1.5, 1.5, 1.5)
        log("Scene.Scale after = " .. vec_to_string(scene.Scale))
    else
        log_warning("[LuaPropertyTest] SceneComponent not found")
    end

    local comp = actor:GetComponent("ActorComponent")
    if comp ~= nil then
        log("ActorComponent.EditorOnly readonly = " .. tostring(comp.EditorOnly))
        try_write("ActorComponent.EditorOnly", function()
            comp.EditorOnly = false
        end)
        log("ActorComponent.EditorOnly after write attempt = " .. tostring(comp.EditorOnly))
    end

    log("[LuaPropertyTest] done")
end

return LuaPropertyTest
