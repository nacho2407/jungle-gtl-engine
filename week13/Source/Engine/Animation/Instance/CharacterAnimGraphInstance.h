#pragma once

#include "Animation/Graph/AnimGraphInstance.h"

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

#include "Source/Engine/Animation/Instance/CharacterAnimGraphInstance.generated.h"

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
	// Owner UCharacterMovementComponent::GetSpeed (Velocity.Length()) — yui_character.lua 의
	// self.Speed = Anim.get_owner_speed() 와 동등.
	UPROPERTY(Edit, Category="Animation|Character", DisplayName="Speed", Min=0.0f, Max=100.0f, Speed=0.5f)
	float Speed = 0.0f;

	// Owner UCharacterMovementComponent::IsFalling — yui_character.lua 의
	// Anim.is_owner_falling() 와 동등. Top-SM 의 Locomotion↔Jump 전이 condition 용.
	// (V1 transition rule 은 float 비교만 지원 — bool 도 MakeFloatReader 가 0/1 cast 하므로
	// "bIsFalling > 0.5" 또는 "bIsFalling == 1" 식으로 비교.)
	UPROPERTY(Edit, Category="Animation|Character", DisplayName="Is Falling")
	bool bIsFalling = false;
};
