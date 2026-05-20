local TopDownSupport = require("TopDownSupport")
local TopDownCharacterMotor = require("TopDownCharacterMotor")

local TopDownCharacterAnimGraphController = {}
TopDownCharacterAnimGraphController.__index = TopDownCharacterAnimGraphController

TopDownCharacterAnimGraphController.Properties = {
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

    ActorYawOffset = {
        Type = "Float",
        Default = -90.0,
        Category = "Movement"
    },

    AnimGraphPath = {
        Type = "String",
        Default = "Asset/Animation Graph/TopDownStateMachine.animgraph",
        Category = "Animation"
    },

    MoveParamName = {
        Type = "String",
        Default = "IsMoving",
        Category = "Animation"
    },

    CameraHeight = {
        Type = "Float",
        Default = 10.0,
        Min = 0.1,
        Category = "Camera"
    },

    CameraBackDistance = {
        Type = "Float",
        Default = 0.0,
        Category = "Camera"
    },

    CameraSideDistance = {
        Type = "Float",
        Default = 0.0,
        Category = "Camera"
    },

    CameraLookAtHeight = {
        Type = "Float",
        Default = 0.0,
        Category = "Camera"
    },

    bDebugTransform = {
        Type = "Bool",
        Default = false,
        Category = "Debug"
    },

    DebugLogInterval = {
        Type = "Float",
        Default = 0.5,
        Min = 0.1,
        Category = "Debug"
    },
}

function TopDownCharacterAnimGraphController.new(scriptComponent, properties)
    local self = setmetatable({}, TopDownCharacterAnimGraphController)

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
    self.bLastMoving = nil
    self.DebugLogAccum = 0.0

    return self
end

function TopDownCharacterAnimGraphController:BeginPlay()
    self:Initialize()
end

function TopDownCharacterAnimGraphController:Initialize()
    if self.bInitialized then
        return true
    end

    local owner = self.Actor or Actor or Owner
    if not owner then
        TopDownSupport.Log("[TopDownCharacterAnimGraphController] Owner actor is missing.")
        return false
    end

    self.Actor = owner
    self.Mesh = TopDownSupport.FindSkeletalMesh(owner)

    if not self.Mesh then
        if not self.bLoggedNoMesh then
            TopDownSupport.Log("[TopDownCharacterAnimGraphController] SkeletalMeshComponent not found.")
            self.bLoggedNoMesh = true
        end
        return false
    end

    local graphPath = self.Properties.AnimGraphPath
        or "Asset/Animation Graph/TopDownStateMachine.animgraph"
    local moveParamName = self.Properties.MoveParamName or "IsMoving"

    if self.Mesh.SetAnimGraphAssetPath then
        pcall(function()
            self.Mesh:SetAnimGraphAssetPath(graphPath)
        end)
    end

    if self.Mesh.SetAnimGraphBool then
        pcall(function()
            self.Mesh:SetAnimGraphBool(moveParamName, false)
        end)
    end

    self.bLastMoving = false
    self.bInitialized = true

    TopDownSupport.Log("[TopDownCharacterAnimGraphController] AnimGraph applied: " .. graphPath)
    return true
end

function TopDownCharacterAnimGraphController:MakeMovementConfig()
    local p = self.Properties

    return {
        MoveSpeed = p.MoveSpeed or 3.0,
        RotationInterpSpeed = p.RotationInterpSpeed or 12.0,
        ActorYawOffset = p.ActorYawOffset or -90.0
    }
end

function TopDownCharacterAnimGraphController:UpdateMoveParameter(isMoving)
    if self.bLastMoving == isMoving then
        return
    end

    self.bLastMoving = isMoving

    if not self.Mesh or not self.Mesh.SetAnimGraphBool then
        return
    end

    local moveParamName = self.Properties.MoveParamName or "IsMoving"

    pcall(function()
        self.Mesh:SetAnimGraphBool(moveParamName, isMoving)
    end)
end

function TopDownCharacterAnimGraphController:UpdateCamera()
    if not self.Actor then
        return nil
    end

    local pawn = TopDownSupport.GetPossessedPawn()
    if not pawn then
        return nil
    end

    local p = self.Properties
    local actorLocation = self.Actor.Location
    local cameraLocation = actorLocation + Vector(
        -(p.CameraBackDistance or 0.0),
        p.CameraSideDistance or 0.0,
        p.CameraHeight or 10.0
    )

    pcall(function()
        pawn.Location = cameraLocation
    end)

    local controller = TopDownSupport.GetPlayerController()
    local needsViewTarget = true

    if controller and controller.GetViewTargetActor then
        local ok, viewTarget = pcall(function()
            return controller:GetViewTargetActor()
        end)

        needsViewTarget = not (ok and viewTarget == pawn)
    end

    if needsViewTarget and controller and controller.SetViewTargetWithBlend then
        pcall(function()
            controller:SetViewTargetWithBlend(pawn, 0.0, 0)
        end)
    end

    local camera = nil
    if controller and controller.GetViewTargetCamera then
        local ok, result = pcall(function()
            return controller:GetViewTargetCamera()
        end)

        if ok then
            camera = result
        end
    end

    if not camera and pawn.GetComponent then
        local ok, result = pcall(function()
            return pawn:GetComponent("CameraComponent")
        end)

        if ok then
            camera = result
        end
    end

    if camera and camera.look_at then
        local lookTarget = actorLocation + Vector(0.0, 0.0, p.CameraLookAtHeight or 0.0)
        pcall(function()
            camera:look_at(lookTarget)
        end)
    end

    if controller and controller.SetInputModeGameOnly then
        pcall(function()
            controller:SetInputModeGameOnly()
        end)
    end

    return pawn
end

function TopDownCharacterAnimGraphController:UpdateDebug(dt, dir, isMoving)
    if self.Properties.bDebugTransform == false then
        return
    end

    self.DebugLogAccum = self.DebugLogAccum + dt

    local interval = self.Properties.DebugLogInterval or 0.5
    if self.DebugLogAccum < interval then
        return
    end

    self.DebugLogAccum = 0.0

    TopDownSupport.Log("[TopDownCharacterAnimGraphController][Debug] IsMoving = " .. tostring(isMoving))
    TopDownSupport.Log("[TopDownCharacterAnimGraphController][Debug] Move Dir = " .. TopDownSupport.VectorToString(dir))
    TopDownSupport.Log("[TopDownCharacterAnimGraphController][Debug] Actor Location = " .. TopDownSupport.VectorToString(self.Actor and self.Actor.Location or nil))
    TopDownSupport.Log("[TopDownCharacterAnimGraphController][Debug] Camera Pawn Location = " .. TopDownSupport.VectorToString(self.CameraPawn and self.CameraPawn.Location or nil))
end

function TopDownCharacterAnimGraphController:Tick(deltaTime)
    if not self.bInitialized and not self:Initialize() then
        return
    end

    local dt = TopDownSupport.GetDeltaTime(deltaTime)
    local dir = TopDownCharacterMotor.MakeMoveDirection()
    local isMoving = dir:SizeSquared2D() > 0.0001

    self:UpdateMoveParameter(isMoving)

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
        TopDownSupport.Log("[TopDownCharacterAnimGraphController] Possessed camera pawn not found yet.")
        self.bLoggedNoPawn = true
    end

    self:UpdateDebug(dt, dir, isMoving)
end

return TopDownCharacterAnimGraphController
