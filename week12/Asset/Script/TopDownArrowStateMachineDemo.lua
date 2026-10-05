local TopDownArrowDemo = {}
TopDownArrowDemo.__index = TopDownArrowDemo

TopDownArrowDemo.Properties = {
    MoveSpeed = {
        Type = "Float",
        Default = 3.0,
        Min = 0.0,
        Category = "TopDown Demo"
    },

    -- Z-up 기준 카메라 높이
    CameraHeight = {
        Type = "Float",
        Default = 6.0,
        Min = 0.1,
        Category = "TopDown Demo"
    },

    -- X-forward 기준: 캐릭터 뒤쪽, 즉 -X 방향으로 카메라를 뺌
    CameraBackDistance = {
        Type = "Float",
        Default = 3.0,
        Min = 0.0,
        Category = "TopDown Demo"
    },

    -- Y축 옆 방향 오프셋. 오른쪽/왼쪽에서 보고 싶으면 조정
    CameraSideDistance = {
        Type = "Float",
        Default = 0.0,
        Category = "TopDown Demo"
    },

    CameraLookAtHeight = {
        Type = "Float",
        Default = 1.0,
        Min = 0.0,
        Category = "TopDown Demo"
    },

    MoveParamName = {
        Type = "String",
        Default = "IsMoving",
        Category = "TopDown Demo"
    },

    bUpdateAnimGraphMoveParam = {
        Type = "Bool",
        Default = true,
        Category = "TopDown Demo"
    },

    RotationInterpSpeed = {
        Type = "Float",
        Default = 12.0,
        Min = 0.0,
        Category = "TopDown Demo"
    },

    ActorYawOffset = {
        Type = "Float",
        Default = 0.0,
        Category = "TopDown Demo"
    },

    bDebugTransform = {
        Type = "Bool",
        Default = true,
        Category = "Debug"
    },

    DebugLogInterval = {
        Type = "Float",
        Default = 0.5,
        Min = 0.1,
        Category = "Debug"
    },
}

local function log(message)
    if Engine and Engine.API and Engine.API.Debug and Engine.API.Debug.Log then
        Engine.API.Debug.Log(message)
    elseif Log then
        Log(message)
    end
end

local function vec_to_string(v)
    if not v then
        return "nil"
    end

    return string.format(
        "(X=%.3f, Y=%.3f, Z=%.3f)",
        v.X or 0.0,
        v.Y or 0.0,
        v.Z or 0.0
    )
end

local function get_delta_time(fallback)
    if Engine and Engine.API and Engine.API.World and Engine.API.World.GetDeltaTime then
        local ok, dt = pcall(function()
            return Engine.API.World.GetDeltaTime()
        end)

        if ok and dt and dt > 0.0 then
            return dt
        end
    end

    if fallback and fallback > 0.0 then
        return fallback
    end

    return 0.016
end

local function is_key_down(key)
    local input = Engine and Engine.API and Engine.API.Input
    if not input then
        return false
    end

    if input.IsRawKeyDown then
        local ok, value = pcall(function()
            return input.IsRawKeyDown(key)
        end)

        if ok then
            return value == true
        end
    end

    if input.IsKeyDown then
        local ok, value = pcall(function()
            return input.IsKeyDown(key)
        end)

        if ok then
            return value == true
        end
    end

    return false
end

local function atan2_safe(y, x)
    if math.atan2 then
        return math.atan2(y, x)
    end

    if x > 0.0 then
        return math.atan(y / x)
    end

    if x < 0.0 and y >= 0.0 then
        return math.atan(y / x) + math.pi
    end

    if x < 0.0 and y < 0.0 then
        return math.atan(y / x) - math.pi
    end

    if x == 0.0 and y > 0.0 then
        return math.pi * 0.5
    end

    if x == 0.0 and y < 0.0 then
        return -math.pi * 0.5
    end

    return 0.0
end

local function clamp(value, minValue, maxValue)
    if value < minValue then
        return minValue
    end

    if value > maxValue then
        return maxValue
    end

    return value
end

local function normalize_angle_degrees(angle)
    angle = angle % 360.0

    if angle > 180.0 then
        angle = angle - 360.0
    end

    if angle < -180.0 then
        angle = angle + 360.0
    end

    return angle
end

local function interp_angle_degrees(current, target, dt, speed)
    if not current then
        return target
    end

    if not speed or speed <= 0.0 then
        return target
    end

    local alpha = clamp(dt * speed, 0.0, 1.0)
    local delta = normalize_angle_degrees(target - current)

    return current + delta * alpha
end

local function make_move_direction()
    local x = 0.0
    local y = 0.0

    -- 현재 상황 기준:
    -- 기존 Up = +X 가 화면 오른쪽으로 보임.
    -- 그러면 화면 오른쪽은 +X로 둬야 하고,
    -- 화면 위쪽은 +Y로 매핑하는 게 맞음.
    --
    -- Up    = +Y
    -- Down  = -Y
    -- Right = +X
    -- Left  = -X

    if is_key_down("Up") or is_key_down("UpArrow") then
        y = y - 1.0
    end

    if is_key_down("Down") or is_key_down("DownArrow") then
        y = y + 1.0
    end

    if is_key_down("Right") or is_key_down("RightArrow") then
        x = x + 1.0
    end

    if is_key_down("Left") or is_key_down("LeftArrow") then
        x = x - 1.0
    end

    local dir = Vector(x, y, 0.0)

    if dir:SizeSquared2D() > 0.0001 then
        return dir:GetSafeNormal2D()
    end

    return Vector(0.0, 0.0, 0.0)
end

local function get_player_controller()
    local world = Engine and Engine.API and Engine.API.World
    if not world then
        return nil
    end

    if world.GetPlayerController then
        local ok, controller = pcall(function()
            return world.GetPlayerController()
        end)

        if ok then
            return controller
        end
    end

    return nil
end

local function get_possessed_pawn()
    local world = Engine and Engine.API and Engine.API.World

    if world and world.GetPossessedActor then
        local ok, pawn = pcall(function()
            return world.GetPossessedActor()
        end)

        if ok and pawn then
            return pawn
        end
    end

    local controller = get_player_controller()
    if not controller then
        return nil
    end

    if controller.GetPossessedActor then
        local ok, pawn = pcall(function()
            return controller:GetPossessedActor()
        end)

        if ok and pawn then
            return pawn
        end
    end

    if controller.GetPawn then
        local ok, pawn = pcall(function()
            return controller:GetPawn()
        end)

        if ok and pawn then
            return pawn
        end
    end

    return nil
end

function TopDownArrowDemo.new(scriptComponent, properties)
    local self = setmetatable({}, TopDownArrowDemo)

    self.Component = scriptComponent
    self.Actor = nil

    if scriptComponent and scriptComponent.GetOwner then
        local ok, owner = pcall(function()
            return scriptComponent:GetOwner()
        end)

        if ok then
            self.Actor = owner
        end
    end

    if not self.Actor then
        self.Actor = Actor or Owner
    end

    self.Properties = properties or {}

    self.Mesh = nil
    self.bInitialized = false
    self.bLoggedNoPawn = false
    self.bLoggedNoMesh = false

    self.DebugLogAccum = 0.0

    return self
end

function TopDownArrowDemo:BeginPlay()
    self:Initialize()
end

function TopDownArrowDemo:Initialize()
    if self.bInitialized then
        return true
    end

    local owner = self.Actor or Actor or Owner
    if not owner then
        log("[TopDownArrowDemo] Owner actor is missing.")
        return false
    end

    self.Actor = owner
    self:FindMesh(owner)

    self.bInitialized = true
    log("[TopDownArrowDemo] Initialized.")

    return true
end

function TopDownArrowDemo:FindMesh(owner)
    local mesh = nil

    if owner.GetSkeletalMeshComponent then
        local ok, result = pcall(function()
            return owner:GetSkeletalMeshComponent()
        end)

        if ok then
            mesh = result
        end
    end

    if not mesh and owner.GetComponent then
        local ok, result = pcall(function()
            return owner:GetComponent("SkeletalMeshComponent")
        end)

        if ok then
            mesh = result
        end
    end

    self.Mesh = mesh

    if not mesh and not self.bLoggedNoMesh then
        log("[TopDownArrowDemo] SkeletalMeshComponent not found. Movement works, but AnimGraph parameter will not update.")
        self.bLoggedNoMesh = true
    end
end

function TopDownArrowDemo:UpdateAnimMoveParam(isMoving)
    if self.Properties.bUpdateAnimGraphMoveParam == false then
        return
    end

    if not self.Mesh then
        return
    end

    if not self.Mesh.SetAnimGraphBool then
        return
    end

    local paramName = self.Properties.MoveParamName or "IsMoving"

    pcall(function()
        self.Mesh:SetAnimGraphBool(paramName, isMoving)
    end)
end

function TopDownArrowDemo:MoveActor(dir, dt)
    if not self.Actor then
        return
    end

    local speed = self.Properties.MoveSpeed or 3.0
    local delta = dir * speed * dt

    if self.Actor.Add_Actor_World_Offset then
        local ok = pcall(function()
            self.Actor:Add_Actor_World_Offset(delta)
        end)

        if not ok then
            self.Actor.Location = self.Actor.Location + delta
        end
    else
        self.Actor.Location = self.Actor.Location + delta
    end

    -- 이동 방향 기준 목표 yaw 계산
    -- X-forward 기준:
    -- dir = (1, 0, 0)  -> yaw 0도
    -- dir = (0, 1, 0)  -> yaw 90도
    -- dir = (0, -1, 0) -> yaw -90도
    local actorYawOffset = self.Properties.ActorYawOffset or 0.0
    local targetYaw = math.deg(atan2_safe(dir.Y, dir.X)) + actorYawOffset

    -- 현재 yaw 가져오기
    local currentYaw = targetYaw
    if self.Actor.Rotation then
        currentYaw = self.Actor.Rotation.Z or targetYaw
    end

    -- 부드럽게 회전
    local interpSpeed = self.Properties.RotationInterpSpeed or 12.0
    local newYaw = interp_angle_degrees(currentYaw, targetYaw, dt, interpSpeed)

    pcall(function()
        self.Actor.Rotation = Vector(0.0, 0.0, newYaw)
    end)
end

function TopDownArrowDemo:UpdateDefaultPawnCamera()
    if not self.Actor then
        return
    end

    local pawn = get_possessed_pawn()
    if not pawn then
        if not self.bLoggedNoPawn then
            log("[TopDownArrowDemo] Possessed DefaultPawn not found yet.")
            self.bLoggedNoPawn = true
        end
        return
    end

    local height = self.Properties.CameraHeight or 8.0
    local back = self.Properties.CameraBackDistance or 3.0
    local side = self.Properties.CameraSideDistance or 0.0

    local actorLocation = self.Actor.Location

    -- Z-up / X-forward 기준
    -- 카메라를 캐릭터의 -X 방향 뒤쪽, Z 위쪽에 배치
    local cameraLocation = actorLocation + Vector(
        -back,
        -side,
        height
    )

    pcall(function()
        pawn.Location = cameraLocation
    end)

    -- 네 엔진 기준:
    -- Vector(0, 90, 0)이 바로 아래를 보는 완전 탑뷰였음.
    -- 그래서 대각선 뷰는 Y값을 90보다 줄임.
    --
    -- height=8, back=3이면 약 69.4도.
    -- back이 0이면 완전 탑뷰 90도.
    local pitchY = 90.0
    if math.abs(back) > 0.001 then
        pitchY = math.deg(math.atan(height / math.abs(back)))
    end

    -- side를 쓰면 Z축 yaw로 좌우 방향 보정
    local yawZ = 0.0
    if math.abs(side) > 0.001 then
        yawZ = math.deg(atan2_safe(side, back))
    end

    pcall(function()
        pawn.Rotation = Vector(0.0, pitchY, yawZ)
    end)

    local controller = get_player_controller()

    if controller and controller.SetViewTargetWithBlend then
        pcall(function()
            controller:SetViewTargetWithBlend(pawn, 0.0, 0)
        end)
    end

    if controller and controller.SetInputModeGameOnly then
        pcall(function()
            controller:SetInputModeGameOnly()
        end)
    end
end

function TopDownArrowDemo:DebugPrintTransforms(dt, dir, isMoving)
    if self.Properties.bDebugTransform == false then
        return
    end

    self.DebugLogAccum = self.DebugLogAccum + dt

    local interval = self.Properties.DebugLogInterval or 0.5
    if self.DebugLogAccum < interval then
        return
    end

    self.DebugLogAccum = 0.0

    local pawn = get_possessed_pawn()

    local actorLocation = self.Actor and self.Actor.Location or nil
    local actorRotation = self.Actor and self.Actor.Rotation or nil
    local pawnLocation = pawn and pawn.Location or nil
    local pawnRotation = pawn and pawn.Rotation or nil

    log("[TopDownArrowDemo][Debug] IsMoving = " .. tostring(isMoving))
    log("[TopDownArrowDemo][Debug] Move Dir = " .. vec_to_string(dir))
    log("[TopDownArrowDemo][Debug] Actor Location = " .. vec_to_string(actorLocation))
    log("[TopDownArrowDemo][Debug] Actor Rotation = " .. vec_to_string(actorRotation))
    log("[TopDownArrowDemo][Debug] Pawn Camera Location = " .. vec_to_string(pawnLocation))
    log("[TopDownArrowDemo][Debug] Pawn Camera Rotation = " .. vec_to_string(pawnRotation))

    if actorLocation and pawnLocation then
        local diff = actorLocation - pawnLocation
        log("[TopDownArrowDemo][Debug] Actor - Camera Diff = " .. vec_to_string(diff))
    end
end

function TopDownArrowDemo:Tick(deltaTime)
    if not self.bInitialized and not self:Initialize() then
        return
    end

    local dt = get_delta_time(deltaTime)

    local dir = make_move_direction()
    local isMoving = dir:SizeSquared2D() > 0.0001

    self:UpdateAnimMoveParam(isMoving)

    if isMoving then
        self:MoveActor(dir, dt)
    end

    self:UpdateDefaultPawnCamera()
    self:DebugPrintTransforms(dt, dir, isMoving)
end

return TopDownArrowDemo