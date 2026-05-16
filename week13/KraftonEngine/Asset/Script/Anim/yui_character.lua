-- Phase B+C 데모 — UCharacterAnimInstance (C++) 의 mock 동작을 lua 로 재현 + Phase C 의
-- CharacterMovement 와 연결. ACharacter 위에 박힌 ULuaAnimInstance 가 이 스크립트 실행.
--
-- 사용 방법:
--   1) ACharacter (또는 ALuaCharacter) 의 SkeletalMesh 컴포넌트 선택
--   2) Animation Mode = Custom
--   3) Anim Instance Class = ULuaAnimInstance
--   4) Script File = "Anim/yui_character.lua" (Editor 콤보)
--
-- 시뮬레이션 모드 (DRIVE_MODE):
--   "movement" — Anim.get_owner_speed() 로 실제 CharacterMovement 속도 읽음 (WASD 로 이동 시 자동 전이).
--   "auto"     — sin 으로 Speed 자동 변동 (이동 없이 데모용. CharacterMovement 없는 SkeletalMeshActor 에서도 동작).
--
-- 시퀀스 모드 (USE_MOCK):
--   true  — mock sway/wave 시퀀스 (.uasset 없이 즉시 동작).
--   false — *_PATH 의 실제 시퀀스 (먼저 FBX import 필요).
--
-- Hot-reload: 이 파일 저장만 해도 에디터에서 즉시 반영.

local USE_MOCK   = true
local DRIVE_MODE = "movement"   -- "movement" 또는 "auto"

local IDLE_PATH = "Content/Data/hirasawa-yui/yui_Idle.uasset"
local WALK_PATH = "Content/Data/hirasawa-yui/yui_Walk.uasset"

function init(self)
    self.Speed          = 0
    self.t              = 0
    self.SpeedThreshold = 0.5    -- m/s — movement 모드는 단위가 작음 (MaxWalkSpeed=6)
    self.AutoPeriodSec  = 6.0
    self.AutoSpeedAmp   = 15.0   -- auto 모드 전용 — Threshold 와 단위 무관 (legacy)

    if USE_MOCK then
        Anim.register_mock_state("Idle", "sway", 1.5, 8.0,  1.0, true)
        Anim.register_mock_state("Walk", "wave", 0.8, 15.0, 1.0, true)
        -- Jump 상태 — 1회 재생 (Loop=false). Falling 상태와 함께 활성.
        Anim.register_mock_state("Jump", "wave", 0.5, 30.0, 1.0, false)
    else
        Anim.register_state("Idle", IDLE_PATH, 1.0, true)
        Anim.register_state("Walk", WALK_PATH, 1.0, true)
        -- 실제 jump anim 있으면 path 지정. 없으면 mock 으로 fallback.
        Anim.register_state("Jump", WALK_PATH, 1.0, false)
    end

    Anim.register_transition("Idle", "Walk",
        function() return self.Speed >  self.SpeedThreshold end, 0.2)
    Anim.register_transition("Walk", "Idle",
        function() return self.Speed <= self.SpeedThreshold end, 0.2)

    -- AnyState → Jump — Falling 시작하는 순간 (지상 이탈 + 점프 둘 다 포함). 빠른 blend.
    Anim.register_transition("AnyState", "Jump",
        function() return Anim.is_owner_falling() end, 0.1)
    -- Jump → Idle — 착지 (Walking 복귀). 다음 frame 의 Idle↔Walk 전이가 자연스럽게 잡음.
    Anim.register_transition("Jump", "Idle",
        function() return not Anim.is_owner_falling() end, 0.2)

    Anim.set_initial_state("Idle")
end

function update(self, dt)
    if DRIVE_MODE == "movement" then
        -- 실제 CharacterMovement 의 속도. WASD 입력이 있으면 > 0, 없으면 0 (또는 braking 중 잔여).
        self.Speed = Anim.get_owner_speed()
    else
        -- auto 모드 — sin 으로 자동 변동. CharacterMovement 없는 환경에서도 데모 동작.
        self.t = self.t + dt
        local omega = 2.0 * math.pi / self.AutoPeriodSec
        self.Speed = self.AutoSpeedAmp + self.AutoSpeedAmp * math.sin(self.t * omega)
    end
end

function on_notify(self, name)
    print("[LuaAnim] notify: " .. name .. "  (Speed=" .. string.format("%.2f", self.Speed) .. ")")
end
