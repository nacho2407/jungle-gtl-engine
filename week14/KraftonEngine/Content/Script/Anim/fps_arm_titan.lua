local ANIM_DIR = "Content/Data/FPSArm/TitanArm/animations/"

local IDLE_PATH = ANIM_DIR .. "idle_anim_autoplay_v_rspn101.uasset"
local WALK_PATH = ANIM_DIR .. "walk_anim_v_rspn101.uasset"
local SPRINT_PATH = ANIM_DIR .. "sprint_seq_v_rspn101.uasset"
local ATTACK_PATH = ANIM_DIR .. "attack_anim_v_rspn101.uasset"
local RELOAD_PATH = ANIM_DIR .. "reload_empty_seq_v_rspn101.uasset"
local RELOAD_CROUCH_PATH = ANIM_DIR .. "reload_crouch_seq_v_rspn101.uasset"
local RELOAD_EMPTY_CROUCH_PATH = ANIM_DIR .. "reload_empty_crouch_seq_v_rspn101.uasset"

local KEY_W = (Key and Key.W) or string.byte("W")
local KEY_SHIFT = (Key and Key.Shift) or 0x10
local KEY_MOUSE_RIGHT = (Key and Key.MouseRight) or 0x02
local KEY_GAMEPAD_LEFT_THUMB = Key and Key.GamepadLeftThumb
local KEY_GAMEPAD_LEFT_TRIGGER = Key and Key.GamepadLeftTrigger

local MOVE_IDLE = 0
local MOVE_WALK = 1
local MOVE_SPRINT = 2
local MOVE_ADS_IDLE = 3
local MOVE_ADS_WALK = 4
local MOVE_ADS_SPRINT = 5
local MOVE_CROUCH_IDLE = 6
local MOVE_CROUCH_WALK = 7
local MOVE_SLIDE = 8
local MOVE_AIR = 9

local LOCOMOTION_BLEND_TIME = 0.18
local MOVING_SPEED_THRESHOLD = 20.0
local WALK_PLAY_RATE = 1.0
local SPRINT_PLAY_RATE = 1.08
local ADS_IDLE_PLAY_RATE = 0.92
local ADS_WALK_PLAY_RATE = 0.82
local ADS_SPRINT_PLAY_RATE = 0.92
local CROUCH_IDLE_PLAY_RATE = 0.88
local CROUCH_WALK_PLAY_RATE = 0.72
local SLIDE_PLAY_RATE = 0.75
local AIR_PLAY_RATE = 0.55

local SHOOT_DURATION = 0.45
local SHOOT_BLEND_IN = 0.02
local SHOOT_BLEND_OUT = 0.12
local RELOAD_DURATION = 1.4
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

local function begin_reload(self, anim_name)
    self.ReloadRequested = false
    self.ReloadCrouchRequested = false
    self.ShootRequested = false
    self.ActionName = anim_name
    self.ActionElapsed = 0.0
    return true
end

local function begin_shoot(self, state_index)
    if self.ActionName == "Reload" or self.ActionName == "ReloadCrouch" then
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

local function finish_reload_action(self)
    if (self.ActionName == "Reload" or self.ActionName == "ReloadCrouch") and self.ActionElapsed >= RELOAD_DURATION then
        self.ActionName = nil
        self.ActionElapsed = 0.0
        return true
    end
    return false
end

local function add_locomotion_pose(self, path, play_rate, looping)
    Anim.blend_list_add_pose(self.Locomotion, Anim.create_sequence_player(path, play_rate or 1.0, looping))
end

local function select_locomotion_state(moving, sprinting, grounded, crouching, sliding, ads)
    if sliding then
        return MOVE_SLIDE
    end

    if not grounded then
        return MOVE_AIR
    end

    if crouching then
        return moving and MOVE_CROUCH_WALK or MOVE_CROUCH_IDLE
    end

    if ads then
        if sprinting then
            return MOVE_ADS_SPRINT
        end
        return moving and MOVE_ADS_WALK or MOVE_ADS_IDLE
    end

    if sprinting then
        return MOVE_SPRINT
    end

    return moving and MOVE_WALK or MOVE_IDLE
end

local function set_owner_flag(uuid, name, value)
    if uuid == nil then return end
    _G.FPSArmAnimFlags = _G.FPSArmAnimFlags or {}
    local flags = _G.FPSArmAnimFlags[uuid]
    if flags == nil then
        flags = {}
        _G.FPSArmAnimFlags[uuid] = flags
    end
    flags[name] = value
end

function init(self)
    self.OwnerUUID = Anim.get_owner_uuid()
    -- 이 메시 (SK_rspn101_jack) 에는 슬라이드/월런/점프 등 전용 애니가 없어서
    -- 캐릭터 스크립트가 transform 폴백을 켤 수 있도록 플래그를 노출한다.
    set_owner_flag(self.OwnerUUID, "needsSlideTilt", true)
    self.MoveState = MOVE_IDLE
    self.Speed = 0.0
    self.ActionName = nil
    self.ActionElapsed = 0.0
    self.ShootRequested = false
    self.ReloadRequested = false
    self.ReloadCrouchRequested = false
    self.CurrentShootState = 2

    self.Locomotion = Anim.create_blend_list_by_enum(MOVE_IDLE, LOCOMOTION_BLEND_TIME)
    add_locomotion_pose(self, IDLE_PATH, 1.0, true)
    add_locomotion_pose(self, WALK_PATH, WALK_PLAY_RATE, true)
    add_locomotion_pose(self, SPRINT_PATH, SPRINT_PLAY_RATE, true)
    add_locomotion_pose(self, IDLE_PATH, ADS_IDLE_PLAY_RATE, true)
    add_locomotion_pose(self, WALK_PATH, ADS_WALK_PLAY_RATE, true)
    add_locomotion_pose(self, SPRINT_PATH, ADS_SPRINT_PLAY_RATE, true)
    add_locomotion_pose(self, IDLE_PATH, CROUCH_IDLE_PLAY_RATE, true)
    add_locomotion_pose(self, WALK_PATH, CROUCH_WALK_PLAY_RATE, true)
    add_locomotion_pose(self, SPRINT_PATH, SLIDE_PLAY_RATE, true)
    add_locomotion_pose(self, IDLE_PATH, AIR_PLAY_RATE, true)

    local top = Anim.create_state_machine("FPSArmTitanTop")
    Anim.sm_add_state(top, "Locomotion", self.Locomotion)
    Anim.sm_add_state(top, "Reload", Anim.create_sequence_player(RELOAD_PATH, 1.0, false))
    Anim.sm_add_state(top, "ReloadCrouch", Anim.create_sequence_player(RELOAD_CROUCH_PATH, 1.0, false))
    Anim.sm_add_state(top, "ReloadEmptyCrouch", Anim.create_sequence_player(RELOAD_EMPTY_CROUCH_PATH, 1.0, false))
    Anim.sm_add_state(top, "ShootA", Anim.create_sequence_player(ATTACK_PATH, 1.0, false))
    Anim.sm_add_state(top, "ShootB", Anim.create_sequence_player(ATTACK_PATH, 1.0, false))

    Anim.sm_add_transition(top, "AnyState", "ReloadCrouch",
        function()
            if self.ReloadCrouchRequested then
                return begin_reload(self, "ReloadCrouch")
            end
            return false
        end, RELOAD_BLEND_IN)

    Anim.sm_add_transition(top, "AnyState", "Reload",
        function()
            if self.ReloadRequested then
                return begin_reload(self, "Reload")
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
        or (KEY_GAMEPAD_LEFT_THUMB ~= nil and Anim.is_key_down(KEY_GAMEPAD_LEFT_THUMB))
    local ads = Anim.is_key_down(KEY_MOUSE_RIGHT)
        or (KEY_GAMEPAD_LEFT_TRIGGER ~= nil and Anim.is_key_down(KEY_GAMEPAD_LEFT_TRIGGER))
    local crouching = Anim.is_owner_crouching()
    local sliding = Anim.is_owner_sliding()
    local grounded = not Anim.is_owner_falling()
    local moving = forward_down or self.Speed > MOVING_SPEED_THRESHOLD
    local sprinting = grounded and moving and sprint_down and not crouching and not sliding

    self.MoveState = select_locomotion_state(moving, sprinting, grounded, crouching, sliding, ads)
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

    -- 슬라이드/크라우치 등 캐릭터 스크립트에서 movement: API 로 못 읽는 상태를 노출.
    set_owner_flag(self.OwnerUUID, "isSliding", sliding == true)
end
