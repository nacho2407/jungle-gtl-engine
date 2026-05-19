-- Phase 1.6b — Sub-state-machine 데모.
-- UE Third Person Template 의 Locomotion + Jump 구조를 모방.
--
--   RootNode = Top SM (Locomotion ↔ Jump)
--     ├─ State "Locomotion"  → sub-SM
--     │       ├─ State "Idle" → Idle Sequence
--     │       └─ State "Walk" → Walk Sequence
--     │       (Speed 임계값으로 Idle ↔ Walk 자체 전이 — Jump 끝나도 Walk/Idle 적절히 복귀)
--     └─ State "Jump"        → Jump Sequence (loop=false)
--
-- 사용 방법:
--   1) ACharacter 의 SkeletalMesh 컴포넌트 선택
--   2) Animation Mode = Custom
--   3) Anim Instance Class = ULuaAnimInstance
--   4) Script File = "Anim/yui_character.lua" (Editor 콤보)
--
-- 좌클릭 → attack montage 재생.
-- Hot-reload: 이 파일 저장만 해도 에디터에서 즉시 반영.

local IDLE_PATH = "Content/Data/hirasawa-yui/IdleWithSkin_mixamo_com.uasset"
local WALK_PATH = "Content/Data/hirasawa-yui/Walking_mixamo_com.uasset"
local JUMP_PATH = "Content/Data/hirasawa-yui/Jump_mixamo_com.uasset"

local ATTACK_MONTAGE_PATH = "Content/Montages/mixamo_com_Montage.uasset"

function init(self)
    self.Speed          = 0
    self.SpeedThreshold = 0.5    -- m/s — MaxWalkSpeed=6 환경에서 작은 임계

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
    -- Locomotion state 의 sub-graph 가 위에서 만든 loco SM — sub-state-machine 핵심.
    Anim.sm_add_state(top, "Locomotion", loco)
    Anim.sm_add_state(top, "Jump", Anim.create_sequence_player(JUMP_PATH, 1.0, false))

    -- AnyState → Jump — Falling 시작하는 순간 (지상 이탈 + 점프 둘 다 포함). 빠른 blend.
    Anim.sm_add_transition(top, "AnyState", "Jump",
        function() return Anim.is_owner_falling() end, 0.1)
    -- Jump → Locomotion — 착지. 복귀 시 sub-SM 의 마지막 active state (Idle 또는 Walk) 유지.
    --   = "Jump 끝났는데 1 frame Idle 거쳐 Walk 로 끊기는" 패턴이 sub-SM 으로 자연 해결.
    Anim.sm_add_transition(top, "Jump", "Locomotion",
        function() return not Anim.is_owner_falling() end, 0.5)

    Anim.sm_set_initial_state(top, "Locomotion")

    -- 트리 root 박기 — RootNode 가 set 되면 NativeInitializeAnimation 의 wrapper FSM fallback
    -- 은 건너뜀 (legacy register_state 같이 안 씀).
    Anim.set_root_node(top)
end

function update(self, dt)
    -- 실제 CharacterMovement 속도 — WASD 입력에 따른 즉시 반응.
    self.Speed = Anim.get_owner_speed()

    -- 좌클릭 → attack montage. 이미 재생 중이어도 PlayMontage 가 자연스럽게 재시작 (BlendIn).
    if Anim.is_left_mouse_pressed() then
        Anim.play_montage(ATTACK_MONTAGE_PATH)
    end
end

function on_notify(self, name)
    print("[LuaAnim] notify: " .. name .. "  (Speed=" .. string.format("%.2f", self.Speed) .. ")")
end
