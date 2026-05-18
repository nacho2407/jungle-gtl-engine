#pragma once

#include "Core/CoreTypes.h"
#include "Object/ObjectPtr.h"
#include "Object/ObjectMacros.h"
#include "Object/SoftObjectPtr.h"
#include "Object/UStruct.h"

#include "Source/Engine/Animation/AnimationMode.generated.h"

class UAnimSequenceBase;

// USkinnedMeshComponent 가 AnimInstance 를 어떤 방식으로 관리할지 결정한다.
// UE 의 EAnimationMode 와 의미 동일 — Blueprint 자리가 우리 환경엔 없으므로
// AnimationCustom 으로 의역 (= 사용자가 UAnimInstance 자식 클래스를 지정).
UENUM()
enum class EAnimationMode : uint8
{
	None,                  // 애니메이션 없음. 컴포넌트의 BoneEdit 만 동작.
	AnimationSingleNode,   // 시퀀스 1개 재생. AnimationData 의 AnimToPlay 사용.
	AnimationCustom,       // AnimInstanceClass 로 지정한 UAnimInstance 자식 인스턴스화 (FSM 등).
};

// EditorPropertyWidget Enum 콤보용 표시 이름. EAnimationMode 와 1:1 순서.
inline const char* GAnimationModeNames[] = {
	"None",
	"AnimationSingleNode",
	"AnimationCustom",
};
inline constexpr uint32 GAnimationModeCount = sizeof(GAnimationModeNames) / sizeof(GAnimationModeNames[0]);

// SingleNode 모드에서 직렬화/에디터 노출용으로 묶어 두는 재생 파라미터.
// AnimToPlay 는 런타임 포인터, AnimToPlayPath 는 직렬화/에디터용 식별자 (= asset 경로).
// SetAnimation 등 설정 경로는 두 멤버를 항상 동기화한다.
USTRUCT()
struct FSingleAnimationPlayData
{
	GENERATED_BODY()

	TObjectPtr<UAnimSequenceBase> AnimToPlay;

	UPROPERTY(Edit, Save, Category="Animation", DisplayName="Anim To Play", AssetType="UAnimSequence")
	FSoftObjectPtr     AnimToPlayPath = "None";
	UPROPERTY(Edit, Save, Category="Animation", DisplayName="Play Rate", Min=-4.0f, Max=4.0f, Speed=0.05f)
	float              PlayRate       = 1.0f;
	UPROPERTY(Edit, Save, Category="Animation", DisplayName="Looping")
	bool               bLooping       = true;
	UPROPERTY(Edit, Save, Category="Animation", DisplayName="Playing")
	bool               bPlaying       = true;
};
