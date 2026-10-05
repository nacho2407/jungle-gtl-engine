local AhriAttack2NotifyTest = {}
AhriAttack2NotifyTest.__index = AhriAttack2NotifyTest

local function log(message)
    if Engine and Engine.API and Engine.API.Debug then
        Engine.API.Debug.Log(message)
    elseif Log then
        Log(message)
    end
end

local function numberOrZero(value)
    if type(value) == "number" then
        return value
    end
    return 0.0
end

local function describe(info)
    if type(info) ~= "table" then
        return ""
    end

    return string.format(
        " name=%s time=%.3f duration=%.3f end=%.3f isState=%s",
        tostring(info.NotifyName or info.Name or ""),
        numberOrZero(info.TriggerTime or info.Time),
        numberOrZero(info.Duration),
        numberOrZero(info.EndTime),
        tostring(info.IsState))
end

function AhriAttack2NotifyTest.new(scriptComponent, properties)
    local self = setmetatable({}, AhriAttack2NotifyTest)
    self.Component = scriptComponent
    self.Actor = scriptComponent and scriptComponent:GetOwner() or Actor
    self.Properties = properties or {}
    return self
end

function AhriAttack2NotifyTest:BeginPlay()
    log("[AhriAttack2NotifyTest] ready")
end

function AhriAttack2NotifyTest:OnAnimNotify(notifyName, triggerTime, info)
    log(string.format(
        "[AhriAttack2NotifyTest] OnAnimNotify notify=%s trigger=%.3f%s",
        tostring(notifyName),
        numberOrZero(triggerTime),
        describe(info)))
end

function AhriAttack2NotifyTest:OnAnimNotifyBegin(notifyName, triggerTime, info)
    log(string.format(
        "[AhriAttack2NotifyTest] OnAnimNotifyBegin notify=%s trigger=%.3f%s",
        tostring(notifyName),
        numberOrZero(triggerTime),
        describe(info)))
end

function AhriAttack2NotifyTest:OnAnimNotifyTick(notifyName, deltaTime, info)
    log(string.format(
        "[AhriAttack2NotifyTest] OnAnimNotifyTick notify=%s dt=%.3f%s",
        tostring(notifyName),
        numberOrZero(deltaTime),
        describe(info)))
end

function AhriAttack2NotifyTest:OnAnimNotifyEnd(notifyName, endTime, info)
    log(string.format(
        "[AhriAttack2NotifyTest] OnAnimNotifyEnd notify=%s end=%.3f%s",
        tostring(notifyName),
        numberOrZero(endTime),
        describe(info)))
end

function AhriAttack2NotifyTest:AnimNotify_test1(triggerTime, info)
    log(string.format(
        "[AhriAttack2NotifyTest] AnimNotify_AnimNotify trigger=%.3f%s",
        numberOrZero(triggerTime),
        describe(info)))
end

function AhriAttack2NotifyTest:AnimNotifyBegin_test1(triggerTime, info)
    log(string.format(
        "[AhriAttack2NotifyTest] AnimNotifyBegin_AnimNotify trigger=%.3f%s",
        numberOrZero(triggerTime),
        describe(info)))
end

function AhriAttack2NotifyTest:AnimNotifyTick_test1(deltaTime, info)
    log(string.format(
        "[AhriAttack2NotifyTest] AnimNotifyTick_AnimNotify dt=%.3f%s",
        numberOrZero(deltaTime),
        describe(info)))
end

function AhriAttack2NotifyTest:AnimNotifyEnd_test1(endTime, info)
    log(string.format(
        "[AhriAttack2NotifyTest] AnimNotifyEnd_AnimNotify end=%.3f%s",
        numberOrZero(endTime),
        describe(info)))
end

function AhriAttack2NotifyTest:AnimNotify_AnimNotify_Begin(triggerTime, info)
    log(string.format(
        "[AhriAttack2NotifyTest] AnimNotify_AnimNotify_Begin trigger=%.3f%s",
        numberOrZero(triggerTime),
        describe(info)))
end

function AhriAttack2NotifyTest:AnimNotify_AnimNotify_Tick(deltaTime, info)
    log(string.format(
        "[AhriAttack2NotifyTest] AnimNotify_AnimNotify_Tick dt=%.3f%s",
        numberOrZero(deltaTime),
        describe(info)))
end

function AhriAttack2NotifyTest:AnimNotify_AnimNotify_End(endTime, info)
    log(string.format(
        "[AhriAttack2NotifyTest] AnimNotify_AnimNotify_End end=%.3f%s",
        numberOrZero(endTime),
        describe(info)))
end

function AhriAttack2NotifyTest:AnimNotify_addNotify(triggerTime, info)
    log(string.format(
        "[AhriAttack2NotifyTest] AnimNotify_addNotify trigger=%.3f%s",
        numberOrZero(triggerTime),
        describe(info)))
end

function AhriAttack2NotifyTest:AnimNotifyBegin_addNotify(triggerTime, info)
    log(string.format(
        "[AhriAttack2NotifyTest] AnimNotifyBegin_addNotify trigger=%.3f%s",
        numberOrZero(triggerTime),
        describe(info)))
end

function AhriAttack2NotifyTest:AnimNotifyTick_addNotify(deltaTime, info)
    log(string.format(
        "[AhriAttack2NotifyTest] AnimNotifyTick_addNotify dt=%.3f%s",
        numberOrZero(deltaTime),
        describe(info)))
end

function AhriAttack2NotifyTest:AnimNotifyEnd_addNotify(endTime, info)
    log(string.format(
        "[AhriAttack2NotifyTest] AnimNotifyEnd_addNotify end=%.3f%s",
        numberOrZero(endTime),
        describe(info)))
end

function AhriAttack2NotifyTest:AnimNotify_addNotify_Begin(triggerTime, info)
    log(string.format(
        "[AhriAttack2NotifyTest] AnimNotify_addNotify_Begin trigger=%.3f%s",
        numberOrZero(triggerTime),
        describe(info)))
end

function AhriAttack2NotifyTest:AnimNotify_addNotify_Tick(deltaTime, info)
    log(string.format(
        "[AhriAttack2NotifyTest] AnimNotify_addNotify_Tick dt=%.3f%s",
        numberOrZero(deltaTime),
        describe(info)))
end

function AhriAttack2NotifyTest:AnimNotify_addNotify_End(endTime, info)
    log(string.format(
        "[AhriAttack2NotifyTest] AnimNotify_addNotify_End end=%.3f%s",
        numberOrZero(endTime),
        describe(info)))
end

function AhriAttack2NotifyTest:AnimNotify_hihi(triggerTime, info)
    log(string.format(
        "[AhriAttack2NotifyTest] AnimNotify_hihi trigger=%.3f%s",
        numberOrZero(triggerTime),
        describe(info)))
end

function AhriAttack2NotifyTest:AnimNotifyBegin_hihi(triggerTime, info)
    log(string.format(
        "[AhriAttack2NotifyTest] AnimNotifyBegin_hihi trigger=%.3f%s",
        numberOrZero(triggerTime),
        describe(info)))
end

function AhriAttack2NotifyTest:AnimNotifyTick_hihi(deltaTime, info)
    log(string.format(
        "[AhriAttack2NotifyTest] AnimNotifyTick_hihi dt=%.3f%s",
        numberOrZero(deltaTime),
        describe(info)))
end

function AhriAttack2NotifyTest:AnimNotifyEnd_hihi(endTime, info)
    log(string.format(
        "[AhriAttack2NotifyTest] AnimNotifyEnd_hihi end=%.3f%s",
        numberOrZero(endTime),
        describe(info)))
end

function AhriAttack2NotifyTest:AnimNotify_hihi_Begin(triggerTime, info)
    log(string.format(
        "[AhriAttack2NotifyTest] AnimNotify_hihi_Begin trigger=%.3f%s",
        numberOrZero(triggerTime),
        describe(info)))
end

function AhriAttack2NotifyTest:AnimNotify_hihi_Tick(deltaTime, info)
    log(string.format(
        "[AhriAttack2NotifyTest] AnimNotify_hihi_Tick dt=%.3f%s",
        numberOrZero(deltaTime),
        describe(info)))
end

function AhriAttack2NotifyTest:AnimNotify_hihi_End(endTime, info)
    log(string.format(
        "[AhriAttack2NotifyTest] AnimNotify_hihi_End end=%.3f%s",
        numberOrZero(endTime),
        describe(info)))
end

return AhriAttack2NotifyTest
