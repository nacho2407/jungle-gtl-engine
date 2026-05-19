#include "CharacterAnimInstance.h"

#include "Animation/AnimationStateMachine.h"
#include "Animation/AnimState.h"
#include "Animation/AnimSequence.h"
#include "Animation/PoseContext.h"
#include "Animation/Nodes/AnimNode_StateMachine.h"
#include "Core/PropertyTypes.h"
#include "Math/MathUtils.h"
#include "Mesh/SkeletalMesh.h"
#include "Mesh/SkeletalMeshAsset.h"
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

	// AnimGraph 트리 build — Phase 1.5: 단순 SM 한 개 (sub-SM 데모는 후속 step).
	// MakeNode 헬퍼가 OwnedNodes 에 push 후 raw 반환 — lifetime 은 AnimInstance 가 관리.
	FAnimNode_StateMachine* SM = MakeNode<FAnimNode_StateMachine>();
	SM->RegisterState(MakeAnimState(FName("Idle"), Idle, 1.0f, true, this));
	SM->RegisterState(MakeAnimState(FName("Walk"), Walk, 1.0f, true, this));

	// Condition 람다는 this 캡처로 멤버 Speed 를 읽음 — Speed 는 NativeUpdateAnimation 이
	// 매 frame 갱신 (auto-drive sin 또는 외부 push). 새 조건은 람다 추가만으로 끝.
	SM->RegisterTransition({
		FName("Idle"), FName("Walk"),
		[this](UAnimInstance*) { return Speed >  SpeedThreshold; },
		/*BlendTime*/0.20f
	});
	SM->RegisterTransition({
		FName("Walk"), FName("Idle"),
		[this](UAnimInstance*) { return Speed <= SpeedThreshold; },
		0.20f
	});

	SM->SetInitialState(FName("Idle"));

	// RootNode 박기 — UAnimInstance::UpdateAnimation 이 매 frame 트리 평가하게 됨.
	// FSM wrapper 멤버는 사용 안 함 (FSM == nullptr 유지). legacy 경로 안 탐.
	SetRootNode(SM);
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

	// Legacy fallback — RootNode 가 set 안 됐을 때만 FSM wrapper 사용. 현재 흐름상 도달 X
	// (NativeInitializeAnimation 에서 SetRootNode 항상 호출), 안전망으로 유지.
	if (!GetRootNode() && FSM)
	{
		FSM->Tick(this, DeltaSeconds);
	}
}

void UCharacterAnimInstance::EvaluateAnimation(FPoseContext& Output)
{
	// RootNode 가 set 되어 있으면 UAnimInstance::EvaluatePose 가 직접 트리 평가 — 여기 안 옴.
	// Legacy fallback (RootNode null) 만 위해 유지.
	if (FSM) FSM->Evaluate(this, Output);
	else     Super::EvaluateAnimation(Output);
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
