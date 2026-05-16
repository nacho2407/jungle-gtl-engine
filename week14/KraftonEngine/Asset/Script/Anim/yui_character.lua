-- Phase B 데모 — UCharacterAnimInstance (C++) 의 mock 동작을 lua 로 재현.
-- 사용 방법:
--   1) SkeletalMeshActor 의 Animation Mode = Custom
--   2) Anim Instance Class = ULuaAnimInstance
--   3) Script File = "Anim/yui_character.lua" (Editor 콤보)
--
-- 시퀀스 .uasset 이 디스크에 있어야 정상 모션이 보입니다. 없으면 ref pose 가 보이고
-- Output 창에 "anim not found: ..." 경고. path 는 자기 환경에 맞게 수정하세요.
--
-- Hot-reload: 이 파일 저장만 해도 에디터에서 즉시 반영 (FLuaScriptManager 가 ReloadScript 호출).

local IDLE_PATH = "Content/Data/hirasawa-yui/yui_Idle.uasset"
local WALK_PATH = "Content/Data/hirasawa-yui/yui_Walk.uasset"

function init(self)
    self.Speed          = 0
    self.t              = 0
    self.SpeedThreshold = 10.0
    self.AutoPeriodSec  = 6.0
    self.AutoSpeedAmp   = 15.0

    Anim.register_state("Idle", IDLE_PATH, 1.0, true)
    Anim.register_state("Walk", WALK_PATH, 1.0, true)

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
