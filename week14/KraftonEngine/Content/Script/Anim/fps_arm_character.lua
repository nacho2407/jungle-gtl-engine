local IDLE_PATH   = "Content/Data/FPSArm/TitanArm/animations/idle_anim_autoplay_v_rspn101.uasset"
local WALK_PATH   = "Content/Data/FPSArm/TitanArm/animations/walk_anim_v_rspn101.uasset"
local RUN_PATH    = "Content/Data/FPSArm/TitanArm/animations/sprint_seq_v_rspn101.uasset"
local SHOOT_PATH  = "Content/Data/FPSArm/TitanArm/animations/attack_anim_v_rspn101.uasset"
local RELOAD_PATH = "Content/Data/FPSArm/TitanArm/animations/reload_empty_seq_v_rspn101.uasset"
local RELOAD_CROUCH_PATH = "Content/Data/FPSArm/TitanArm/animations/reload_empty_crouch_seq_v_rspn101.uasset"

local KEY_W = (Key and Key.W) or string.byte("W")
local KEY_SHIFT = (Key and Key.Shift) or 0x10

local MOVE_IDLE = 0
local MOVE_WALK = 1
local MOVE_RUN = 2

local LOCOMOTION_BLEND_TIME = 0.22
local SHOOT_DURATION = 0.12
local SHOOT_BLEND_IN = 0.05
local SHOOT_BLEND_OUT = 0.08
local RELOAD_DURATION = 1.1
local RELOAD_BLEND_IN = 0.10
local RELOAD_BLEND_OUT = 0.16

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

local function begin_reload(self)
    self.ReloadRequested = false
    self.ReloadCrouchRequested = false
    self.ShootRequested = false
    self.ActionName = "Reload"
    self.ActionElapsed = 0.0
    return true
end

local function begin_shoot(self, state_index)
    if self.ActionName == "Reload" then
        self.ShootRequested = false
        return false
    end

    self.ShootRequested = false
    self.ActionName = "Shoot"
    self.ActionElapsed = 0.0
    self.CurrentShootState = state_index
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

function init(self)
    self.OwnerUUID = Anim.get_owner_uuid()
    self.MoveState = MOVE_IDLE
    self.Speed = 0.0
    self.ActionName = nil
    self.ActionElapsed = 0.0
    self.ShootRequested = false
    self.ReloadRequested = false
    self.ReloadCrouchRequested = false
    self.CurrentShootState = 2

    self.Locomotion = Anim.create_blend_list_by_enum(MOVE_IDLE, LOCOMOTION_BLEND_TIME)
    Anim.blend_list_add_pose(self.Locomotion, Anim.create_sequence_player(IDLE_PATH, 1.0, true))
    Anim.blend_list_add_pose(self.Locomotion, Anim.create_sequence_player(WALK_PATH, 1.0, true))
    Anim.blend_list_add_pose(self.Locomotion, Anim.create_sequence_player(RUN_PATH, 1.0, true))

    local top = Anim.create_state_machine("FPSArmTop")
    Anim.sm_add_state(top, "Locomotion", self.Locomotion)
    Anim.sm_add_state(top, "ShootA", Anim.create_sequence_player(SHOOT_PATH, 1.0, false))
    Anim.sm_add_state(top, "ShootB", Anim.create_sequence_player(SHOOT_PATH, 1.0, false))
    Anim.sm_add_state(top, "Reload", Anim.create_sequence_player(RELOAD_PATH, 1.0, false))
    Anim.sm_add_state(top, "ReloadCrouch", Anim.create_sequence_player(RELOAD_CROUCH_PATH, 1.0, false))

    Anim.sm_add_transition(top, "AnyState", "ReloadCrouch",
        function()
            if self.ReloadCrouchRequested then
                return begin_reload(self)
            end
            return false
        end, RELOAD_BLEND_IN)

    Anim.sm_add_transition(top, "AnyState", "Reload",
        function()
            if self.ReloadRequested then
                return begin_reload(self)
            end
            return false
        end, RELOAD_BLEND_IN)

    Anim.sm_add_transition(top, "AnyState", "ShootA",
        function()
            if self.ShootRequested and self.CurrentShootState ~= 1 then
                return begin_shoot(self, 1)
            end
            return false
        end, SHOOT_BLEND_IN)

    Anim.sm_add_transition(top, "AnyState", "ShootB",
        function()
            if self.ShootRequested then
                return begin_shoot(self, 2)
            end
            return false
        end, SHOOT_BLEND_IN)

    Anim.sm_add_transition(top, "ShootA", "Locomotion",
        function() return finish_action(self, "Shoot", SHOOT_DURATION) end,
        SHOOT_BLEND_OUT)

    Anim.sm_add_transition(top, "ShootB", "Locomotion",
        function() return finish_action(self, "Shoot", SHOOT_DURATION) end,
        SHOOT_BLEND_OUT)

    Anim.sm_add_transition(top, "Reload", "Locomotion",
        function() return finish_action(self, "Reload", RELOAD_DURATION) end,
        RELOAD_BLEND_OUT)

    Anim.sm_add_transition(top, "ReloadCrouch", "Locomotion",
        function() return finish_action(self, "Reload", RELOAD_DURATION) end,
        RELOAD_BLEND_OUT)

    Anim.sm_set_initial_state(top, "Locomotion")
    Anim.set_root_node(Anim.create_slot("DefaultSlot", top))
end

function update(self, dt)
    self.Speed = Anim.get_owner_speed()

    local forward_down = Anim.is_key_down(KEY_W)
    local sprint_down = Anim.is_key_down(KEY_SHIFT)
    local grounded = not Anim.is_owner_falling()

    if grounded and forward_down and sprint_down then
        self.MoveState = MOVE_RUN
    elseif grounded and forward_down then
        self.MoveState = MOVE_WALK
    else
        self.MoveState = MOVE_IDLE
    end

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
