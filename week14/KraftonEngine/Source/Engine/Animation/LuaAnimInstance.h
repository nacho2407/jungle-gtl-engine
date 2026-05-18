#pragma once

#include "Animation/AnimInstance.h"

#include <sol/sol.hpp>

class UAnimationStateMachine;
class UAnimSequenceBase;

// Lua 로 상태/전이를 정의하는 AnimInstance. UCharacterAnimInstance 의 sibling — 같은 FSM 인프라
// 위에서 동작하지만 게임 로직 (상태/전이/Speed 등 변수) 을 .lua 스크립트로 옮긴다.
//
// 라이프사이클:
//   NativeInitializeAnimation:
//     1) sol::environment 생성 → Anim 모듈 binding 설치
//     2) ScriptFile 의 .lua 코드 실행 (환경 안에 함수 정의됨)
//     3) init(self) 호출 — lua 가 Anim.register_state/register_transition/set_initial_state 수행
//   NativeUpdateAnimation(dt):
//     update(self, dt) → FSM->Tick
//   EvaluateAnimation:
//     FSM->Evaluate (기존 FSM 코드 위임)
//   HandleAnimNotify(N):
//     on_notify(self, N.NotifyName) 호출
//
// 변수 모델: lua self table 단독 — self.Speed = 10 등 모두 lua 안에서. C++ 는 binding 만.
// Hot-reload: FLuaScriptManager 에 등록되어 .lua 변경 시 ReloadScript 호출됨.

#include "Source/Engine/Animation/LuaAnimInstance.generated.h"

UCLASS()
class ULuaAnimInstance : public UAnimInstance
{
public:
	GENERATED_BODY()
	ULuaAnimInstance() = default;
	~ULuaAnimInstance() override;

	// Editor 노출 — Asset/Script/Anim 하위 .lua 파일 (FAssetRegistry "LuaAnimScript" 콤보).
	UPROPERTY(Edit, Save, Category="Animation|Lua", DisplayName="Script File", AssetType="LuaAnimScript")
	FString ScriptFile;

	// UAnimInstance:
	void NativeInitializeAnimation() override;
	void NativeUpdateAnimation(float DeltaSeconds) override;
	void EvaluateAnimation(FPoseContext& Output) override;
	void HandleAnimNotify(const FAnimNotifyEvent& Notify) override;

	void PostEditProperty(const char* PropertyName) override;
	void Serialize(FArchive& Ar) override;

	// FLuaScriptManager 가 .lua 변경 감지 시 호출.
	void ReloadScript();

	UAnimationStateMachine* GetFSM() const { return FSM; }

	// ── Lua → C++ 진입점 (Anim 모듈에서 노출) ──
	void Lua_RegisterState(const std::string& Name, const std::string& AnimPath, float Rate, bool Loop);
	void Lua_RegisterMockState(const std::string& Name, const std::string& MockType,
	                           float Duration, float AmplitudeDeg, float Rate, bool Loop);
	void Lua_RegisterTransition(const std::string& From, const std::string& To,
	                            sol::protected_function Cond, float BlendTime);
	void Lua_SetInitialState(const std::string& Name);
	void Lua_RequestTransition(const std::string& Name, float BlendDuration);
	std::string Lua_GetCurrentState() const;

private:
	void ClearFSM();
	void InstallBindings();
	void DispatchLuaInit();

	sol::environment              Env;
	sol::protected_function       LuaInit;
	sol::protected_function       LuaUpdate;
	sol::protected_function       LuaOnNotify;
	sol::table                    LuaSelf;        // self table — lua 가 변수 저장 (self.Speed 등)

	UAnimationStateMachine*       FSM = nullptr;
};
