local DebugLog = _G.DebugLog or {}

DebugLog.Path = DebugLog.Path or "Saved/BulletDebugLog.txt"
DebugLog.FallbackPaths = DebugLog.FallbackPaths or {
    "Saved/BulletDebugLog.txt",
    "CrashEngine/Saved/BulletDebugLog.txt",
    "BulletDebugLog.txt"
}
DebugLog.Enabled = false

local function safeToString(value)
    if value == nil then
        return "nil"
    end

    return tostring(value)
end

function DebugLog.Vector(value)
    if value == nil then
        return "nil"
    end

    if value.X ~= nil and value.Y ~= nil and value.Z ~= nil then
        return string.format("(%.3f, %.3f, %.3f)", value.X, value.Y, value.Z)
    end

    return safeToString(value)
end

function DebugLog.Actor(value)
    if value == nil then
        return "nil"
    end

    local valid = false
    if value.IsValid ~= nil then
        valid = value:IsValid()
    end

    return string.format("UUID=%s Valid=%s", safeToString(value.UUID), safeToString(valid))
end

DebugLog.Write = function(category, ...)
    return
end

DebugLog.SessionStart = function(label)
    if DebugLog.Started then
        return
    end

    DebugLog.Started = true
end

_G.DebugLog = DebugLog

return DebugLog
