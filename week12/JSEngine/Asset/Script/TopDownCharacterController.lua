local TopDownSupport = require("TopDownSupport")
local TopDownCharacterMotor = require("TopDownCharacterMotor")

local TopDownCharacterController = {}
TopDownCharacterController.__index = TopDownCharacterController

TopDownCharacterController.Properties = {
    MoveSpeed = {
        Type = "Float",
        Default = 3.0,
        Min = 0.0,
        Category = "Movement"
    },

    RotationInterpSpeed = {
        Type = "Float",
        Default = 12.0,
        Min = 0.0,
        Category = "Movement"
    },

    -- 메쉬 방향만 이상하면 여기 조정.
    -- 현재는 MakeMovementConfig에서 강제로 -90.0 넣고 있음.
    ActorYawOffset = {
        Type = "Float",
        Default = 90.0,
        Category = "Movement"
    },

    CameraHeight = {
        Type = "Float",
        Default = 8.0,
        Min = 0.1,
        Category = "Camera"
    },

    CameraYawZ = {
        Type = "Float",
        Default = 90.0,
        Category = "Camera"
    },

    MoveParamName = {
        Type = "String",
        Default = "IsMoving",
        Category = "Animation"
    },

    -- AnimGraph의 Q 공격 전환용 Bool 파라미터 이름
    AttackParamName = {
        Type = "String",
        Default = "AttackQ",
        Category = "Animation"
    },

    -- Q를 눌렀을 때 AttackQ=true를 유지하는 시간.
    -- Run 상태에서 Q를 눌러도 Run -> Idle -> AttackQ로 넘어갈 시간을 주기 위해
    -- 한 프레임보다 길게 잡음.
    AttackTriggerHoldTime = {
        Type = "Float",
        Default = 0.45,
        Min = 0.01,
        Category = "Attack"
    },

    -- Q 애니메이션 중 이동을 막는 시간.
    -- 실제 Q 애니메이션 길이에 맞춰 0.8~1.2 정도로 조절하면 됨.
    AttackLockDuration = {
        Type = "Float",
        Default = 1.0,
        Min = 0.01,
        Category = "Attack"
    },

    bDisableMovementDuringAttack = {
        Type = "Bool",
        Default = true,
        Category = "Attack"
    },

    bUpdateAnimGraphMoveParam = {
        Type = "Bool",
        Default = true,
        Category = "Animation"
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

function TopDownCharacterController.new(scriptComponent, properties)
    local self = setmetatable({}, TopDownCharacterController)

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
    self.CameraPawn = nil

    self.bInitialized = false
    self.bLoggedNoPawn = false
    self.bLoggedNoMesh = false

    self.DebugLogAccum = 0.0

    -- Q 공격 상태 관리
    self.AttackLockRemaining = 0.0
    self.AttackTriggerRemaining = 0.0
    self.bWasAttackKeyDown = false

    return self
end

function TopDownCharacterController:BeginPlay()
    self:Initialize()
end

function TopDownCharacterController:Initialize()
    if self.bInitialized then
        return true
    end

    local owner = self.Actor or Actor or Owner
    if not owner then
        TopDownSupport.Log("[TopDownCharacterController] Owner actor is missing.")
        return false
    end

    self.Actor = owner
    self.Mesh = TopDownSupport.FindSkeletalMesh(owner)

    if not self.Mesh and not self.bLoggedNoMesh then
        TopDownSupport.Log("[TopDownCharacterController] SkeletalMeshComponent not found. Movement works, but AnimGraph parameter will not update.")
        self.bLoggedNoMesh = true
    end

    self.bInitialized = true
    TopDownSupport.Log("[TopDownCharacterController] Initialized.")

    return true
end

function TopDownCharacterController:MakeMovementConfig()
    local p = self.Properties

    return {
        MoveSpeed = p.MoveSpeed or 3.0,
        RotationInterpSpeed = p.RotationInterpSpeed or 12.0,

        -- 지금 네가 먹는다고 확인했던 값 그대로 유지.
        -- 메쉬 방향만 더 틀어야 하면 여기 숫자만 바꾸면 됨.
        ActorYawOffset = -90.0
    }
end

function TopDownCharacterController:IsAttackKeyDown()
    return TopDownSupport.IsKeyDown("Q")
        or TopDownSupport.IsKeyDown("q")
        or TopDownSupport.IsKeyDown("QKey")
end

function TopDownCharacterController:StartAttackQ()
    local p = self.Properties

    self.AttackLockRemaining = p.AttackLockDuration or 1.0
    self.AttackTriggerRemaining = p.AttackTriggerHoldTime or 0.45

    TopDownSupport.Log("[TopDownCharacterController] AttackQ started.")
end

function TopDownCharacterController:UpdateAttackState(dt)
    if self.AttackLockRemaining > 0.0 then
        self.AttackLockRemaining = self.AttackLockRemaining - dt

        if self.AttackLockRemaining < 0.0 then
            self.AttackLockRemaining = 0.0
        end
    end

    if self.AttackTriggerRemaining > 0.0 then
        self.AttackTriggerRemaining = self.AttackTriggerRemaining - dt

        if self.AttackTriggerRemaining < 0.0 then
            self.AttackTriggerRemaining = 0.0
        end
    end

    local bAttackKeyDown = self:IsAttackKeyDown()
    local bAttackPressed = bAttackKeyDown and not self.bWasAttackKeyDown

    -- 공격 중에는 Q 재입력 무시
    if bAttackPressed and self.AttackLockRemaining <= 0.0 then
        self:StartAttackQ()
    end

    self.bWasAttackKeyDown = bAttackKeyDown
end

function TopDownCharacterController:IsAttackLocked()
    if self.Properties.bDisableMovementDuringAttack == false then
        return false
    end

    return self.AttackLockRemaining > 0.0
end

function TopDownCharacterController:IsAttackTriggerActive()
    return self.AttackTriggerRemaining > 0.0
end

function TopDownCharacterController:UpdateAnimation(isMoving)
    local moveParamName = self.Properties.MoveParamName or "IsMoving"
    local attackParamName = self.Properties.AttackParamName or "AttackQ"

    -- 이동 파라미터
    if self.Properties.bUpdateAnimGraphMoveParam ~= false then
        TopDownCharacterMotor.SetAnimMoveParam(
            self.Mesh,
            moveParamName,
            isMoving
        )
    end

    -- Q 공격 파라미터
    -- Q를 누른 직후 일정 시간 true를 유지하고,
    -- 그 이후에는 false로 내려서 AttackQ가 계속 반복 진입하지 않게 함.
    TopDownCharacterMotor.SetAnimMoveParam(
        self.Mesh,
        attackParamName,
        self:IsAttackTriggerActive()
    )
end

function TopDownCharacterController:UpdateCamera()
    if not self.Actor then
        return nil
    end

    local pawn = TopDownSupport.GetPossessedPawn()
    if not pawn then
        return nil
    end

    local actorLocation = self.Actor.Location

    local cameraLocation = actorLocation + Vector(
        0.0,
        0.0,
        10.0
    )

    pcall(function()
        pawn.Location = cameraLocation
    end)

    -- 현재 네 상황 유지:
    -- 카메라 관련 코드는 건드리지 않음.
    local testYawZ = 0

    pcall(function()
        pawn.Rotation = Vector(0.0, 90.0, testYawZ)
    end)

    TopDownSupport.Log(
        "[TopDownCharacterController][CameraTest] Set Camera Rotation = " ..
        TopDownSupport.VectorToString(Vector(0.0, 90.0, testYawZ))
    )

    TopDownSupport.Log(
        "[TopDownCharacterController][CameraTest] Actual Pawn Rotation = " ..
        TopDownSupport.VectorToString(pawn.Rotation)
    )

    local controller = TopDownSupport.GetPlayerController()

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

    return pawn
end

function TopDownCharacterController:UpdateDebug(dt, dir, isMoving)
    if self.Properties.bDebugTransform == false then
        return
    end

    self.DebugLogAccum = self.DebugLogAccum + dt

    local interval = self.Properties.DebugLogInterval or 0.5
    if self.DebugLogAccum < interval then
        return
    end

    self.DebugLogAccum = 0.0

    local actorLocation = self.Actor and self.Actor.Location or nil
    local actorRotation = self.Actor and self.Actor.Rotation or nil
    local pawnLocation = self.CameraPawn and self.CameraPawn.Location or nil
    local pawnRotation = self.CameraPawn and self.CameraPawn.Rotation or nil

    TopDownSupport.Log("[TopDownCharacterController][Debug] IsMoving = " .. tostring(isMoving))
    TopDownSupport.Log("[TopDownCharacterController][Debug] IsAttackLocked = " .. tostring(self:IsAttackLocked()))
    TopDownSupport.Log("[TopDownCharacterController][Debug] AttackTrigger = " .. tostring(self:IsAttackTriggerActive()))
    TopDownSupport.Log("[TopDownCharacterController][Debug] Move Dir = " .. TopDownSupport.VectorToString(dir))
    TopDownSupport.Log("[TopDownCharacterController][Debug] Actor Location = " .. TopDownSupport.VectorToString(actorLocation))
    TopDownSupport.Log("[TopDownCharacterController][Debug] Actor Rotation = " .. TopDownSupport.VectorToString(actorRotation))
    TopDownSupport.Log("[TopDownCharacterController][Debug] Camera Pawn Location = " .. TopDownSupport.VectorToString(pawnLocation))
    TopDownSupport.Log("[TopDownCharacterController][Debug] Camera Pawn Rotation = " .. TopDownSupport.VectorToString(pawnRotation))

    if actorLocation and pawnLocation then
        local diff = actorLocation - pawnLocation
        TopDownSupport.Log("[TopDownCharacterController][Debug] Actor - Camera Diff = " .. TopDownSupport.VectorToString(diff))
    end
end

function TopDownCharacterController:Tick(deltaTime)
    if not self.bInitialized and not self:Initialize() then
        return
    end

    local dt = TopDownSupport.GetDeltaTime(deltaTime)

    -- Q 입력/공격 타이머 먼저 갱신
    self:UpdateAttackState(dt)

    local dir = TopDownCharacterMotor.MakeMoveDirection()
    local bHasMoveInput = dir:SizeSquared2D() > 0.0001

    -- 공격 중이면 이동 입력은 읽어도 실제 이동은 막음.
    local bCanMove = not self:IsAttackLocked()
    local isMoving = bHasMoveInput and bCanMove

    self:UpdateAnimation(isMoving)

    if isMoving then
        TopDownCharacterMotor.MoveAndRotate(
            self.Actor,
            dir,
            dt,
            self:MakeMovementConfig()
        )
    end

    self.CameraPawn = self:UpdateCamera()

    if not self.CameraPawn and not self.bLoggedNoPawn then
        TopDownSupport.Log("[TopDownCharacterController] Possessed camera pawn not found yet.")
        self.bLoggedNoPawn = true
    end

    self:UpdateDebug(dt, dir, isMoving)
end

return TopDownCharacterController