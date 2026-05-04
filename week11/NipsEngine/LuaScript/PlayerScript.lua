local Script = {}
Script.__index = Script

Script.Properties = {
    bCanTick = {
        Type = "Bool",
        Default = true,
        Category = "Script"
    }
}
function Script:Attack(mode, degree, yaw)
    self.bDoingAttack = true

    if not self.BodySection then
        self.bDoingAttack = false
        return
    end

    local swingSign = math.sin(math.rad(degree)) > 0 and 1 or -1
    local swingStart = 0
    local swingEnd   =  0
    local pitchStart =  0
    local pitchEnd   =  0

    if mode == 1 then
        swingStart = -55 * swingSign
        swingEnd   =  90 * swingSign
        pitchStart =  0
        pitchEnd   =  0
    elseif mode == 2 then
        swingStart = -55 * swingSign
        swingEnd   =  90 * swingSign
        pitchStart =  -45
        pitchEnd   =  -45
    else        
        swingStart = -55 * swingSign
        swingEnd   =  90 * swingSign
        pitchStart =  -90
        pitchEnd   =  -90
    end


    for i = 1, 8 do
        local t = i / 8
        local ease = 1 - (1 - t) * (1 - t)
        self.BodySection.Rotation = Vector(
            pitchStart + (pitchEnd - pitchStart) * ease,
            0,
            swingStart + (swingEnd - swingStart) * ease  -- yaw 없음
        )
        coroutine.yield(WaitForSeconds(0.025))
    end

    -- 복귀
    for i = 1, 5 do
        local t = i / 5
        local ease = t * t
        self.BodySection.Rotation = Vector(
            pitchEnd * (1 - ease),
            0,
            swingEnd * (1 - ease)
        )
        coroutine.yield(WaitForSeconds(0.02))
    end

    self.BodySection.Rotation = Vector(0, 0, 0)
    self.bDoingAttack = false
end

function Script:Dash(dir)
    self.bDoingDash = true
    self.dashDir = dir
    self.dashStepsLeft = 5
    self.dashCooldown = 0.0
end

function Script:DestroyActorAfter(actor, time)
    coroutine.yield(time)
    Engine.API.World.DestroyActor(actor)
end

function Script:FinishAttackAfter(attackId, time)
    coroutine.yield(time)
    if _G.GameJam and _G.GameJam.NotifyPlayerAttackFinished then
        _G.GameJam.NotifyPlayerAttackFinished(attackId)
    end
end


function Script.new(component, properties)
    local self = setmetatable({}, Script)

    self.component = component
    self.owner = component:GetOwner()
    self.time = 0
    self.PrevLocation = Vector(0, 0, 0)
    self.bDoingAttack = false
    self.bDoingDash = false
    self.dashDir = Vector(0, 0, 0)
    self.dashStepsLeft = 0
    self.dashCooldown = 0.0
    self.attackSequence = 0
    self.bTimeSlowActive = false
    self.BodySection = self.owner:GetComponentByName("BodySection"):AsSceneComponent()

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

function Script:BeginPlay()
    Log("[BeginPlay] " .. tostring(self.owner.UUID))

    StartCoroutine(function()
        Log("Coroutine Start")
        coroutine.yield(WaitForSeconds(1.0))
        Log("1 seconds later")
    end)
end

function Script:Tick(dt)
    if not self.bCanTick then return end

    self.time = self.time + dt

    -- Dash 스텝 처리
    if self.bDoingDash then
        self.dashCooldown = self.dashCooldown - dt
        if self.dashCooldown <= 0 then
            self.owner.Location = self.owner.Location + self.dashDir * 1.5
            self.dashStepsLeft = self.dashStepsLeft - 1
            self.dashCooldown = 0.01

            if self.dashStepsLeft <= 0 then
                -- 대쉬 종료 시 PrevLocation 초기화
                self.PrevLocation = self.owner.Location
                self.bDoingDash = false
            end
        end
        return
    end

    local move = Vector(0, 0, 0)

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
    -- Speed 에 따라 바꿀 경우 jittering 현상이 있어 현재 배제
    -- local speed = delta:Size() / math.max(dt, 1e-6)

    local amplitude = 0.0001 + 5 * 0.002
    local freq = 6.0
    local offsetZ = math.cos(self.time * freq) * amplitude
    local offsetX = math.sin(self.time * freq * 0.5) * amplitude * 0.5
    Engine.API.World.AddViewTargetCameraLocation(Vector(offsetX, 0, offsetZ))

    self.owner.Location = CurLoc
    self.PrevLocation = CurLoc

    local rotation = self.owner.Rotation
    rotation.z = rotation.z + Engine.API.Input.GetMouseDelta().X * 0.1
    self.owner.Rotation = rotation

    Engine.API.World.GetViewTargetCamera():add_pitch_input(Engine.API.Input.GetMouseDelta().Y * 0.1)

    local player = Engine.API.World.GetPossessedActor()
    self.camera_pitch = (self.camera_pitch or 0) + Engine.API.Input.GetMouseDelta().Y * 0.1

    if not self.bDoingDash and Engine.API.Input.IsKeyPressed("Shift") then
        local dir = move:Size() > 0.001 and move:Normalized() or self.owner:GetActorForwardVector()
        self:Dash(dir)
    end

    local clampedPitch = 0
    if self.BodySection and not self.bDoingAttack then
        clampedPitch = math.max(-80, math.min(80, self.camera_pitch))
        self.BodySection.Rotation = Vector(0, clampedPitch, 0)
    end

    if (Engine.API.Input.IsMousePressed("RMB")) then
        Engine.API.World.SetTimeScale(0.5)
        if not self.bTimeSlowActive then
            self.bTimeSlowActive = true
            if _G.GameJam and _G.GameJam.NotifyTimeSlowStarted then
                _G.GameJam.NotifyTimeSlowStarted()
            end
        end
    end

    if (Engine.API.Input.IsMouseReleased("RMB")) then
        Engine.API.World.SetTimeScale(1)
        if self.bTimeSlowActive then
            self.bTimeSlowActive = false
            if _G.GameJam and _G.GameJam.NotifyTimeSlowEnded then
                _G.GameJam.NotifyTimeSlowEnded()
            end
        end
    end

    if not self.bDoingAttack and player and Engine.API.Input.IsMousePressed("LMB") then
        local slash = Engine.API.World.SpawnActor("ABladeSlash")
        if slash == nil then
            return
        end

        self.attackSequence = self.attackSequence + 1
        local attackId = tostring(self.owner.UUID) .. "_" .. tostring(self.attackSequence)
        slash:AddTag("PlayerAttack")
        slash:AddTag("AttackId:" .. attackId)
        if _G.GameJam and _G.GameJam.NotifyPlayerAttackStarted then
            _G.GameJam.NotifyPlayerAttackStarted(attackId)
        end

        local yaw = rotation.z
        local yaw_rad = math.rad(yaw)

        local mode = math.random(1, 3)
        local degree
        if mode == 1 then
            degree = 90
        elseif mode == 2 then
            degree = 45
        else
            degree = 135
        end

        local pitch_x = degree * math.sin(yaw_rad)
        local pitch_y = degree * math.cos(yaw_rad)
        local scale_long = 7.5
        local scale_short = 5
        local fx = math.abs(math.sin(yaw_rad))
        local fz = math.abs(math.cos(yaw_rad))
        local scale_x = scale_short + (scale_long - scale_short) * fx
        local scale_z = scale_short + (scale_long - scale_short) * fz

        local pitch_rad = math.rad(self.camera_pitch)
        local fwd = self.owner:GetActorForwardVector()
        local fwd_pitched = Vector(
            fwd.X * math.cos(pitch_rad),
            fwd.Y * math.cos(pitch_rad),
            -math.sin(pitch_rad)
        )

        slash.Rotation = Vector(pitch_y, pitch_x + clampedPitch, yaw)
        slash.Location = self.owner.Location + self.owner.Scale / 2 + fwd_pitched * (scale_long / 2)
        slash.Scale = Vector(scale_x, 2, scale_z)

        StartCoroutine(function()
            self:Attack(mode, degree, yaw)
        end)

        StartCoroutine(function()
            self:DestroyActorAfter(slash, 0.1)
        end)

        StartCoroutine(function()
            self:FinishAttackAfter(attackId, 0.14)
        end)
    end
end

function Script:EndPlay()
    Log("[EndPlay] " .. tostring(self.owner.UUID))
    if self.bTimeSlowActive and _G.GameJam and _G.GameJam.NotifyTimeSlowEnded then
        _G.GameJam.NotifyTimeSlowEnded()
    end
    if self.bTimeSlowActive then
        Engine.API.World.SetTimeScale(1)
        self.bTimeSlowActive = false
    end
end

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
