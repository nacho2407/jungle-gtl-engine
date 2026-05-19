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

local IDLE_PATH = "Content/Data/hirasawa-yui/IdleWithSkin_mixamo_com.uasset"
local WALK_PATH = "Content/Data/hirasawa-yui/Walking_mixamo_com.uasset"
local JUMP_PATH = "Content/Data/hirasawa-yui/Jump_mixamo_com.uasset"

local ATTACK_MONTAGE_PATH = "Content/Montages/AM_MagicAttack.uasset"

-- UpperBody mask 의 root 본 — Spine 부터 자손 (팔/머리/손) 까지 자동 mask BFS.
-- mixamo rig 의 본 이름은 보통 "mixamorig:Spine" 류. 본 못 찾으면 mask 가 전부 false → base 100%.
local UPPER_BODY_ROOT_BONE = "Bip001 Spine"

function init(self)
    self.Speed          = 0
    self.SpeedThreshold = 0.5

    -- ── Locomotion sub-SM (Idle ↔ Walk) ──
    local loco = Anim.create_state_machine("Locomotion")
    Anim.sm_add_state(loco, "Idle", Anim.create_sequence_player(IDLE_PATH, 1.0, true))
    Anim.sm_add_state(loco, "Walk", Anim.create_sequence_player(WALK_PATH, 1.0, true))
    Anim.sm_add_transition(loco, "Idle", "Walk",
        function() return self.Speed >  self.SpeedThreshold end, 0.2)
    Anim.sm_add_transition(loco, "Walk", "Idle",
        function() return self.Speed <= self.SpeedThreshold end, 0.2)
    Anim.sm_set_initial_state(loco, "Idle")

    -- ── Top SM (Locomotion ↔ Jump) ──
    local top = Anim.create_state_machine("Top")
    Anim.sm_add_state(top, "Locomotion", loco)
    Anim.sm_add_state(top, "Jump", Anim.create_sequence_player(JUMP_PATH, 1.0, false))
    Anim.sm_add_transition(top, "AnyState", "Jump",
        function() return Anim.is_owner_falling() end, 0.1)
    Anim.sm_add_transition(top, "Jump", "Locomotion",
        function() return not Anim.is_owner_falling() end, 0.5)
    Anim.sm_set_initial_state(top, "Locomotion")

    -- ── DefaultSlot — 풀바디 montage 진입점. InputPose = TopSM ──
    local default_slot = Anim.create_slot("DefaultSlot", top)

    -- ── UpperBodySlot — 상반신 montage 진입점. InputPose = RefPose ──
    -- montage 없을 땐 Slot.GetEffectiveBlendWeight 가 0 이라 base 만 보임.
    local upper_slot = Anim.create_slot("UpperBody", Anim.create_ref_pose())

    -- ── LayeredBlend — Spine 본 트리만 upper_slot 적용. 하반신은 default_slot (loco/jump) ──
    local layer = Anim.create_layered_blend_per_bone(default_slot, upper_slot, UPPER_BODY_ROOT_BONE)

    Anim.set_root_node(layer)
end

function update(self, dt)
    self.Speed = Anim.get_owner_speed()

    -- 좌클릭 → 풀바디 attack (DefaultSlot 기본).
    if Anim.is_left_mouse_pressed() then
        Anim.play_montage(ATTACK_MONTAGE_PATH)
    end

    -- 우클릭 → 상반신만 attack (UpperBody slot). 하반신은 locomotion 유지.
    if Anim.is_right_mouse_pressed() then
        Anim.play_montage(ATTACK_MONTAGE_PATH, nil, nil, nil, "UpperBody")
    end
end

function on_notify(self, name)
    print("[LuaAnim] notify: " .. name .. "  (Speed=" .. string.format("%.2f", self.Speed) .. ")")
end
