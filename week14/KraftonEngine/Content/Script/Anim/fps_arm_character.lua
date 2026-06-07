local ANIM_DIR = "Content/Data/FirstPerson/Animations/FPSARM/"

local IDLE_PATH = ANIM_DIR .. "a_Idle_w_imc_grunt_rifle_helmet_lod0.uasset"
local WALK_PATH = ANIM_DIR .. "pt_Walk_Forward_w_imc_grunt_rifle_helmet_lod0.uasset"
local RUN_PATH = ANIM_DIR .. "pt_Run_Forward_w_imc_grunt_rifle_helmet_lod0.uasset"

local ADS_IDLE_PATH = ANIM_DIR .. "a_Idle_ADS_w_imc_grunt_rifle_helmet_lod0.uasset"
local ADS_WALK_PATH = ANIM_DIR .. "pt_ADS_Walk_Forward_w_imc_grunt_rifle_helmet_lod0.uasset"
local ADS_RUN_PATH = ANIM_DIR .. "pt_Run_Forward_ADS_w_imc_grunt_rifle_helmet_lod0.uasset"

local CROUCH_IDLE_PATH = ANIM_DIR .. "a_Idle_Crouch_w_imc_grunt_rifle_helmet_lod0.uasset"
local CROUCH_WALK_PATH = ANIM_DIR .. "a_CrouchWalkN_w_imc_grunt_rifle_helmet_lod0.uasset"
local CROUCH_ADS_IDLE_PATH = ANIM_DIR .. "a_Idle_Crouch_ADS_w_imc_grunt_rifle_helmet_lod0.uasset"
local CROUCH_ADS_WALK_PATH = ANIM_DIR .. "a_CrouchWalkAdsN_w_imc_grunt_rifle_helmet_lod0.uasset"
local CROUCH_ENTER_PATH = ANIM_DIR .. "a_stand_2_crouch_w_imc_grunt_rifle_helmet_lod0.uasset"
local CROUCH_EXIT_PATH = ANIM_DIR .. "a_crouch_2_stand_w_imc_grunt_rifle_helmet_lod0.uasset"

local SLIDE_PATH = ANIM_DIR .. "pt_Slide_Forward_w_imc_grunt_rifle_helmet_lod0.uasset"
local WALLRUN_LEFT_PATH = ANIM_DIR .. "a_pt_wallrun_left_w_imc_grunt_rifle_helmet_lod0.uasset"
local WALLRUN_RIGHT_PATH = ANIM_DIR .. "a_pt_wallrun_right_w_imc_grunt_rifle_helmet_lod0.uasset"

local JUMP_START_PATH = ANIM_DIR .. "a_MP_Jump_start_w_imc_grunt_rifle_helmet_lod0.uasset"
local JUMP_FLOAT_PATH = ANIM_DIR .. "a_MP_Jump_float_w_imc_grunt_rifle_helmet_lod0.uasset"
local JUMP_LAND_PATH = ANIM_DIR .. "a_MP_Jump_end_w_imc_grunt_rifle_helmet_lod0.uasset"

local SHOOT_HIP_PATH = ANIM_DIR .. "a_Fire_w_imc_grunt_rifle_helmet_lod0.uasset"
local SHOOT_ADS_PATH = ANIM_DIR .. "a_shootGeneric_w_imc_grunt_rifle_helmet_lod0.uasset"
local SHOOT_CROUCH_PATH = ANIM_DIR .. "a_CrouchFire_w_imc_grunt_rifle_helmet_lod0.uasset"
local RELOAD_PATH = ANIM_DIR .. "a_Rifle_Reload_w_imc_grunt_rifle_helmet_lod0.uasset"
local RELOAD_CROUCH_PATH = ANIM_DIR .. "a_RifleReload_Crouch_w_imc_grunt_rifle_helmet_lod0.uasset"

local KEY_W = (Key and Key.W) or string.byte("W")
local KEY_SHIFT = (Key and Key.Shift) or 0x10
local KEY_MOUSE_RIGHT = (Key and Key.MouseRight) or 0x02

local MOVE_IDLE = 0
local MOVE_WALK = 1
local MOVE_RUN = 2
local MOVE_ADS_IDLE = 3
local MOVE_ADS_WALK = 4
local MOVE_ADS_RUN = 5
local MOVE_CROUCH_IDLE = 6
local MOVE_CROUCH_WALK = 7
local MOVE_CROUCH_ADS_IDLE = 8
local MOVE_CROUCH_ADS_WALK = 9
local MOVE_SLIDE = 10
local MOVE_WALLRUN_LEFT = 11
local MOVE_WALLRUN_RIGHT = 12
local MOVE_JUMP_START = 13
local MOVE_JUMP_FLOAT = 14
local MOVE_JUMP_LAND = 15
local MOVE_CROUCH_ENTER = 16
local MOVE_CROUCH_EXIT = 17

local LOCOMOTION_BLEND_TIME = 0.18
local MOVING_SPEED_THRESHOLD = 20.0
local JUMP_START_DURATION = 0.18
local JUMP_LAND_DURATION = 0.20
local CROUCH_TRANSITION_DURATION = 0.24

local SHOOT_DURATION = 0.12
local SHOOT_BLEND_IN = 0.04
local SHOOT_BLEND_OUT = 0.08
local RELOAD_DURATION = 1.1
local RELOAD_BLEND_IN = 0.10
local RELOAD_BLEND_OUT = 0.16

local SHOOT_HIP = "Hip"
local SHOOT_ADS = "ADS"
local SHOOT_CROUCH = "Crouch"

local function consume_anim_request(self, name)
    local all_requests = rawget(_G, "FPSArmAnimRequests")
    if all_requests == nil then
        return false
    end

    local owner_requests = all_requests[self.OwnerUUID]
    if owner_requests == nil or owner_requests[name] ~= true then
        return false
    end

    owner_requests[name] = false
    return true
end

local function begin_reload(self, crouched)
    self.ReloadRequested = false
    self.ReloadCrouchRequested = false
    self.ShootRequested = false
    self.ActionName = crouched and "ReloadCrouch" or "Reload"
    self.ActionElapsed = 0.0
    return true
end

local function begin_shoot(self, variant, state_index)
    if self.ActionName == "Reload" or self.ActionName == "ReloadCrouch" then
        self.ShootRequested = false
        return false
    end

    self.ShootRequested = false
    self.ActionName = "Shoot"
    self.ActionElapsed = 0.0
    self.CurrentShootState = state_index
    self.CurrentShootVariant = variant
    return true
end

local function finish_action(self, action_name, duration)
    if self.ActionName == action_name and self.ActionElapsed >= duration then
        self.ActionName = nil
        self.ActionElapsed = 0.0
        return true
    end
    return false
end

local function finish_reload_action(self)
    if (self.ActionName == "Reload" or self.ActionName == "ReloadCrouch") and self.ActionElapsed >= RELOAD_DURATION then
        self.ActionName = nil
        self.ActionElapsed = 0.0
        return true
    end
    return false
end

local function add_locomotion_pose(self, path, looping)
    Anim.blend_list_add_pose(self.Locomotion, Anim.create_sequence_player(path, 1.0, looping))
end

local function add_shoot_pair(top, self, variant, state_a, state_b, path)
    Anim.sm_add_state(top, state_a, Anim.create_sequence_player(path, 1.0, false))
    Anim.sm_add_state(top, state_b, Anim.create_sequence_player(path, 1.0, false))

    Anim.sm_add_transition(top, "AnyState", state_a,
        function()
            if self.ShootRequested and self.ShootVariant == variant and self.CurrentShootState ~= 1 then
                return begin_shoot(self, variant, 1)
            end
            return false
        end, SHOOT_BLEND_IN)

    Anim.sm_add_transition(top, "AnyState", state_b,
        function()
            if self.ShootRequested and self.ShootVariant == variant then
                return begin_shoot(self, variant, 2)
            end
            return false
        end, SHOOT_BLEND_IN)

    Anim.sm_add_transition(top, state_a, "Locomotion",
        function() return finish_action(self, "Shoot", SHOOT_DURATION) end,
        SHOOT_BLEND_OUT)

    Anim.sm_add_transition(top, state_b, "Locomotion",
        function() return finish_action(self, "Shoot", SHOOT_DURATION) end,
        SHOOT_BLEND_OUT)
end

local function select_shoot_variant(crouching, sliding, ads)
    if crouching or sliding then
        return SHOOT_CROUCH
    end

    if ads then
        return SHOOT_ADS
    end

    return SHOOT_HIP
end

local function select_locomotion_state(self, moving, sprinting, grounded, crouching, sliding, ads)
    if Anim.is_owner_wall_running() then
        if Anim.is_owner_wall_running_on_right_side() then
            return MOVE_WALLRUN_RIGHT
        end
        return MOVE_WALLRUN_LEFT
    end

    if sliding then
        return MOVE_SLIDE
    end

    if not grounded then
        if self.AirElapsed < JUMP_START_DURATION then
            return MOVE_JUMP_START
        end
        return MOVE_JUMP_FLOAT
    end

    if self.LandingElapsed < JUMP_LAND_DURATION then
        return MOVE_JUMP_LAND
    end

    if self.CrouchTransitionState ~= nil then
        return self.CrouchTransitionState
    end

    if crouching then
        if ads then
            return moving and MOVE_CROUCH_ADS_WALK or MOVE_CROUCH_ADS_IDLE
        end
        return moving and MOVE_CROUCH_WALK or MOVE_CROUCH_IDLE
    end

    if ads then
        if sprinting then
            return MOVE_ADS_RUN
        end
        return moving and MOVE_ADS_WALK or MOVE_ADS_IDLE
    end

    if sprinting then
        return MOVE_RUN
    end

    return moving and MOVE_WALK or MOVE_IDLE
end

function init(self)
    self.OwnerUUID = Anim.get_owner_uuid()
    self.MoveState = MOVE_IDLE
    self.Speed = 0.0
    self.ActionName = nil
    self.ActionElapsed = 0.0
    self.ShootRequested = false
    self.ReloadRequested = false
    self.ReloadCrouchRequested = false
    self.ShootVariant = SHOOT_HIP
    self.CurrentShootVariant = SHOOT_HIP
    self.CurrentShootState = 2
    self.WasFalling = false
    self.AirElapsed = 0.0
    self.LandingElapsed = JUMP_LAND_DURATION
    self.WasCrouching = false
    self.CrouchTransitionState = nil
    self.CrouchTransitionElapsed = 0.0

    self.Locomotion = Anim.create_blend_list_by_enum(MOVE_IDLE, LOCOMOTION_BLEND_TIME)
    add_locomotion_pose(self, IDLE_PATH, true)
    add_locomotion_pose(self, WALK_PATH, true)
    add_locomotion_pose(self, RUN_PATH, true)
    add_locomotion_pose(self, ADS_IDLE_PATH, true)
    add_locomotion_pose(self, ADS_WALK_PATH, true)
    add_locomotion_pose(self, ADS_RUN_PATH, true)
    add_locomotion_pose(self, CROUCH_IDLE_PATH, true)
    add_locomotion_pose(self, CROUCH_WALK_PATH, true)
    add_locomotion_pose(self, CROUCH_ADS_IDLE_PATH, true)
    add_locomotion_pose(self, CROUCH_ADS_WALK_PATH, true)
    add_locomotion_pose(self, SLIDE_PATH, true)
    add_locomotion_pose(self, WALLRUN_LEFT_PATH, true)
    add_locomotion_pose(self, WALLRUN_RIGHT_PATH, true)
    add_locomotion_pose(self, JUMP_START_PATH, false)
    add_locomotion_pose(self, JUMP_FLOAT_PATH, true)
    add_locomotion_pose(self, JUMP_LAND_PATH, false)
    add_locomotion_pose(self, CROUCH_ENTER_PATH, false)
    add_locomotion_pose(self, CROUCH_EXIT_PATH, false)

    local top = Anim.create_state_machine("FPSArmTop")
    Anim.sm_add_state(top, "Locomotion", self.Locomotion)
    Anim.sm_add_state(top, "Reload", Anim.create_sequence_player(RELOAD_PATH, 1.0, false))
    Anim.sm_add_state(top, "ReloadCrouch", Anim.create_sequence_player(RELOAD_CROUCH_PATH, 1.0, false))

    Anim.sm_add_transition(top, "AnyState", "ReloadCrouch",
        function()
            if self.ReloadCrouchRequested then
                return begin_reload(self, true)
            end
            return false
        end, RELOAD_BLEND_IN)

    Anim.sm_add_transition(top, "AnyState", "Reload",
        function()
            if self.ReloadRequested then
                return begin_reload(self, false)
            end
            return false
        end, RELOAD_BLEND_IN)

    add_shoot_pair(top, self, SHOOT_CROUCH, "ShootCrouchA", "ShootCrouchB", SHOOT_CROUCH_PATH)
    add_shoot_pair(top, self, SHOOT_ADS, "ShootAdsA", "ShootAdsB", SHOOT_ADS_PATH)
    add_shoot_pair(top, self, SHOOT_HIP, "ShootHipA", "ShootHipB", SHOOT_HIP_PATH)

    Anim.sm_add_transition(top, "Reload", "Locomotion",
        function() return finish_reload_action(self) end,
        RELOAD_BLEND_OUT)

    Anim.sm_add_transition(top, "ReloadCrouch", "Locomotion",
        function() return finish_reload_action(self) end,
        RELOAD_BLEND_OUT)

    Anim.sm_set_initial_state(top, "Locomotion")
    Anim.set_root_node(Anim.create_slot("DefaultSlot", top))
end

function update(self, dt)
    self.Speed = Anim.get_owner_speed()

    local forward_down = Anim.is_key_down(KEY_W)
    local sprint_down = Anim.is_key_down(KEY_SHIFT)
    local ads = Anim.is_key_down(KEY_MOUSE_RIGHT)
    local grounded = not Anim.is_owner_falling()
    local crouching = Anim.is_owner_crouching()
    local sliding = Anim.is_owner_sliding()
    local moving = forward_down or self.Speed > MOVING_SPEED_THRESHOLD
    local sprinting = grounded and moving and sprint_down and not crouching and not sliding

    if not grounded then
        if not self.WasFalling then
            self.AirElapsed = 0.0
        end
        self.AirElapsed = self.AirElapsed + dt
    else
        if self.WasFalling then
            self.LandingElapsed = 0.0
        end
        self.AirElapsed = 0.0
    end
    self.WasFalling = not grounded

    if self.LandingElapsed < JUMP_LAND_DURATION then
        self.LandingElapsed = self.LandingElapsed + dt
    end

    if grounded and not sliding and crouching ~= self.WasCrouching then
        self.CrouchTransitionState = crouching and MOVE_CROUCH_ENTER or MOVE_CROUCH_EXIT
        self.CrouchTransitionElapsed = 0.0
    end
    self.WasCrouching = crouching

    if self.CrouchTransitionState ~= nil then
        self.CrouchTransitionElapsed = self.CrouchTransitionElapsed + dt
        if self.CrouchTransitionElapsed >= CROUCH_TRANSITION_DURATION then
            self.CrouchTransitionState = nil
            self.CrouchTransitionElapsed = 0.0
        end
    end

    self.MoveState = select_locomotion_state(self, moving, sprinting, grounded, crouching, sliding, ads)
    self.ShootVariant = select_shoot_variant(crouching, sliding, ads)
    Anim.blend_list_set_active(self.Locomotion, self.MoveState)

    if self.ActionName ~= nil then
        self.ActionElapsed = self.ActionElapsed + dt
    end

    if consume_anim_request(self, "reload") then
        self.ReloadRequested = true
    end

    if consume_anim_request(self, "reload_crouch") then
        self.ReloadCrouchRequested = true
    end

    if consume_anim_request(self, "shoot") then
        self.ShootRequested = true
    end
end
