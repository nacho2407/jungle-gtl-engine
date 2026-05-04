local DebugLog = _G.DebugLog or {}

DebugLog.Path = DebugLog.Path or "Saved/BulletDebugLog.txt"
DebugLog.FallbackPaths = DebugLog.FallbackPaths or {
    "Saved/BulletDebugLog.txt",
    "CrashEngine/Saved/BulletDebugLog.txt",
    "BulletDebugLog.txt"
}
DebugLog.Enabled = true

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

function DebugLog.Write(category, ...)
    if not DebugLog.Enabled then
        return
    end

    local parts = {}
    local now = "time-unavailable"
    if os ~= nil and os.date ~= nil then
        now = os.date("%Y-%m-%d %H:%M:%S")
    end

    parts[#parts + 1] = "[" .. now .. "]"
    parts[#parts + 1] = "[" .. safeToString(category) .. "]"

    local values = { ... }
    for i = 1, #values do
        parts[#parts + 1] = safeToString(values[i])
    end

    local line = table.concat(parts, " ")
    print(line)

    if AppendDebugLog ~= nil then
        if AppendDebugLog(line) then
            return
        end
    end

    if io == nil or io.open == nil then
        return
    end

    local file = io.open(DebugLog.Path, "a")
    if file == nil then
        for i = 1, #DebugLog.FallbackPaths do
            file = io.open(DebugLog.FallbackPaths[i], "a")
            if file ~= nil then
                DebugLog.Path = DebugLog.FallbackPaths[i]
                break
            end
        end
    end

    if file == nil then
        return
    end

    file:write(line)
    file:write("\n")
    file:close()
end

function DebugLog.SessionStart(label)
    if DebugLog.Started then
        return
    end

    DebugLog.Started = true
    DebugLog.Write("Session", "==========", "Start", safeToString(label), "==========")
end

_G.DebugLog = DebugLog

return DebugLog
