local ANIM_ROOT = "Content/Data/Boss/Heavy/Animations/"

local IDLE_PATH = ANIM_ROOT .. "a_IDLE_combat_deadbolt_titan_torso_d_lod0.uasset"
local WALK_PATH = ANIM_ROOT .. "a_WalkCenter_deadbolt_titan_torso_d_lod0.uasset"
local RUN_PATH = ANIM_ROOT .. "a_combat_run_F_deadbolt_titan_torso_d_lod0.uasset"
local STRAFE_LEFT_PATH = ANIM_ROOT .. "a_combat_walk_L_deadbolt_titan_torso_d_lod0.uasset"
local STRAFE_RIGHT_PATH = ANIM_ROOT .. "a_combat_walk_R_deadbolt_titan_torso_d_lod0.uasset"
local RETREAT_PATH = ANIM_ROOT .. "a_bound_back_deadbolt_titan_torso_d_lod0.uasset"
local LEAP_FLOAT_PATH = ANIM_ROOT .. "a_MP_Jump_float_deadbolt_titan_torso_d_lod0.uasset"

local CANNON_PATH = ANIM_ROOT .. "a_Fire_auto_deadbolt_titan_torso_d_lod0.uasset"
local POWER_SHOT_PATH = ANIM_ROOT .. "htLegion_MP_Stand_PowerShot_deadbolt_titan_torso_d_lod0.uasset"
local MELEE_PATH = ANIM_ROOT .. "at_elite_melee_low_stomp_F_deadbolt_titan_torso_d_lod0.uasset"
local LEAP_START_PATH = ANIM_ROOT .. "a_MP_Jump_start_deadbolt_titan_torso_d_lod0.uasset"
local LEAP_LAND_PATH = ANIM_ROOT .. "a_traverse_land_A_deadbolt_titan_torso_d_lod0.uasset"
local PHASE_PATH = ANIM_ROOT .. "a_Legion_gunup_deadbolt_titan_torso_d_lod0.uasset"
local HIT_REACT_PATH = ANIM_ROOT .. "at_combat_start_react_deadbolt_titan_torso_d_lod0.uasset"

local MOVE_IDLE = 0
local MOVE_WALK = 1
local MOVE_RUN = 2
local MOVE_STRAFE_LEFT = 3
local MOVE_STRAFE_RIGHT = 4
local MOVE_RETREAT = 5
local MOVE_LEAP_FLOAT = 6
local MOVE_MAX = MOVE_LEAP_FLOAT

local LOCOMOTION_BLEND_TIME = 0.18
local DEFAULT_ACTION_BLEND_IN = 0.1
local DEFAULT_ACTION_BLEND_OUT = 0.14
local MELEE_BLEND_IN = 0.12
local MELEE_BLEND_OUT = 0.22
local LEAP_BLEND_IN = 0.08
local LEAP_BLEND_OUT = 0.18
local HIT_REACT_BLEND_IN = 0.06
local AIM_RESPONSE = 8.0
local AIM_WEIGHT_RESPONSE = 10.0

local AIM_BONES = {
    { name = "def_c_spineA", share = 0.20 },
    { name = "def_c_spineB", share = 0.35 },
    { name = "def_c_spineC", share = 0.30 },
    { name = "def_c_neckA", share = 0.15 },
}

local ACTION_DURATIONS = {
    cannon = 0.85,
    powerShot = 1.25,
    melee = 3.8,
    retreat = 0.55,
    leapStart = 0.55,
    leapLand = 0.7,
    phase = 1.0,
    hitReact = 1.4,
}

local function clamp(value, minValue, maxValue)
    if value < minValue then return minValue end
    if value > maxValue then return maxValue end
    return value
end

local function lerp(a, b, alpha)
    return a + (b - a) * alpha
end

local function clamp_move_state(value)
    if value == nil then return MOVE_IDLE end
    if value < MOVE_IDLE then return MOVE_IDLE end
    if value > MOVE_MAX then return MOVE_MAX end
    return value
end

local function get_external_state(self)
    local allStates = rawget(_G, "BossTitanAnimState")
    if allStates == nil or self.OwnerUUID == nil then
        return nil
    end

    return allStates[self.OwnerUUID]
end

local function begin_action(self, name)
    local external = get_external_state(self)
    if external == nil then return false end
    if external.ActionName ~= name then return false end

    local serial = external.ActionSerial or 0
    if serial == self.LastActionSerial then return false end

    self.LastActionSerial = serial
    self.ActionName = name
    self.ActionElapsed = 0.0
    self.ActionDuration = external.ActionDuration or ACTION_DURATIONS[name] or 1.0
    return true
end

local function finish_action(self, name)
    if self.ActionName == name and self.ActionElapsed >= self.ActionDuration then
        self.ActionName = nil
        self.ActionElapsed = 0.0
        self.ActionDuration = 0.0
        return true
    end
    return false
end

local function apply_aim_offsets(self)
    if self.AimWeight <= 0.001 then return end
    if type(Anim.set_bone_rotation_offset) ~= "function" then return end

    for _, bone in ipairs(AIM_BONES) do
        Anim.set_bone_rotation_offset(
            bone.name,
            self.AimPitch * bone.share,
            0.0,
            0.0,
            self.AimWeight)
    end
end

function init(self)
    self.OwnerUUID = Anim.get_owner_uuid()
    self.MoveState = MOVE_IDLE
    self.ActionName = nil
    self.ActionElapsed = 0.0
    self.ActionDuration = 0.0
    self.LastActionSerial = 0
    self.AimPitch = 0.0
    self.AimWeight = 0.0

    self.Locomotion = Anim.create_blend_list_by_enum(MOVE_IDLE, LOCOMOTION_BLEND_TIME)
    Anim.blend_list_add_pose(self.Locomotion, Anim.create_sequence_player(IDLE_PATH, 1.0, true))
    Anim.blend_list_add_pose(self.Locomotion, Anim.create_sequence_player(WALK_PATH, 1.0, true))
    Anim.blend_list_add_pose(self.Locomotion, Anim.create_sequence_player(RUN_PATH, 1.0, true))
    Anim.blend_list_add_pose(self.Locomotion, Anim.create_sequence_player(STRAFE_LEFT_PATH, 1.0, true))
    Anim.blend_list_add_pose(self.Locomotion, Anim.create_sequence_player(STRAFE_RIGHT_PATH, 1.0, true))
    Anim.blend_list_add_pose(self.Locomotion, Anim.create_sequence_player(RETREAT_PATH, 1.0, true))
    Anim.blend_list_add_pose(self.Locomotion, Anim.create_sequence_player(LEAP_FLOAT_PATH, 1.0, true))

    local top = Anim.create_state_machine("BossTitanTop")
    Anim.sm_add_state(top, "Locomotion", self.Locomotion)
    Anim.sm_add_state(top, "Cannon", Anim.create_sequence_player(CANNON_PATH, 1.0, false))
    Anim.sm_add_state(top, "PowerShot", Anim.create_sequence_player(POWER_SHOT_PATH, 1.0, false))
    Anim.sm_add_state(top, "Melee", Anim.create_sequence_player(MELEE_PATH, 1.0, false))
    Anim.sm_add_state(top, "Retreat", Anim.create_sequence_player(RETREAT_PATH, 1.0, false))
    Anim.sm_add_state(top, "LeapStart", Anim.create_sequence_player(LEAP_START_PATH, 1.0, false))
    Anim.sm_add_state(top, "LeapLand", Anim.create_sequence_player(LEAP_LAND_PATH, 1.0, false))
    Anim.sm_add_state(top, "Phase", Anim.create_sequence_player(PHASE_PATH, 1.0, false))
    Anim.sm_add_state(top, "HitReact", Anim.create_sequence_player(HIT_REACT_PATH, 1.0, false))

    Anim.sm_add_transition(top, "AnyState", "HitReact",
        function() return begin_action(self, "hitReact") end,
        HIT_REACT_BLEND_IN)

    Anim.sm_add_transition(top, "AnyState", "Phase",
        function() return begin_action(self, "phase") end,
        DEFAULT_ACTION_BLEND_IN)

    Anim.sm_add_transition(top, "AnyState", "LeapStart",
        function() return begin_action(self, "leapStart") end,
        LEAP_BLEND_IN)

    Anim.sm_add_transition(top, "AnyState", "LeapLand",
        function() return begin_action(self, "leapLand") end,
        LEAP_BLEND_IN)

    Anim.sm_add_transition(top, "AnyState", "Melee",
        function() return begin_action(self, "melee") end,
        MELEE_BLEND_IN)

    Anim.sm_add_transition(top, "AnyState", "PowerShot",
        function() return begin_action(self, "powerShot") end,
        DEFAULT_ACTION_BLEND_IN)

    Anim.sm_add_transition(top, "AnyState", "Cannon",
        function() return begin_action(self, "cannon") end,
        DEFAULT_ACTION_BLEND_IN)

    Anim.sm_add_transition(top, "AnyState", "Retreat",
        function() return begin_action(self, "retreat") end,
        DEFAULT_ACTION_BLEND_IN)

    Anim.sm_add_transition(top, "HitReact", "Locomotion",
        function() return finish_action(self, "hitReact") end,
        DEFAULT_ACTION_BLEND_OUT)

    Anim.sm_add_transition(top, "Phase", "Locomotion",
        function() return finish_action(self, "phase") end,
        DEFAULT_ACTION_BLEND_OUT)

    Anim.sm_add_transition(top, "LeapStart", "Locomotion",
        function() return finish_action(self, "leapStart") end,
        LEAP_BLEND_OUT)

    Anim.sm_add_transition(top, "LeapLand", "Locomotion",
        function() return finish_action(self, "leapLand") end,
        LEAP_BLEND_OUT)

    Anim.sm_add_transition(top, "Melee", "Locomotion",
        function() return finish_action(self, "melee") end,
        MELEE_BLEND_OUT)

    Anim.sm_add_transition(top, "PowerShot", "Locomotion",
        function() return finish_action(self, "powerShot") end,
        DEFAULT_ACTION_BLEND_OUT)

    Anim.sm_add_transition(top, "Cannon", "Locomotion",
        function() return finish_action(self, "cannon") end,
        DEFAULT_ACTION_BLEND_OUT)

    Anim.sm_add_transition(top, "Retreat", "Locomotion",
        function() return finish_action(self, "retreat") end,
        DEFAULT_ACTION_BLEND_OUT)

    Anim.sm_set_initial_state(top, "Locomotion")
    Anim.set_root_node(Anim.create_slot("DefaultSlot", top))
end

function update(self, dt)
    local targetAimPitch = 0.0
    local targetAimWeight = 0.0
    local external = get_external_state(self)
    if external ~= nil then
        self.MoveState = clamp_move_state(external.MoveState)
        targetAimPitch = external.AimPitch or 0.0
        targetAimWeight = external.AimWeight or 0.0
    end

    self.AimPitch = lerp(self.AimPitch, targetAimPitch, clamp(dt * AIM_RESPONSE, 0.0, 1.0))
    self.AimWeight = lerp(self.AimWeight, targetAimWeight, clamp(dt * AIM_WEIGHT_RESPONSE, 0.0, 1.0))

    Anim.blend_list_set_active(self.Locomotion, self.MoveState)
    apply_aim_offsets(self)

    if self.ActionName ~= nil then
        self.ActionElapsed = self.ActionElapsed + dt
    end
end
