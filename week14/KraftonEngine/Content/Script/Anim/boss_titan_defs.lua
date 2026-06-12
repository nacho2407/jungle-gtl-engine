local M = {}

M.ANIM_ROOT = "Content/Data/Boss/Heavy/Animations/"

M.ANIM = {
    idle = M.ANIM_ROOT .. "a_IDLE_combat_deadbolt_titan_torso_d_lod0.uasset",
    walk = M.ANIM_ROOT .. "a_combat_walk_F_deadbolt_titan_torso_d_lod0.uasset",
    run = M.ANIM_ROOT .. "a_combat_run_F_deadbolt_titan_torso_d_lod0.uasset",
    strafeLeft = M.ANIM_ROOT .. "a_combat_walk_L_deadbolt_titan_torso_d_lod0.uasset",
    strafeRight = M.ANIM_ROOT .. "a_combat_walk_R_deadbolt_titan_torso_d_lod0.uasset",
    cannon = M.ANIM_ROOT .. "a_Fire_auto_deadbolt_titan_torso_d_lod0.uasset",
    powerShot = M.ANIM_ROOT .. "htLegion_MP_Stand_PowerShot_deadbolt_titan_torso_d_lod0.uasset",
    melee = M.ANIM_ROOT .. "at_elite_melee_low_stomp_F_deadbolt_titan_torso_d_lod0.uasset",
    retreat = M.ANIM_ROOT .. "a_bound_back_deadbolt_titan_torso_d_lod0.uasset",
    leapStart = M.ANIM_ROOT .. "a_MP_Jump_start_deadbolt_titan_torso_d_lod0.uasset",
    leapFloat = M.ANIM_ROOT .. "a_MP_Jump_float_deadbolt_titan_torso_d_lod0.uasset",
    leapLand = M.ANIM_ROOT .. "a_traverse_land_A_deadbolt_titan_torso_d_lod0.uasset",
    phase = M.ANIM_ROOT .. "a_Legion_gunup_deadbolt_titan_torso_d_lod0.uasset",
    crouchStand = M.ANIM_ROOT .. "a_crouch_2_stand_deadbolt_titan_torso_d_lod0.uasset",
    hitReact = M.ANIM_ROOT .. "at_combat_start_react_deadbolt_titan_torso_d_lod0.uasset",
}

M.MOVE = {
    IDLE = 0,
    WALK = 1,
    RUN = 2,
    STRAFE_LEFT = 3,
    STRAFE_RIGHT = 4,
    RETREAT = 5,
    LEAP_FLOAT = 6,
}

M.MOVE_MAX = M.MOVE.LEAP_FLOAT

M.MOVE_NAMES = {
    [M.MOVE.IDLE] = "IDLE",
    [M.MOVE.WALK] = "WALK",
    [M.MOVE.RUN] = "RUN",
    [M.MOVE.STRAFE_LEFT] = "STRAFE_L",
    [M.MOVE.STRAFE_RIGHT] = "STRAFE_R",
    [M.MOVE.RETREAT] = "RETREAT",
    [M.MOVE.LEAP_FLOAT] = "LEAP_FLOAT",
}

M.MOVE_BY_ANIM_PATH = {
    [M.ANIM.idle] = M.MOVE.IDLE,
    [M.ANIM.walk] = M.MOVE.WALK,
    [M.ANIM.run] = M.MOVE.RUN,
    [M.ANIM.strafeLeft] = M.MOVE.STRAFE_LEFT,
    [M.ANIM.strafeRight] = M.MOVE.STRAFE_RIGHT,
    [M.ANIM.retreat] = M.MOVE.RETREAT,
    [M.ANIM.leapFloat] = M.MOVE.LEAP_FLOAT,
}

M.ACTION_BY_ANIM_PATH = {
    [M.ANIM.cannon] = "cannon",
    [M.ANIM.powerShot] = "powerShot",
    [M.ANIM.melee] = "melee",
    [M.ANIM.retreat] = "retreat",
    [M.ANIM.leapStart] = "leapStart",
    [M.ANIM.leapLand] = "leapLand",
    [M.ANIM.phase] = "phase",
    [M.ANIM.hitReact] = "hitReact",
}

M.ACTION_DURATIONS = {
    openingWalk = 999.0,
    cannon = 0.85,
    powerShot = 1.25,
    melee = 3.8,
    retreat = 0.18,
    leapStart = 0.2,
    leapLand = 0.22,
    phase = 0.35,
    phaseCrouchDown = 0.35,
    phaseCrouchHold = 0.35,
    phaseStandUp = 0.35,
    hitReact = 1.4,
}

M.ANIM_NAMES = {}
for name, path in pairs(M.ANIM) do
    M.ANIM_NAMES[path] = name
end

return M
