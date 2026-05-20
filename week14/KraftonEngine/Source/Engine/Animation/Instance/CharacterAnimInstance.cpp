#include "CharacterAnimInstance.h"

#include "Animation/StateMachine/AnimState.h"
#include "Animation/Sequence/AnimSequence.h"
#include "Animation/PoseContext.h"
#include "Animation/Nodes/AnimNode_Slot.h"
#include "Animation/Nodes/AnimNode_StateMachine.h"
#include "Core/Types/PropertyTypes.h"
#include "Math/MathUtils.h"
#include "Mesh/Skeletal/SkeletalMesh.h"
#include "Mesh/Skeletal/SkeletalMeshAsset.h"
#include "Serialization/Archive.h"

#include <cmath>
namespace
{
	// UAnimState 인스턴스 한 개 만들고 필드 채워서 반환. Outer 는 보통 AnimInstance.
	UAnimState* MakeAnimState(FName Name, UAnimSequenceBase* Sequence, float PlayRate, bool bLoop, UObject* Outer)
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

	// Idle: 루트 본만 Z 축 sway — 정지중 미세 흔들림.
	UAnimSequence* Idle = UAnimSequence::CreateMockSwaySequence(
		Mesh, /*BoneIdx*/0, /*Duration*/1.5f, /*AmpDeg*/8.0f);

	// Walk: 전 본 sinusoidal wave — 위상차로 chain 진행처럼 보임.
	UAnimSequence* Walk = UAnimSequence::CreateMockWaveSequence(
		Mesh, /*Duration*/0.8f, /*AmpDeg*/15.0f);

	// AnimGraph 트리 build — Phase 1.5b: sub-state-machine 데모.
	//   RootNode = Top-SM
	//                └─ State "Locomotion"  (SubGraphOverride = Loco-SM)
	//                       Loco-SM ├─ State "Idle" → Idle Sequence
	//                                └─ State "Walk" → Walk Sequence
	//                                transitions: Idle↔Walk (Speed)
	//
	// Top-SM 이 단일 state 만 갖는 건 일시적 — Jump anim 자산 추가 시 Top 에 "Jump" state 추가 +
	// 두 transition 등록만으로 확장. sub-state-machine 의 핵심 의미 (자기 안에 sub-SM 보유한
	// state) 는 단일 state 라도 검증 가능.
	//
	// MakeNode 헬퍼가 OwnedNodes 에 push 후 raw 반환 — lifetime 은 AnimInstance 가 관리.

	FAnimNode_StateMachine* LocoSM = MakeNode<FAnimNode_StateMachine>();
	LocoSM->RegisterState(MakeAnimState(FName("Idle"), Idle, 1.0f, true, this));
	LocoSM->RegisterState(MakeAnimState(FName("Walk"), Walk, 1.0f, true, this));
	LocoSM->RegisterTransition({
		FName("Idle"), FName("Walk"),
		[this](UAnimInstance*) { return Speed >  SpeedThreshold; },
		/*BlendTime*/0.20f
	});
	LocoSM->RegisterTransition({
		FName("Walk"), FName("Idle"),
		[this](UAnimInstance*) { return Speed <= SpeedThreshold; },
		0.20f
	});
	LocoSM->SetInitialState(FName("Idle"));

	// Top SM 의 "Locomotion" state — SubGraphOverride 로 Loco-SM 위임.
	UAnimState* LocomotionState = UObjectManager::Get().CreateObject<UAnimState>(this);
	LocomotionState->StateName        = FName("Locomotion");
	LocomotionState->SubGraphOverride = LocoSM;

	FAnimNode_StateMachine* TopSM = MakeNode<FAnimNode_StateMachine>();
	TopSM->RegisterState(LocomotionState);
	TopSM->SetInitialState(FName("Locomotion"));

	// DefaultSlot 으로 wrap — RootNode 경로의 montage 가 트리 안 Slot 노드 통해 처리되도록.
	// Slot.InputPose = TopSM 이므로 평소엔 montage 없으면 pass-through (overhead 무), 좌클릭
	// attack montage 같은 PlayMontage 호출 시 Slot 안에서 lerp.
	FAnimNode_Slot* DefaultSlot = MakeNode<FAnimNode_Slot>();
	DefaultSlot->SlotName  = DefaultMontageSlot;
	DefaultSlot->InputPose = TopSM;

	// RootNode 박기 — Initialize 호출이 DefaultSlot.InputPose (TopSM) → TopSM.CurrentState
	// (Locomotion) ->OnEnter → SubGraphOverride (Loco-SM) ->OnBecomeRelevant →
	// Loco-SM 의 CurrentState (Idle) ->OnEnter 까지 재귀로 sub-graph 까지 초기화.
	SetRootNode(DefaultSlot);
}

void UCharacterAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	// 사용자 변수 갱신 — RootNode 유무와 무관하게 매 frame 호출됨. 결과 Speed 는
	// AnimGraph 의 transition condition 람다가 같은 frame 의 RootNode->Update 에서 사용.
	if (bAutoDriveSpeed && AutoPeriodSec > 0.0f)
	{
		ElapsedTime += DeltaSeconds;
		const float Omega = 2.0f * FMath::Pi / AutoPeriodSec;
		// Speed 평균이 SpeedThreshold 근방이 되도록 오프셋 (== AutoSpeedAmp).
		Speed = AutoSpeedAmp + AutoSpeedAmp * std::sin(ElapsedTime * Omega);
	}
}

void UCharacterAnimInstance::EvaluateAnimation(FPoseContext& Output)
{
	// RootNode 항상 set — UAnimInstance::EvaluatePose 가 직접 트리 평가. 여기 도달 X.
	// Safety fallback 으로 ref pose.
	Super::EvaluateAnimation(Output);
}

void UCharacterAnimInstance::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	// Editor-set 데모 파라미터만 — Speed/ElapsedTime 같은 runtime 변수는 매 frame 덮어쓰므로 제외.
	Ar << bAutoDriveSpeed;
	Ar << SpeedThreshold;
	Ar << AutoPeriodSec;
	Ar << AutoSpeedAmp;
}
