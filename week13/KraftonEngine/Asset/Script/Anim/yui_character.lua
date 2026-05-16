-- Phase B 데모 — UCharacterAnimInstance (C++) 의 mock 동작을 lua 로 재현.
-- 사용 방법:
--   1) SkeletalMeshActor 의 Animation Mode = Custom
--   2) Anim Instance Class = ULuaAnimInstance
--   3) Script File = "Anim/yui_character.lua" (Editor 콤보)
--
-- 기본은 mock 시퀀스 (sway/wave) — .uasset 없이도 즉시 동작 확인 가능.
-- 실제 import 한 시퀀스로 바꾸려면 USE_MOCK 을 false 로 두고 *_PATH 수정.
--
-- Hot-reload: 이 파일 저장만 해도 에디터에서 즉시 반영 (FLuaScriptManager 가 ReloadScript 호출).

local USE_MOCK  = true
local IDLE_PATH = "Content/Data/hirasawa-yui/yui_Idle.uasset"   -- 임포트 후 자기 경로로 수정
local WALK_PATH = "Content/Data/hirasawa-yui/yui_Walk.uasset"

function init(self)
    self.Speed          = 0
    self.t              = 0
    self.SpeedThreshold = 10.0
    self.AutoPeriodSec  = 6.0
    self.AutoSpeedAmp   = 15.0

    if USE_MOCK then
        -- Phase 3 mock 팩토리 재활용. sway=루트 본만 Z 축 흔들, wave=전 본 sinusoidal.
        Anim.register_mock_state("Idle", "sway", 1.5, 8.0,  1.0, true)
        Anim.register_mock_state("Walk", "wave", 0.8, 15.0, 1.0, true)
    else
        Anim.register_state("Idle", IDLE_PATH, 1.0, true)
        Anim.register_state("Walk", WALK_PATH, 1.0, true)
    end

    -- Condition 람다는 self 캡처 — self.Speed 가 update() 에서 갱신됨.
    Anim.register_transition("Idle", "Walk",
        function() return self.Speed >  self.SpeedThreshold end, 0.2)
    Anim.register_transition("Walk", "Idle",
        function() return self.Speed <= self.SpeedThreshold end, 0.2)

    Anim.set_initial_state("Idle")
end

function update(self, dt)
    -- Phase 5 의 UCharacterAnimInstance::NativeUpdateAnimation 과 동등 — 외부 입력 없이
    -- sin 으로 Speed 가 [0, 2*Amp] 사이를 AutoPeriodSec 주기로 변동. SpeedThreshold 를
    -- 주기적으로 넘나들며 Idle ↔ Walk 자동 전이.
    self.t = self.t + dt
    local omega = 2.0 * math.pi / self.AutoPeriodSec
    self.Speed = self.AutoSpeedAmp + self.AutoSpeedAmp * math.sin(self.t * omega)
end

function on_notify(self, name)
    print("[LuaAnim] notify: " .. name .. "  (t=" .. string.format("%.2f", self.t) .. "s)")
end
