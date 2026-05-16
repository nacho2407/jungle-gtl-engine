#include "CharacterAnimInstance.h"

#include "Animation/AnimationStateMachine.h"
#include "Animation/AnimState.h"
#include "Animation/AnimSequence.h"
#include "Animation/PoseContext.h"
#include "Core/PropertyTypes.h"
#include "Math/MathUtils.h"
#include "Mesh/SkeletalMesh.h"
#include "Mesh/SkeletalMeshAsset.h"

#include <cmath>

IMPLEMENT_CLASS(UCharacterAnimInstance, UAnimInstance)

namespace
{
	// UAnimState 인스턴스 한 개 만들고 필드 채워서 반환. Outer 는 보통 AnimInstance.
	UAnimState* MakeState(FName Name, UAnimSequenceBase* Sequence, float PlayRate, bool bLoop, UObject* Outer)
	{
		UAnimState* S = UObjectManager::Get().CreateObject<UAnimState>(Outer);
		S->StateName = Name;
		S->Sequence  = Sequence;
		S->PlayRate  = PlayRate;
		S->bLooping  = bLoop;
		return S;
	}
}

void UCharacterAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	USkeletalMesh* Mesh = GetSkeletalMesh();
	if (!Mesh) return;
	FSkeletalMesh* Asset = Mesh->GetSkeletalMeshAsset();
	if (!Asset || Asset->Bones.empty()) return;

	FSM = UObjectManager::Get().CreateObject<UAnimationStateMachine>(this);

	// Idle: 루트 본만 Z 축 sway — 정지중 미세 흔들림.
	UAnimSequence* Idle = UAnimSequence::CreateMockSwaySequence(
		Mesh, /*BoneIdx*/0, /*Duration*/1.5f, /*AmpDeg*/8.0f);

	// Walk: 전 본 sinusoidal wave — 위상차로 chain 진행처럼 보임.
	UAnimSequence* Walk = UAnimSequence::CreateMockWaveSequence(
		Mesh, /*Duration*/0.8f, /*AmpDeg*/15.0f);

	FSM->RegisterState(MakeState(FName("Idle"), Idle, 1.0f, true, this));
	FSM->RegisterState(MakeState(FName("Walk"), Walk, 1.0f, true, this));

	// Condition 람다는 this 캡처로 멤버 Speed 를 읽음. 새 조건은 람다 추가만으로 끝.
	FSM->RegisterTransition({
		FName("Idle"), FName("Walk"),
		[this](UAnimInstance*) { return Speed >  SpeedThreshold; },
		/*BlendTime*/0.20f
	});
	FSM->RegisterTransition({
		FName("Walk"), FName("Idle"),
		[this](UAnimInstance*) { return Speed <= SpeedThreshold; },
		0.20f
	});

	FSM->SetInitialState(FName("Idle"));
}

void UCharacterAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	if (bAutoDriveSpeed && AutoPeriodSec > 0.0f)
	{
		ElapsedTime += DeltaSeconds;
		const float Omega = 2.0f * FMath::Pi / AutoPeriodSec;
		// Speed 평균이 SpeedThreshold 근방이 되도록 오프셋 (== AutoSpeedAmp).
		Speed = AutoSpeedAmp + AutoSpeedAmp * std::sin(ElapsedTime * Omega);
	}

	if (FSM) FSM->Tick(this, DeltaSeconds);
}

void UCharacterAnimInstance::EvaluateAnimation(FPoseContext& Output)
{
	if (FSM) FSM->Evaluate(this, Output);
	else     Super::EvaluateAnimation(Output);
}

void UCharacterAnimInstance::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	Super::GetEditableProperties(OutProps);

	// 카테고리는 "Animation|Character" — 컴포넌트의 "Animation" 카테고리와 같은 그룹 안에 옆에 표시.
	const char* Category = "Animation|Character";

	FPropertyDescriptor AutoProp;
	AutoProp.Name     = "Auto Drive Speed";
	AutoProp.Type     = EPropertyType::Bool;
	AutoProp.Category = Category;
	AutoProp.ValuePtr = &bAutoDriveSpeed;
	OutProps.push_back(AutoProp);

	FPropertyDescriptor SpeedProp;
	SpeedProp.Name     = "Speed";
	SpeedProp.Type     = EPropertyType::Float;
	SpeedProp.Category = Category;
	SpeedProp.ValuePtr = &Speed;
	SpeedProp.Min      = 0.0f;
	SpeedProp.Max      = 100.0f;
	SpeedProp.Speed    = 0.5f;
	OutProps.push_back(SpeedProp);

	FPropertyDescriptor ThreshProp;
	ThreshProp.Name     = "Speed Threshold";
	ThreshProp.Type     = EPropertyType::Float;
	ThreshProp.Category = Category;
	ThreshProp.ValuePtr = &SpeedThreshold;
	ThreshProp.Min      = 0.0f;
	ThreshProp.Max      = 50.0f;
	ThreshProp.Speed    = 0.1f;
	OutProps.push_back(ThreshProp);

	FPropertyDescriptor PeriodProp;
	PeriodProp.Name     = "Auto Period (s)";
	PeriodProp.Type     = EPropertyType::Float;
	PeriodProp.Category = Category;
	PeriodProp.ValuePtr = &AutoPeriodSec;
	PeriodProp.Min      = 0.1f;
	PeriodProp.Max      = 30.0f;
	PeriodProp.Speed    = 0.1f;
	OutProps.push_back(PeriodProp);

	FPropertyDescriptor AmpProp;
	AmpProp.Name     = "Auto Speed Amp";
	AmpProp.Type     = EPropertyType::Float;
	AmpProp.Category = Category;
	AmpProp.ValuePtr = &AutoSpeedAmp;
	AmpProp.Min      = 0.0f;
	AmpProp.Max      = 100.0f;
	AmpProp.Speed    = 0.5f;
	OutProps.push_back(AmpProp);
}
