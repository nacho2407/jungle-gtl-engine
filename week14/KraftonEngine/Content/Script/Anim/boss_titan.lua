local BossAnimDefs = require("Anim.boss_titan_defs")

local ANIM = BossAnimDefs.ANIM

local IDLE_PATH = ANIM.idle
local WALK_PATH = ANIM.walk
local RUN_PATH = ANIM.run
local STRAFE_LEFT_PATH = ANIM.strafeLeft
local STRAFE_RIGHT_PATH = ANIM.strafeRight
local RETREAT_PATH = ANIM.retreat
local LEAP_FLOAT_PATH = ANIM.leapFloat

local CANNON_PATH = ANIM.cannon
local POWER_SHOT_PATH = ANIM.powerShot
local MELEE_PATH = ANIM.melee
local LEAP_START_PATH = ANIM.leapStart
local LEAP_LAND_PATH = ANIM.leapLand
local PHASE_PATH = ANIM.phase
local CROUCH_STAND_PATH = ANIM.crouchStand
local HIT_REACT_PATH = ANIM.hitReact

local MOVE = BossAnimDefs.MOVE
local MOVE_IDLE = MOVE.IDLE
local MOVE_WALK = MOVE.WALK
local MOVE_RUN = MOVE.RUN
local MOVE_STRAFE_LEFT = MOVE.STRAFE_LEFT
local MOVE_STRAFE_RIGHT = MOVE.STRAFE_RIGHT
local MOVE_RETREAT = MOVE.RETREAT
local MOVE_LEAP_FLOAT = MOVE.LEAP_FLOAT
local MOVE_MAX = BossAnimDefs.MOVE_MAX

local LOCOMOTION_BLEND_TIME = 0.18
local DEFAULT_ACTION_BLEND_IN = 0.1
local DEFAULT_ACTION_BLEND_OUT = 0.14
local MELEE_BLEND_IN = 0.12
local MELEE_BLEND_OUT = 0.22
local LEAP_BLEND_IN = 0.08
local LEAP_BLEND_OUT = 0.18
local HIT_REACT_BLEND_IN = 0.06
local PHASE_TRANSITION_BLEND_IN = 0.18
local PHASE_TRANSITION_BLEND_OUT = 0.18
local WALK_PLAY_RATE = 1.0
local RUN_PLAY_RATE = 1.12
local RUN_LOOP_WINDOW = 2.05
local RUN_LOOP_BLEND_TIME = 0.04
local AIM_RESPONSE = 8.0
local AIM_WEIGHT_RESPONSE = 10.0

local AIM_BONES = {
    { name = "def_c_spineA", share = 0.20 },
    { name = "def_c_spineB", share = 0.35 },
    { name = "def_c_spineC", share = 0.30 },
    { name = "def_c_neckA", share = 0.15 },
}

local DEBUG_LOG_INTERVAL = 0.4
local DEBUG_LOG_PATH = "boss_log.txt"

local MOVE_DEBUG_NAMES = BossAnimDefs.MOVE_NAMES

local function debug_log(line)
    if _G.BossDebugLog ~= nil then
        _G.BossDebugLog(line)
    else
        print(line)
        if DebugFile ~= nil and DebugFile.AppendText ~= nil then
            if _G.BossLogSessionOpen ~= true then
                if _G.BossLogPreOpenLines == nil then
                    _G.BossLogPreOpenLines = {}
                end
                table.insert(_G.BossLogPreOpenLines, line)
                _G.BossLogPreOpened = true
                return
            end
            local path = _G.BossLogPath or DEBUG_LOG_PATH
            DebugFile.AppendText(path, line .. "\n")
        end
    end
end

local ACTION_DURATIONS = BossAnimDefs.ACTION_DURATIONS

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

local function begin_sustained_action(self, name)
    local external = get_external_state(self)
    if external == nil then return false end
    if external.ActionName ~= name then return false end
    if self.ActionName == name then return false end

    self.LastActionSerial = external.ActionSerial or self.LastActionSerial
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

local function finish_phase_transition_action(self, name)
    if self.ActionName ~= name or self.ActionElapsed < self.ActionDuration then
        return false
    end

    local external = get_external_state(self)
    if external ~= nil and external.ActionName == name then
        return false
    end

    self.ActionName = nil
    self.ActionElapsed = 0.0
    self.ActionDuration = 0.0
    return true
end

local function finish_sustained_action(self, name)
    if self.ActionName ~= name then return false end

    local external = get_external_state(self)
    if external ~= nil and external.ActionName == name and self.ActionElapsed < self.ActionDuration then
        return false
    end

    self.ActionName = nil
    self.ActionElapsed = 0.0
    self.ActionDuration = 0.0
    return true
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

local function set_root_lock(path, enabled)
    if type(Anim.set_sequence_force_root_lock) ~= "function" then return end
    Anim.set_sequence_force_root_lock(path, enabled)
end

local function force_root_lock(path)
    set_root_lock(path, true)
end

local function use_ai_driven_locomotion(path)
    if type(Anim.set_sequence_enable_root_motion) == "function" then
        Anim.set_sequence_enable_root_motion(path, false)
    end
    set_root_lock(path, false)
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
    self.DebugLogTimer = 0.0
    self.DebugSessionTime = 0.0
    self.RunLoopTimer = 0.0
    self.RunLoopIndex = 0

    debug_log(string.format("[BossAnim] init OwnerUUID=%s WALK=%s RUN=%s",
        tostring(self.OwnerUUID), WALK_PATH, RUN_PATH))
    debug_log(string.format("[BossAnim] external_state_present=%s",
        tostring(get_external_state(self) ~= nil)))

    use_ai_driven_locomotion(WALK_PATH)
    use_ai_driven_locomotion(RUN_PATH)
    use_ai_driven_locomotion(STRAFE_LEFT_PATH)
    use_ai_driven_locomotion(STRAFE_RIGHT_PATH)
    use_ai_driven_locomotion(RETREAT_PATH)
    use_ai_driven_locomotion(LEAP_FLOAT_PATH)
    debug_log(string.format("[BossAnim] locomotion_root_lock=false root_motion=false run_rate=%.2f", RUN_PLAY_RATE))
    force_root_lock(CROUCH_STAND_PATH)

    self.IdlePlayer = Anim.create_sequence_player(IDLE_PATH, 1.0, true)
    self.WalkPlayer = Anim.create_sequence_player(WALK_PATH, WALK_PLAY_RATE, true)
    self.RunPlayerA = Anim.create_sequence_player(RUN_PATH, RUN_PLAY_RATE, true)
    self.RunPlayerB = Anim.create_sequence_player(RUN_PATH, RUN_PLAY_RATE, true)
    self.StrafeLeftPlayer = Anim.create_sequence_player(STRAFE_LEFT_PATH, 1.0, true)
    self.StrafeRightPlayer = Anim.create_sequence_player(STRAFE_RIGHT_PATH, 1.0, true)
    self.RetreatPlayer = Anim.create_sequence_player(RETREAT_PATH, 1.0, true)
    self.LeapFloatPlayer = Anim.create_sequence_player(LEAP_FLOAT_PATH, 1.0, true)
    self.OpeningWalkPlayer = Anim.create_sequence_player(WALK_PATH, WALK_PLAY_RATE, true)
    self.RunLoopBlend = Anim.create_blend_list_by_enum(0, RUN_LOOP_BLEND_TIME)
    Anim.blend_list_add_pose(self.RunLoopBlend, self.RunPlayerA)
    Anim.blend_list_add_pose(self.RunLoopBlend, self.RunPlayerB)

    self.Locomotion = Anim.create_blend_list_by_enum(MOVE_IDLE, LOCOMOTION_BLEND_TIME)
    Anim.blend_list_add_pose(self.Locomotion, self.IdlePlayer)
    Anim.blend_list_add_pose(self.Locomotion, self.WalkPlayer)
    Anim.blend_list_add_pose(self.Locomotion, self.RunLoopBlend)
    Anim.blend_list_add_pose(self.Locomotion, self.StrafeLeftPlayer)
    Anim.blend_list_add_pose(self.Locomotion, self.StrafeRightPlayer)
    Anim.blend_list_add_pose(self.Locomotion, self.RetreatPlayer)
    Anim.blend_list_add_pose(self.Locomotion, self.LeapFloatPlayer)

    local top = Anim.create_state_machine("BossTitanTop")
    Anim.sm_add_state(top, "Locomotion", self.Locomotion)
    Anim.sm_add_state(top, "OpeningWalk", self.OpeningWalkPlayer)
    Anim.sm_add_state(top, "Cannon", Anim.create_sequence_player(CANNON_PATH, 1.0, false))
    Anim.sm_add_state(top, "PowerShot", Anim.create_sequence_player(POWER_SHOT_PATH, 1.0, false))
    Anim.sm_add_state(top, "Melee", Anim.create_sequence_player(MELEE_PATH, 1.0, false))
    Anim.sm_add_state(top, "Retreat", Anim.create_sequence_player(RETREAT_PATH, 1.0, false))
    Anim.sm_add_state(top, "LeapStart", Anim.create_sequence_player(LEAP_START_PATH, 1.0, false))
    Anim.sm_add_state(top, "LeapLand", Anim.create_sequence_player(LEAP_LAND_PATH, 1.0, false))
    Anim.sm_add_state(top, "Phase", Anim.create_sequence_player(PHASE_PATH, 1.0, false))
    Anim.sm_add_state(top, "PhaseCrouchDown", Anim.create_sequence_player(CROUCH_STAND_PATH, -1.0, false))
    Anim.sm_add_state(top, "PhaseCrouchHold", Anim.create_sequence_player(CROUCH_STAND_PATH, 0.0, false))
    Anim.sm_add_state(top, "PhaseStandUp", Anim.create_sequence_player(CROUCH_STAND_PATH, 1.0, false))
    Anim.sm_add_state(top, "HitReact", Anim.create_sequence_player(HIT_REACT_PATH, 1.0, false))

    Anim.sm_add_transition(top, "AnyState", "HitReact",
        function() return begin_action(self, "hitReact") end,
        HIT_REACT_BLEND_IN)

    Anim.sm_add_transition(top, "AnyState", "OpeningWalk",
        function() return begin_sustained_action(self, "openingWalk") end,
        LOCOMOTION_BLEND_TIME)

    Anim.sm_add_transition(top, "AnyState", "Phase",
        function() return begin_action(self, "phase") end,
        DEFAULT_ACTION_BLEND_IN)

    Anim.sm_add_transition(top, "AnyState", "PhaseCrouchDown",
        function() return begin_action(self, "phaseCrouchDown") end,
        PHASE_TRANSITION_BLEND_IN)

    Anim.sm_add_transition(top, "AnyState", "PhaseCrouchHold",
        function() return begin_action(self, "phaseCrouchHold") end,
        PHASE_TRANSITION_BLEND_IN)

    Anim.sm_add_transition(top, "AnyState", "PhaseStandUp",
        function() return begin_action(self, "phaseStandUp") end,
        PHASE_TRANSITION_BLEND_IN)

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

    Anim.sm_add_transition(top, "OpeningWalk", "Locomotion",
        function() return finish_sustained_action(self, "openingWalk") end,
        LOCOMOTION_BLEND_TIME)

    Anim.sm_add_transition(top, "Phase", "Locomotion",
        function() return finish_action(self, "phase") end,
        DEFAULT_ACTION_BLEND_OUT)

    Anim.sm_add_transition(top, "PhaseCrouchDown", "PhaseCrouchHold",
        function() return begin_action(self, "phaseCrouchHold") end,
        PHASE_TRANSITION_BLEND_OUT)

    Anim.sm_add_transition(top, "PhaseCrouchHold", "PhaseStandUp",
        function() return begin_action(self, "phaseStandUp") end,
        PHASE_TRANSITION_BLEND_OUT)

    Anim.sm_add_transition(top, "PhaseCrouchDown", "Locomotion",
        function() return finish_phase_transition_action(self, "phaseCrouchDown") end,
        PHASE_TRANSITION_BLEND_OUT)

    Anim.sm_add_transition(top, "PhaseCrouchHold", "Locomotion",
        function() return finish_phase_transition_action(self, "phaseCrouchHold") end,
        PHASE_TRANSITION_BLEND_OUT)

    Anim.sm_add_transition(top, "PhaseStandUp", "Locomotion",
        function() return finish_phase_transition_action(self, "phaseStandUp") end,
        PHASE_TRANSITION_BLEND_OUT)

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

    if self.MoveState == MOVE_RUN then
        self.RunLoopTimer = self.RunLoopTimer + dt
        if self.RunLoopTimer >= RUN_LOOP_WINDOW then
            self.RunLoopTimer = 0.0
            self.RunLoopIndex = 1 - self.RunLoopIndex
        end
    else
        self.RunLoopTimer = 0.0
        self.RunLoopIndex = 0
    end
    Anim.blend_list_set_active(self.RunLoopBlend, self.RunLoopIndex)
    Anim.blend_list_set_active(self.Locomotion, self.MoveState)
    apply_aim_offsets(self)

    if self.ActionName ~= nil then
        self.ActionElapsed = self.ActionElapsed + dt
    end

    self.DebugSessionTime = self.DebugSessionTime + dt
    self.DebugLogTimer = self.DebugLogTimer - dt
    if self.DebugLogTimer <= 0.0 then
        self.DebugLogTimer = DEBUG_LOG_INTERVAL
        local moveName = MOVE_DEBUG_NAMES[self.MoveState] or "?"
        local extMove = external and (MOVE_DEBUG_NAMES[external.MoveState] or tostring(external.MoveState)) or "no-ext"
        local extAction = external and tostring(external.ActionName) or "no-ext"
        local walkTime = -1.0
        local walkLength = -1.0
        local runTime = -1.0
        local runATime = -1.0
        local runBTime = -1.0
        local runLength = -1.0
        local openingWalkTime = -1.0
        local openingWalkLength = -1.0
        if type(Anim.get_sequence_player_time) == "function" then
            walkTime = Anim.get_sequence_player_time(self.WalkPlayer)
            runATime = Anim.get_sequence_player_time(self.RunPlayerA)
            runBTime = Anim.get_sequence_player_time(self.RunPlayerB)
            runTime = self.RunLoopIndex == 0 and runATime or runBTime
            openingWalkTime = Anim.get_sequence_player_time(self.OpeningWalkPlayer)
        end
        if type(Anim.get_sequence_player_length) == "function" then
            walkLength = Anim.get_sequence_player_length(self.WalkPlayer)
            runLength = Anim.get_sequence_player_length(self.RunPlayerA)
            openingWalkLength = Anim.get_sequence_player_length(self.OpeningWalkPlayer)
        end

        debug_log(string.format(
            "[%.2f][BossAnim] move=%s(%d) ext_move=%s ext_act=%s act=%s elapsed=%.2f/%.2f aim(p=%.1f w=%.2f) openWalkT=%.2f/%.2f walkT=%.2f/%.2f runT=%.2f/%.2f runLoop=%d %.2f/%.2f",
            self.DebugSessionTime,
            moveName, self.MoveState,
            extMove, extAction,
            tostring(self.ActionName),
            self.ActionElapsed, self.ActionDuration,
            self.AimPitch, self.AimWeight,
            openingWalkTime, openingWalkLength,
            walkTime, walkLength,
            runTime, runLength,
            self.RunLoopIndex, self.RunLoopTimer, RUN_LOOP_WINDOW))
    end
end
