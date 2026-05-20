#pragma once

#include "Animation/AnimGraphInstance.h"

// UAnimGraphInstance + UCharacterAnimInstance 데모 변수의 합성.
//
// 그래프가 가정하는 OwnerClass 와 실제 박힌 AnimInstance 클래스가 같아야 변수 reflection 이
// 동작한다. UCharacterAnimInstance 는 UAnimInstance 직접 상속이라 그래프 흐름과 호환 X —
// 그래서 이 클래스를 별도로 둔다.
//
// 사용:
//   1) SkeletalMeshComponent.AnimInstanceClass = UCharacterAnimGraphInstance
//   2) GraphAssetPath 에 편집한 그래프 자산 path 박음
//   3) 그래프의 OwnerClass = "UCharacterAnimGraphInstance" 선택
//   4) VariableGet → BlendListByEnum.Selector 에 Speed 연결
//   5) 자동 변동되는 Speed (sin) 가 매 frame BlendListByEnum 의 ActiveChildIndex 갱신
//      → Idle ↔ Walk 자동 전환 (sequence 가 박혀 있다는 가정)

#include "Source/Engine/Animation/CharacterAnimGraphInstance.generated.h"

UCLASS()
class UCharacterAnimGraphInstance : public UAnimGraphInstance
{
public:
	GENERATED_BODY()
	UCharacterAnimGraphInstance() = default;
	~UCharacterAnimGraphInstance() override = default;

	void NativeUpdateAnimation(float DeltaSeconds) override;
	void Serialize(FArchive& Ar)                   override;

	// 외부 push 변수 — 그래프의 VariableGet 노드가 reflection 으로 읽음.
	UPROPERTY(Edit, Category="Animation|Character", DisplayName="Speed", Min=0.0f, Max=100.0f, Speed=0.5f)
	float Speed = 0.0f;

	// 자동 구동(데모). false 면 외부가 Speed 직접 갱신.
	UPROPERTY(Edit, Save, Category="Animation|Character", DisplayName="Auto Drive Speed")
	bool  bAutoDriveSpeed = true;
	UPROPERTY(Edit, Save, Category="Animation|Character", DisplayName="Auto Period (s)", Min=0.1f, Max=30.0f, Speed=0.1f)
	float AutoPeriodSec   = 10.0f;
	UPROPERTY(Edit, Save, Category="Animation|Character", DisplayName="Auto Speed Amp", Min=0.0f, Max=100.0f, Speed=0.5f)
	float AutoSpeedAmp    = 15.0f;

private:
	float ElapsedTime = 0.0f;
};
