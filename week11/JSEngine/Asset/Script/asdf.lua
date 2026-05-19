local Script = {}
Script.__index = Script

-- Editor에 노출될 기본 Property 목록
--Type은 Int, Float, Bool, String, Vector이 있고 Default는 String
Script.Properties = {
    bCanTick = {
        Type = "Bool",
        Default = true,
        Category = "Script"
    }
}

-- instance를 반환하는 함수 Script 객체화하는 함수
function Script.new(component, properties)
    local self = setmetatable({}, Script)

    self.component = component
    self.owner = component:GetOwner()

    -- Editor에서 설정한 Property 값을 self에 복사
    properties = properties or {}

    for key, desc in pairs(Script.Properties) do
        if properties[key] ~= nil then
            self[key] = properties[key]
        else
            self[key] = desc.Default
        end
    end

    return self
end


--LifeCycle
function Script:BeginPlay()
    local comp = self.owner:GetComponentByType("SubUVComponent")
    local subuv = comp and comp:AsSubUVComponent()

    if subuv == nil then
        Log("[LuaTest] SubUVComponent not found or cast failed")
        return
    end

    Log("[LuaTest] subuv.Play type = " .. tostring(type(subuv.Play)))
    Log("[LuaTest] subuv.SetFrameRate type = " .. tostring(type(subuv.SetFrameRate)))
    Log("[LuaTest] subuv.SetLoop type = " .. tostring(type(subuv.SetLoop)))

    local okRate, errRate = pcall(function()
        subuv:SetFrameRate(12.5)
    end)
    Log("[LuaTest] SetFrameRate(12.5) ok = " .. tostring(okRate))
    if not okRate then
        Log("[LuaTest] SetFrameRate error = " .. tostring(errRate))
    end

    local okLoopFalse, errLoopFalse = pcall(function()
        subuv:SetLoop(false)
    end)
    Log("[LuaTest] SetLoop(false) ok = " .. tostring(okLoopFalse))
    if not okLoopFalse then
        Log("[LuaTest] SetLoop(false) error = " .. tostring(errLoopFalse))
    end

    subuv.FrameIndex = 7
    Log("[LuaTest] before Play FrameIndex = " .. tostring(subuv.FrameIndex))

    local okPlay, errPlay = pcall(function()
        subuv:Play()
    end)
    Log("[LuaTest] Play() ok = " .. tostring(okPlay))
    if not okPlay then
        Log("[LuaTest] Play error = " .. tostring(errPlay))
    end

    Log("[LuaTest] after Play FrameIndex = " .. tostring(subuv.FrameIndex))

    local okLoopTrue, errLoopTrue = pcall(function()
        subuv:SetLoop(true)
    end)
    Log("[LuaTest] SetLoop(true) ok = " .. tostring(okLoopTrue))
    if not okLoopTrue then
        Log("[LuaTest] SetLoop(true) error = " .. tostring(errLoopTrue))
    end

    Log("[LuaTest] IsLoop type = " .. tostring(type(subuv.IsLoop)))
Log("[LuaTest] IsFinished type = " .. tostring(type(subuv.IsFinished)))

local okIsLoop, resultIsLoop = pcall(function()
    return subuv:IsLoop()
end)
Log("[LuaTest] IsLoop() ok = " .. tostring(okIsLoop) .. ", result = " .. tostring(resultIsLoop))

local okFinished, resultFinished = pcall(function()
    return subuv:IsFinished()
end)
Log("[LuaTest] IsFinished() ok = " .. tostring(okFinished) .. ", result = " .. tostring(resultFinished))
end

function Script:Tick(dt)
    if not self.bCanTick then
        return
    end

        local comp = self.owner:GetComponentByType("SubUVComponent")
local subuv = comp and comp:AsSubUVComponent()

if subuv == nil then
    Log("[LuaTest] cast failed")
    return
end

Log("[LuaTest] subuv.Play type = " .. tostring(type(subuv.Play)))

subuv.FrameIndex = 5
subuv:Play()

Log("[LuaTest] after Play FrameIndex = " .. tostring(subuv.FrameIndex))
end

function Script:EndPlay()
    Log("[EndPlay] " .. tostring(self.owner.UUID))
end

--Delegate
function Script:OnHit(HitComponent, OtherActor, OtherComp, NormalImpulse, Hit)
    Log("[OnHit] " .. tostring(self.owner.UUID))

    if OtherActor ~= nil then
        Log("OtherActor: " .. tostring(OtherActor.Name))
    end
end

function Script:OnBeginOverlap(OverlappedComponent, OtherActor, OtherComp, OtherBodyIndex, bFromSweep, SweepResult)
    Log("[OnBeginOverlap] " .. tostring(self.owner.UUID))

    if OtherActor ~= nil then
        Log("OtherActor: " .. tostring(OtherActor.Name))
    end

    if bFromSweep then
        Log("BeginOverlap from sweep")
    end
end

function Script:OnEndOverlap(OverlappedComponent, OtherActor, OtherComp, OtherBodyIndex, bFromSweep, SweepResult)
    Log("[OnEndOverlap] " .. tostring(self.owner.UUID))

    if OtherActor ~= nil then
        Log("OtherActor: " .. tostring(OtherActor.Name))
    end
end

return Script