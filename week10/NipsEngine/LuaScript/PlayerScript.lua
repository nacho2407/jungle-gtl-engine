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
    Log("[BeginPlay] " .. tostring(self.owner.UUID))

    StartCoroutine(function()
        Log("Coroutine Start")

        coroutine.yield(WaitForSeconds(1.0))

        Log("1 seconds later")
    end
    )
end

function Script:Tick(dt)
    if not self.bCanTick then
        return
    end

    local move = Vector(0,0,0)

    if Engine.API.Input.IsKeyDown("W") then
        move = move + self.owner:GetActorForwardVector()
    end
    if Engine.API.Input.IsKeyDown("S") then
        move = move - self.owner:GetActorForwardVector()
    end
    if Engine.API.Input.IsKeyDown("A") then
        move = move - self.owner:GetActorRightVector()
    end
    if Engine.API.Input.IsKeyDown("D") then
        move = move + self.owner:GetActorRightVector()
    end

    if move:Size() > 0.001 then
        self.owner.Location = self.owner.Location + move:Normalized() * 6.0 * dt
    end

    -- Mouse
    local sensivity = 0.003

    local rotation = self.owner.Rotation
    rotation.z = rotation.z + Engine.API.Input.GetMouseDelta().X * 0.1
    self.owner.Rotation = rotation

    Engine.API.World.GetViewTargetCamera():add_pitch_input(Engine.API.Input.GetMouseDelta().Y * 0.1)
    
    local player = Engine.API.World.GetPossessedActor()

    if player and Engine.API.Input.IsMousePressed("LMB") then

        local slash = Engine.API.World.SpawnActor("ABullet")

    end
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