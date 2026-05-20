#pragma once

#include "Animation/AnimInstance.h"

#include <sol/sol.hpp>

class UAnimSequenceBase;

// Lua 로 AnimGraph 트리를 정의하는 AnimInstance. UCharacterAnimInstance 의 sibling — 같은
// AnimGraph 인프라 위에서 동작하지만 트리 build (SM 노드 / Slot / LayeredBlend 등) 와
// 게임 로직 (변수 갱신) 을 .lua 스크립트로 옮긴다.
//
// 라이프사이클:
//   NativeInitializeAnimation:
//     1) sol::environment 생성 → Anim 모듈 binding 설치
//     2) ScriptFile 의 .lua 코드 실행 (환경 안에 함수 정의됨)
//     3) init(self) 호출 — lua 가 Anim.create_* / Anim.set_root_node 로 RootNode 트리 build
//     4) RootNode 가 set 안 됐으면 (legacy graph 없음) safety fallback 없음 — lua 가 그래프 책임.
//     5) Auto wrap — Root 가 Slot 노드가 아니면 자동으로 DefaultSlot 으로 감쌈 (montage 진입점 보장).
//   NativeUpdateAnimation(dt):
//     update(self, dt) — lua 가 변수 갱신 (Speed 등). RootNode->Update 는 UAnimInstance 가 별도.
//   EvaluateAnimation/HandleAnimNotify:
//     UAnimInstance 의 RootNode 경로 / Notify dispatch 인프라 그대로 활용.
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

	// Editor 노출 — Content/Script/Anim 하위 .lua 파일 (FAssetRegistry "LuaAnimScript" 콤보).
	UPROPERTY(Edit, Save, Category="Animation|Lua", DisplayName="Script File", AssetType="LuaAnimScript")
	FString ScriptFile;

	// UAnimInstance:
	void NativeInitializeAnimation() override;
	void NativeUpdateAnimation(float DeltaSeconds) override;
	void HandleAnimNotify(const FAnimNotifyEvent& Notify) override;

	void PostEditProperty(const char* PropertyName) override;
	void Serialize(FArchive& Ar) override;

	// FLuaScriptManager 가 .lua 변경 감지 시 호출.
	void ReloadScript();

private:
	void ClearGraph();      // ReloadScript 경로 — RootNode + OwnedNodes + lua env 정리.
	void InstallBindings();
	void DispatchLuaInit();

	sol::environment              Env;
	sol::protected_function       LuaInit;
	sol::protected_function       LuaUpdate;
	sol::protected_function       LuaOnNotify;
	sol::table                    LuaSelf;        // self table — lua 가 변수 저장 (self.Speed 등)
};
