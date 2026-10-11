-- Phase 2.6 — Sub-state-machine + LayeredBlendPerBone + UpperBody slot 데모.
--
--   Root = LayeredBlendPerBone(
--            BasePose  = DefaultSlot ← TopSM (Locomotion ↔ Jump, Locomotion 안에 Idle/Walk sub-SM)
--            BlendPose = UpperBodySlot ← RefPose
--            Mask = Spine 본 트리)
--
-- DefaultSlot 의 montage 는 풀바디 (UE 의 DefaultSlot 컨벤션). UpperBodySlot 의 montage 는
-- mask 영역 (상반신) 만. UpperBodySlot 의 montage 가 없을 땐 Slot.GetEffectiveBlendWeight 가
-- 0 이라 base 100% — RefPose 가 영향 안 줌.
--
-- 사용 방법:
--   1) ACharacter 의 SkeletalMesh 컴포넌트 선택
--   2) Animation Mode = Custom + Anim Instance Class = ULuaAnimInstance
--   3) Script File = "Anim/yui_character.lua"
--
-- 좌클릭 = 풀바디 attack (DefaultSlot, 기존 동작)
-- 우클릭 = 상반신만 attack (UpperBodySlot, 하반신 locomotion 유지)
--
-- Hot-reload: 이 파일 저장만 해도 에디터에서 즉시 반영.

local IDLE_PATH = "Content/Data/hirasawa-yui/Sword And Shield Idle_mixamo_com.uasset"
local WALK_PATH = "Content/Data/hirasawa-yui/Walking_mixamo_com.uasset"
--local JUMP_PATH = "Content/Data/hirasawa-yui/Jump_mixamo_com.uasset"
local JUMP_PATH = "Content/Data/hirasawa-yui/Falling Idle_mixamo_com.uasset"
local BENCAO_PATH = "Content/Data/hirasawa-yui/bencao_mixamo_com.uasset"
local ESQUIVA_1_PATH = "Content/Data/hirasawa-yui/esquiva 1_mixamo_com.uasset"

local DEFAULT_SLOT = "DefaultSlot"
local UPPER_BODY_SLOT = "UpperBody"
local ACTION_PLAY_RATE = 1.5
local KEY_1 = string.byte("1")
local KEY_2 = string.byte("2")
local KEY_3 = string.byte("3")
local KEY_Z = string.byte("Z")
local KEY_X = string.byte("X")
local KEY_C = string.byte("C")
local BENCAO_DURATION = 1.366667 / ACTION_PLAY_RATE
local BENCAO_BLEND_IN = 0.35
local BENCAO_BLEND_OUT = 0.35
local ESQUIVA_1_DURATION = 4.066667 / ACTION_PLAY_RATE
local ESQUIVA_1_BLEND_IN = 0.35
local ESQUIVA_1_BLEND_OUT = 0.35

local COMBAT_MODE_SWORD = "Sword"
local COMBAT_MODE_RAYGUN = "RayGun"
local COMBAT_MODE_KI = "Ki"

local COMBAT_MODES = {
    Sword = {
        idle_sequence = IDLE_PATH,
        walk_sequence = WALK_PATH,
        attack_montage = "Content/Montages/One Hand Sword Combo_mixamo_com_Montage.uasset",
        play_rate = ACTION_PLAY_RATE,
    },
    RayGun = {
        idle_sequence = "Content/Data/hirasawa-yui/Pistol Idle_mixamo_com.uasset",
        walk_sequence = WALK_PATH,
        attack_montage = "Content/Montages/Pistol Idle_mixamo_com_Montage.uasset",
        play_rate = 1.0,
    },
    Ki = {
        idle_sequence = "Content/Data/hirasawa-yui/Standing 1H Magic Attack 03_mixamo_com.uasset",
        walk_sequence = WALK_PATH,
        attack_montage = "Content/Montages/AM_MagicAttack.uasset",
        play_rate = ACTION_PLAY_RATE,
    },
}

local MODE_ORDER = {
    COMBAT_MODE_SWORD,
    COMBAT_MODE_RAYGUN,
    COMBAT_MODE_KI,
}

local MODE_KEYS = {
    { key = KEY_1, mode = COMBAT_MODE_SWORD, attack = true, slot = nil },
    { key = KEY_2, mode = COMBAT_MODE_RAYGUN, attack = true, slot = nil },
    { key = KEY_3, mode = COMBAT_MODE_KI, attack = true, slot = nil },
}

local ATTACK_KEYS = {
    { key = KEY_Z, slot = nil },
}

-- UpperBody mask 의 root 본 — Spine 부터 자손 (팔/머리/손) 까지 자동 mask BFS.
-- mixamo rig 의 본 이름은 보통 "mixamorig:Spine" 류. 본 못 찾으면 mask 가 전부 false → base 100%.
local UPPER_BODY_ROOT_BONE = "Bip001 Spine"

local function set_combat_mode(self, mode)
    if COMBAT_MODES[mode] == nil then
        return
    end

    self.CombatMode = mode
    _G.YuiCombatMode = mode
    for index, mode_name in ipairs(MODE_ORDER) do
        if mode_name == mode then
            self.CombatModeIndex = index - 1
            break
        end
    end
    print("[Yui] CombatMode = " .. mode)
end

local function get_current_combat_mode(self)
    return COMBAT_MODES[self.CombatMode] or COMBAT_MODES[COMBAT_MODE_SWORD]
end

local function play_current_mode_attack(self, slot)
    if Anim.is_owner_falling() or Anim.is_montage_playing(DEFAULT_SLOT) then
        return
    end

    local mode = get_current_combat_mode(self)
    Anim.play_montage(mode.attack_montage, mode.section, mode.play_rate, mode.blend_in, slot)
end

local function create_mode_blend_pose(sequence_key, fallback_path)
    local blend = Anim.create_blend_list_by_enum(0, 0.2)
    for _, mode_name in ipairs(MODE_ORDER) do
        local mode = COMBAT_MODES[mode_name]
        local path = mode[sequence_key] or fallback_path
        Anim.blend_list_add_pose(blend, Anim.create_sequence_player(path, 1.0, true))
    end
    return blend
end

function init(self)
    self.Speed          = 0
    self.SpeedThreshold = 0.5
    self.CombatMode = COMBAT_MODE_SWORD
    _G.YuiCombatMode = self.CombatMode
    _G.YuiRaygunFiring = false
    self.CombatModeIndex = 0
    self.BencaoRequested = false
    self.BencaoPlaying = false
    self.BencaoElapsed = 0.0
    self.Esquiva1Requested = false
    self.Esquiva1Playing = false
    self.Esquiva1Elapsed = 0.0

    -- ── Locomotion sub-SM (Idle ↔ Walk) ──
    local loco = Anim.create_state_machine("Locomotion")
    self.IdleByCombatMode = create_mode_blend_pose("idle_sequence", IDLE_PATH)
    self.WalkByCombatMode = create_mode_blend_pose("walk_sequence", WALK_PATH)
    Anim.sm_add_state(loco, "Idle", self.IdleByCombatMode)
    Anim.sm_add_state(loco, "Walk", self.WalkByCombatMode)
    Anim.sm_add_transition(loco, "Idle", "Walk",
        function() return self.Speed >  self.SpeedThreshold end, 0.2)
    Anim.sm_add_transition(loco, "Walk", "Idle",
        function() return self.Speed <= self.SpeedThreshold end, 0.2)
    Anim.sm_set_initial_state(loco, "Idle")

    -- ── Top SM (Locomotion ↔ Jump) ──
    local top = Anim.create_state_machine("Top")
    Anim.sm_add_state(top, "Locomotion", loco)
    Anim.sm_add_state(top, "Jump", Anim.create_sequence_player(JUMP_PATH, 1.0, false))
    Anim.sm_add_state(top, "Bencao", Anim.create_sequence_player(BENCAO_PATH, ACTION_PLAY_RATE, false))
    Anim.sm_add_state(top, "Esquiva1", Anim.create_sequence_player(ESQUIVA_1_PATH, ACTION_PLAY_RATE, false))
    Anim.sm_add_transition(top, "AnyState", "Jump",
        function()
            if Anim.is_owner_falling() then
                self.BencaoRequested = false
                self.BencaoPlaying = false
                self.Esquiva1Requested = false
                self.Esquiva1Playing = false
                return true
            end
            return false
        end, 0.1)
    Anim.sm_add_transition(top, "AnyState", "Bencao",
        function()
            if self.BencaoRequested
                and not Anim.is_owner_falling()
                and not Anim.is_montage_playing(DEFAULT_SLOT)
                and not self.Esquiva1Playing then
                self.BencaoRequested = false
                self.BencaoPlaying = true
                self.BencaoElapsed = 0.0
                return true
            end
            return false
        end, BENCAO_BLEND_IN)
    Anim.sm_add_transition(top, "AnyState", "Esquiva1",
        function()
            if self.Esquiva1Requested
                and not Anim.is_owner_falling()
                and not Anim.is_montage_playing(DEFAULT_SLOT)
                and not self.BencaoPlaying then
                self.Esquiva1Requested = false
                self.Esquiva1Playing = true
                self.Esquiva1Elapsed = 0.0
                return true
            end
            return false
        end, ESQUIVA_1_BLEND_IN)
    Anim.sm_add_transition(top, "Jump", "Locomotion",
        function() return not Anim.is_owner_falling() end, 0.5)
    Anim.sm_add_transition(top, "Bencao", "Locomotion",
        function()
            if self.BencaoPlaying and self.BencaoElapsed >= BENCAO_DURATION then
                self.BencaoPlaying = false
                return true
            end
            return false
        end, BENCAO_BLEND_OUT)
    Anim.sm_add_transition(top, "Esquiva1", "Locomotion",
        function()
            if self.Esquiva1Playing and self.Esquiva1Elapsed >= ESQUIVA_1_DURATION then
                self.Esquiva1Playing = false
                return true
            end
            return false
        end, ESQUIVA_1_BLEND_OUT)
    Anim.sm_set_initial_state(top, "Locomotion")

    -- ── DefaultSlot — 풀바디 montage 진입점. InputPose = TopSM ──
    local default_slot = Anim.create_slot(DEFAULT_SLOT, top)

    -- ── UpperBodySlot — 상반신 montage 진입점. InputPose = RefPose ──
    -- montage 없을 땐 Slot.GetEffectiveBlendWeight 가 0 이라 base 만 보임.
    local upper_slot = Anim.create_slot(UPPER_BODY_SLOT, Anim.create_ref_pose())

    -- ── LayeredBlend — Spine 본 트리만 upper_slot 적용. 하반신은 default_slot (loco/jump) ──
    local layer = Anim.create_layered_blend_per_bone(default_slot, upper_slot, UPPER_BODY_ROOT_BONE)

    Anim.set_root_node(layer)
end

function update(self, dt)
    self.Speed = Anim.get_owner_speed()
    Anim.blend_list_set_active(self.IdleByCombatMode, self.CombatModeIndex)
    Anim.blend_list_set_active(self.WalkByCombatMode, self.CombatModeIndex)
    _G.YuiRaygunFiring = self.CombatMode == COMBAT_MODE_RAYGUN and Anim.is_left_mouse_down()

    if self.BencaoPlaying then
        self.BencaoElapsed = self.BencaoElapsed + dt
    end
    if self.Esquiva1Playing then
        self.Esquiva1Elapsed = self.Esquiva1Elapsed + dt
    end

    if Anim.is_key_pressed(KEY_X) and not self.BencaoPlaying and not Anim.is_owner_falling() then
        self.BencaoRequested = true
    end
    if Anim.is_key_pressed(KEY_C) and not self.Esquiva1Playing and not Anim.is_owner_falling() then
        self.Esquiva1Requested = true
    end

    for _, binding in ipairs(MODE_KEYS) do
        if Anim.is_key_pressed(binding.key) then
            set_combat_mode(self, binding.mode)
            if binding.attack then
                play_current_mode_attack(self, binding.slot)
            end
        end
    end

    for _, binding in ipairs(ATTACK_KEYS) do
        if Anim.is_key_pressed(binding.key) then
            play_current_mode_attack(self, binding.slot)
        end
    end

    -- 좌클릭 → 풀바디 attack (DefaultSlot 기본).
    if Anim.is_left_mouse_pressed() then
        play_current_mode_attack(self, nil)
    end

    -- 우클릭 → 상반신만 attack (UpperBody slot). 하반신은 locomotion 유지.
    if Anim.is_right_mouse_pressed() then
        play_current_mode_attack(self, UPPER_BODY_SLOT)
    end
end

function on_notify(self, name)
    print("[LuaAnim] notify: " .. name .. "  (Speed=" .. string.format("%.2f", self.Speed) .. ")")
end
