local ANIM_ROOT = "Content/Data/Enemy/"

local IDLE_PATH = ANIM_ROOT .. "Robodude_Take_001.uasset"
local ALERT_IDLE_PATH = ANIM_ROOT .. "rifle aiming idle_mixamo_com.uasset"
local CHASE_PATH = ANIM_ROOT .. "rifle run_mixamo_com.uasset"
local FIRE_PATH = ANIM_ROOT .. "firing rifle_mixamo_com.uasset"
local HIT_PATH = ANIM_ROOT .. "hit reaction_mixamo_com.uasset"

local STATE_IDLE = 0
local STATE_ALERT = 1
local STATE_CHASE = 2
local STATE_MAX = STATE_CHASE

local LOCOMOTION_BLEND_TIME = 0.18
local FIRE_BLEND_IN = 0.06
local FIRE_BLEND_OUT = 0.12
local HIT_BLEND_IN = 0.04
local HIT_BLEND_OUT = 0.12

local ACTION_DURATIONS = {
    fire = 0.55,
    hit = 0.45,
}

local function clamp_state(value)
    if value == nil then return STATE_IDLE end
    if value < STATE_IDLE then return STATE_IDLE end
    if value > STATE_MAX then return STATE_MAX end
    return value
end

local function get_external_state(self)
    local allStates = rawget(_G, "SimpleEnemyAnimState")
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
    self.ActionDuration = external.ActionDuration or ACTION_DURATIONS[name] or 0.5
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

function init(self)
    self.OwnerUUID = Anim.get_owner_uuid()
    self.MoveState = STATE_IDLE
    self.ActionName = nil
    self.ActionElapsed = 0.0
    self.ActionDuration = 0.0
    self.LastActionSerial = 0

    if type(Anim.set_sequence_force_root_lock) == "function" then
        Anim.set_sequence_force_root_lock(CHASE_PATH, true)
    end

    self.Locomotion = Anim.create_blend_list_by_enum(STATE_IDLE, LOCOMOTION_BLEND_TIME)
    Anim.blend_list_add_pose(self.Locomotion, Anim.create_sequence_player(IDLE_PATH, 1.0, true))
    Anim.blend_list_add_pose(self.Locomotion, Anim.create_sequence_player(ALERT_IDLE_PATH, 1.0, true))
    Anim.blend_list_add_pose(self.Locomotion, Anim.create_sequence_player(CHASE_PATH, 1.0, true))

    local top = Anim.create_state_machine("SimpleEnemyRobodudeTop")
    Anim.sm_add_state(top, "Locomotion", self.Locomotion)
    Anim.sm_add_state(top, "Fire", Anim.create_sequence_player(FIRE_PATH, 1.0, false))
    Anim.sm_add_state(top, "Hit", Anim.create_sequence_player(HIT_PATH, 1.0, false))

    Anim.sm_add_transition(top, "AnyState", "Hit",
        function() return begin_action(self, "hit") end,
        HIT_BLEND_IN)

    Anim.sm_add_transition(top, "AnyState", "Fire",
        function() return begin_action(self, "fire") end,
        FIRE_BLEND_IN)

    Anim.sm_add_transition(top, "Hit", "Locomotion",
        function() return finish_action(self, "hit") end,
        HIT_BLEND_OUT)

    Anim.sm_add_transition(top, "Fire", "Locomotion",
        function() return finish_action(self, "fire") end,
        FIRE_BLEND_OUT)

    Anim.sm_set_initial_state(top, "Locomotion")
    Anim.set_root_node(Anim.create_slot("DefaultSlot", top))
end

function update(self, dt)
    local external = get_external_state(self)
    if external ~= nil then
        self.MoveState = clamp_state(external.MoveState)
    end

    Anim.blend_list_set_active(self.Locomotion, self.MoveState)

    if self.ActionName ~= nil then
        self.ActionElapsed = self.ActionElapsed + dt
    end
end
