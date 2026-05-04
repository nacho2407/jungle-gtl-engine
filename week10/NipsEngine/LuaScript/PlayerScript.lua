local Script = {}
Script.__index = Script

Script.Properties = {
    bCanTick = {
        Type = "Bool",
        Default = true,
        Category = "Script"
    },

    MaxHealth = {
        Type = "Float",
        Default = 100.0,
        Category = "Stats"
    },

    Health = {
        Type = "Float",
        Default = 100.0,
        Category = "Stats"
    }
}

local function Clamp(value, minValue, maxValue)
    value = tonumber(value) or minValue
    if value < minValue then
        return minValue
    end
    if value > maxValue then
        return maxValue
    end
    return value
end

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
            swingStart + (swingEnd - swingStart) * ease
        )
        coroutine.yield(WaitForSeconds(0.025))
    end

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

-- =========================================================
--  SlamAttack: 내려찍기 (Q)
--  X축(pitch) 기준: 뒤로 -90° 젖혔다가 → 앞으로 +90° 찍기
--  카메라: 슬램 시작 시 45° 아래 전환, 복귀 시 원래 pitch로 복원
-- =========================================================
function Script:SlamAttack()
    self.bDoingSlam = true

    if not self.BodySection then
        self.bDoingSlam = false
        return
    end

    Engine.API.World.GetViewTargetCamera():add_pitch_input(-self.camera_pitch or 0)
    local originalPitch = 0
    self.slamPitchTarget = 15

    -- 1단계: 뒤로 젖히기 (0 → -90)
    for i = 1, 7 do
        local t    = i / 7
        local ease = t * t
        self.BodySection.Rotation = Vector(-90 * ease, 0, 0)
        coroutine.yield(WaitForSeconds(0.028))
    end

    coroutine.yield(WaitForSeconds(0.07))

    -- 2단계: 앞으로 내리찍기 (-90 → +90)
    for i = 1, 6 do
        local t    = i / 6
        local inv  = 1 - t
        local ease = 1 - inv * inv * inv
        self.BodySection.Rotation = Vector(-90, 0, 90 * ease)
        coroutine.yield(WaitForSeconds(0.013))
    end

    -- 3단계: 충격 — slash 스폰 + 카메라 쉐이크
    local player = Engine.API.World.GetPossessedActor()
    if player then
        if _G.GameJam and _G.GameJam.NotifyPlayerAttackGround then
            _G.GameJam.NotifyPlayerAttackGround({
                source = self.owner
            })
        end

        local slam = Engine.API.World.SpawnActor("ABladeSlash")
        local fwd  = self.owner:GetActorForwardVector()
        slam.Location = self.owner.Location + fwd * 3 + Vector(0, 0, -1)
        slam.Rotation = Vector(90, 0, self.owner.Rotation.z)
        slam.Scale    = Vector(30, 30, 30)
        StartCoroutine(function()
            self:DestroyActorAfter(slam, 0.12)
        end)
    end

    self.bSlamShake         = true
    self.slamShakeTime      = 0.35
    self.slamShakeDuration  = 0.35
    self.slamShakeIntensity = 100.0

    coroutine.yield(WaitForSeconds(0.07))

    -- 4단계: 복귀
    for i = 1, 6 do
        local t    = i / 6
        local ease = t * t
        self.BodySection.Rotation = Vector(-90, 0, 90 * (1 - ease))
        coroutine.yield(WaitForSeconds(0.025))
    end

    for i = 1, 6 do
        local t    = i / 6
        local ease = t * t
        self.BodySection.Rotation = Vector(-90 * (1 - ease), 0, 0)
        coroutine.yield(WaitForSeconds(0.025))
    end

    self.BodySection.Rotation = Vector(0, 0, 0)

    -- 카메라 원래 pitch로 복귀 (Tick이 부드럽게 보간)
    self.slamPitchTarget = originalPitch
    -- slamPitchOffset이 target에 도달하면 자동으로 멈춤
    -- 완전히 복귀될 때까지 대기 후 슬램 종료
    while math.abs(self.slamPitchOffset - self.slamPitchTarget) > 0.5 do
        coroutine.yield(WaitForSeconds(0.016))
    end
    self.slamPitchOffset = 0
    self.slamPitchTarget = 0
    self.camera_pitch = originalPitch

    self.bDoingSlam = false
end

function Script:Dash(dir)
    self.bDoingDash = true
    self.dashDir = dir
    self.dashStepsLeft = 5
    self.dashCooldown = 0.0
    if _G.GameJam and _G.GameJam.NotifyPlayerDashed then
        _G.GameJam.NotifyPlayerDashed({
            direction = dir
        })
    end
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
function Script:HitReact(fromActor)

    if not fromActor then return end
    if self.bHitReact or self.hitInvincibleTime > 0 then return end

    self.bHitReact = true
    self.hitInvincibleTime = 0.4

    local dir = self.owner.Location - fromActor.Location
    dir.z = 0

    if dir:Size() > 0.001 then
        dir = dir:Normalized()
    else
        dir = Vector(0,0,0)
    end

    StartCoroutine(function()

        local duration = 0.25
        local t = 0

        while t < duration do
            local dt = 0.016
            t = t + dt

            local p = t / duration

            -- 카메라 쉐이크
            local intensity = 0.15 * (1 - p)
            Engine.API.World.AddViewTargetCameraLocation(Vector(
                math.sin(t * 80) * intensity,
                0,
                math.cos(t * 90) * intensity
            ))

            -- 넉백
            self.owner.Location = self.owner.Location + dir * (1 - p) * 1.5

            coroutine.yield(WaitForSeconds(dt))
        end

        self.bHitReact = false
    end)
end

function Script:GetHealthSnapshot()
    local maxHealth = math.max(1.0, tonumber(self.MaxHealth) or 100.0)
    local health = Clamp(self.Health, 0.0, maxHealth)
    self.MaxHealth = maxHealth
    self.Health = health

    return {
        health = health,
        maxHealth = maxHealth,
        ratio = health / maxHealth
    }
end

function Script:ResetHealth(maxHealth)
    if maxHealth ~= nil then
        self.MaxHealth = math.max(1.0, tonumber(maxHealth) or self.MaxHealth or 100.0)
    else
        self.MaxHealth = math.max(1.0, tonumber(self.MaxHealth) or 100.0)
    end

    self.Health = self.MaxHealth
    return self:GetHealthSnapshot()
end

function Script:ApplyDamage(amount, source)
    local snapshot = self:GetHealthSnapshot()
    local damage = math.max(0.0, tonumber(amount) or 0.0)
    if damage <= 0.0 or snapshot.health <= 0.0 then
        return 0.0, snapshot
    end

    local oldHealth = snapshot.health
    self.Health = Clamp(oldHealth - damage, 0.0, snapshot.maxHealth)
    snapshot = self:GetHealthSnapshot()

    if self.Health < oldHealth then
        self:HitReact(source)
    end

    return oldHealth - self.Health, snapshot
end

function Script:RecoverHealth(amount, source)
    local snapshot = self:GetHealthSnapshot()
    local recover = math.max(0.0, tonumber(amount) or 0.0)
    if recover <= 0.0 or snapshot.health >= snapshot.maxHealth then
        return 0.0, snapshot
    end

    local oldHealth = snapshot.health
    self.Health = Clamp(oldHealth + recover, 0.0, snapshot.maxHealth)
    snapshot = self:GetHealthSnapshot()
    return self.Health - oldHealth, snapshot
end

function Script:InstallHealthBridge()
    _G.GameJam = _G.GameJam or {}
    _G.GameJam.PlayerHealthOwner = self
    _G.GameJam.GetPlayerHealthSnapshot = function()
        return self:GetHealthSnapshot()
    end
    _G.GameJam.ResetPlayerHealth = function(maxHealth)
        return self:ResetHealth(maxHealth)
    end
    _G.GameJam.ApplyPlayerDamage = function(amount, source)
        return self:ApplyDamage(amount, source)
    end
    _G.GameJam.RecoverPlayerHealth = function(amount, source)
        return self:RecoverHealth(amount, source)
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
    self.bDoingSlam = false         -- Q 슬램 전용 플래그
    self.bSlamShake = false
    self.slamShakeTime = 0
    self.slamShakeDuration = 0
    self.slamShakeIntensity = 0
    self.dashDir = Vector(0, 0, 0)
    self.dashStepsLeft = 0
    self.dashCooldown = 0.0
    self.attackSequence = 0
    self.BodySection = self.owner:GetComponentByName("BodySection"):AsSceneComponent()
    self.slamPitchOffset    = 0      -- 현재 적용 중인 슬램 pitch 오프셋
    self.slamPitchTarget    = 0      -- 목표 오프셋
    self.slamPitchSpeed     = 180    -- 초당 최대 이동 각도 (조절 가능)
    self.bHitReact = false
    self.hitInvincibleTime = 0

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
    self:ResetHealth(self.MaxHealth)
    self:InstallHealthBridge()

    StartCoroutine(function()
        Log("Coroutine Start")
        coroutine.yield(WaitForSeconds(1.0))
        Log("1 seconds later")
    end)
end

function Script:Tick(dt)
    if not self.bCanTick then return end

    local WorldTimeScale = Engine.API.World.GetTimeScale()
    if WorldTimeScale ~= 0 then
        dt = dt / WorldTimeScale
    end
    
    self.time = self.time + dt

    if self.hitInvincibleTime > 0 then
        self.hitInvincibleTime = self.hitInvincibleTime - dt
    end

    if self.bHitReact then
        return
    end

    -- -------------------------------------------------------
    -- Slam 카메라 쉐이크: 슬램 중/후 관계없이 항상 소비
    -- -------------------------------------------------------
    if self.bSlamShake then
        self.slamShakeTime = self.slamShakeTime - dt
        if self.slamShakeTime <= 0 then
            self.bSlamShake = false
        else
            local progress  = self.slamShakeTime / self.slamShakeDuration
            local intensity = self.slamShakeIntensity * progress
            local shakeZ = math.sin(self.time * 85) * intensity * 0.002
            local shakeX = math.cos(self.time * 65) * intensity * 0.001
            Engine.API.World.AddViewTargetCameraLocation(Vector(shakeX, 0, shakeZ))
        end
    end

    -- -------------------------------------------------------
    -- 슬램 카메라 pitch 오프셋 보간 (Tick에서 매 프레임 처리)
    -- -------------------------------------------------------
    if self.slamPitchOffset ~= self.slamPitchTarget then
        local diff = self.slamPitchTarget - self.slamPitchOffset
        local step = self.slamPitchSpeed * dt
        if math.abs(diff) <= step then
            local delta = diff - 0  -- 남은 만큼만
            self.slamPitchOffset = self.slamPitchTarget
            Engine.API.World.GetViewTargetCamera():add_pitch_input(delta)
        else
            local delta = (diff > 0 and step or -step)
            self.slamPitchOffset = self.slamPitchOffset + delta
            Engine.API.World.GetViewTargetCamera():add_pitch_input(delta)
        end
    end

    -- -------------------------------------------------------
    -- Q 슬램 중이면 다른 입력 전부 차단
    -- -------------------------------------------------------
    if self.bDoingSlam then
        return
    end

    -- Dash 스텝 처리
    if self.bDoingDash then
        self.dashCooldown = self.dashCooldown - dt
        if self.dashCooldown <= 0 then
            self.owner.Location = self.owner.Location + self.dashDir * 1.5
            self.dashStepsLeft = self.dashStepsLeft - 1
            self.dashCooldown = 0.01

            if self.dashStepsLeft <= 0 then
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

    -- -------------------------------------------------------
    -- Q: 내려찍기 슬램 (슬램/일반어택 중 아닐 때만)
    -- -------------------------------------------------------
    if not self.bDoingAttack and player and Engine.API.Input.IsKeyPressed("R") then
        StartCoroutine(function()
            self:SlamAttack()
        end)
    end
end

function Script:EndPlay()
    Log("[EndPlay] " .. tostring(self.owner.UUID))
    if _G.GameJam and _G.GameJam.PlayerHealthOwner == self then
        _G.GameJam.PlayerHealthOwner = nil
        _G.GameJam.GetPlayerHealthSnapshot = nil
        _G.GameJam.ResetPlayerHealth = nil
        _G.GameJam.ApplyPlayerDamage = nil
        _G.GameJam.RecoverPlayerHealth = nil
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

    if OtherActor == nil then
        return
    end

    local isDamageSource = (OtherActor.HasTag ~= nil and OtherActor:HasTag("Enemy"))
        or (OtherActor.IsA ~= nil and OtherActor:IsA("ADestructibleActor"))

    if isDamageSource then
        if _G.GameJam and _G.GameJam.DamagePlayer then
            _G.GameJam.DamagePlayer(nil, OtherActor)
        else
            self:ApplyDamage(20.0, OtherActor)
        end
    end
end

function Script:OnEndOverlap(OverlappedComponent, OtherActor, OtherComp, OtherBodyIndex, bFromSweep, SweepResult)
    Log("[OnEndOverlap] " .. tostring(self.owner.UUID))
    if OtherActor ~= nil then
        Log("OtherActor: " .. tostring(OtherActor.Name))
    end
end

return Script
