local UFunctionTest = {}
UFunctionTest.__index = UFunctionTest

local function log(message)
    if Engine and Engine.API and Engine.API.Debug then
        Engine.API.Debug.Log(message)
    elseif Log then
        Log(message)
    end
end

function UFunctionTest.new(scriptComponent, properties)
    local self = setmetatable({}, UFunctionTest)
    self.Component = scriptComponent
    self.Actor = scriptComponent and scriptComponent:GetOwner() or Actor
    return self
end

function UFunctionTest:BeginPlay()
    local actor = self.Actor
    if not actor then
        log("[UFunctionTest] missing actor")
        return
    end

    local p0 = actor:GetActorLocation()
    log(string.format("[UFunctionTest] direct before: %.2f %.2f %.2f", p0.X, p0.Y, p0.Z))

    actor:SetActorLocation(Vector(p0.X + 100.0, p0.Y, p0.Z))

    local p1 = actor:Call("GetActorLocation")
    log(string.format("[UFunctionTest] call after: %.2f %.2f %.2f", p1.X, p1.Y, p1.Z))

    actor:Call("SetActorLocation", Vector(p1.X, p1.Y + 100.0, p1.Z))
end

return UFunctionTest