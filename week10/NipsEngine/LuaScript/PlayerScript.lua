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
    self.time = 0
    self.PrevLocation =  Vector(0, 0, 0)

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
    
    self.time = self.time + dt

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

    local CurLoc = self.owner.Location

    if move:Size() > 0.001 then
        CurLoc = CurLoc + move:Normalized() * 6.0 * dt
    end

    if not self.PrevLocation then
        self.PrevLocation = CurLoc
    end

    local delta = CurLoc - self.PrevLocation
    local speed = delta:Size() / math.max(dt, 1e-6)

    local amplitude = 0.0001 + speed * 0.0005   -- 크기
    local freq = 6.0 + speed * 0.1             -- 속도

    local offsetZ = math.cos(self.time * freq) * amplitude

    self.owner.Location = CurLoc + Vector(0, 0, offsetZ)

    self.PrevLocation = CurLoc


    -- Mouse
    local sensivity = 0.003

    local rotation = self.owner.Rotation
    rotation.z = rotation.z + Engine.API.Input.GetMouseDelta().X * 0.1
    self.owner.Rotation = rotation

    Engine.API.World.GetViewTargetCamera():add_pitch_input(Engine.API.Input.GetMouseDelta().Y * 0.1)
    
    local player = Engine.API.World.GetPossessedActor()

    self.camera_pitch = (self.camera_pitch or 0) + Engine.API.Input.GetMouseDelta().Y * 0.1

    if player and Engine.API.Input.IsMousePressed("LMB") then
        local slash = Engine.API.World.SpawnActor("ABladeSlash")

        local yaw = rotation.z
        local yaw_rad = math.rad(yaw)
        local degree = math.random(0, 180)
        local pitch_x = degree * (math.sin(yaw_rad))
        local pitch_y = degree * (math.cos(yaw_rad))
        local scale_long = 5
        local scale_short = 1
        local fx = math.abs(math.sin(yaw_rad))
        local fz = math.abs(math.cos(yaw_rad))
        local scale_x = scale_short + (scale_long - scale_short) * fx
        local scale_z = scale_short + (scale_long - scale_short) * fz

        local pitch_rad = math.rad(self.camera_pitch)
        local fwd = self.owner:GetActorForwardVector()
        -- forward를 pitch 각도만큼 위아래로 기울임
        local fwd_pitched = Vector(
            fwd.X * math.cos(pitch_rad),
            fwd.Y * math.cos(pitch_rad),
            -math.sin(pitch_rad)  -- 위를 보면 올라가고 아래를 보면 내려감
        )

        slash.Rotation = Vector(pitch_y, pitch_x, yaw)
        slash.Location = self.owner.Location + self.owner.Scale / 2 + fwd_pitched * (scale_long / 2)
        slash.Scale = Vector(scale_x, 0.1, scale_z)
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