#include "AnimNode_BlendListByEnum.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimationRuntime.h"
#include "Animation/PoseContext.h"
#include "Math/Quat.h"

#include <algorithm>

void FAnimNode_BlendListByEnum::Initialize(const FAnimationInitializeContext& Context)
{
	// 모든 입력 노드 init — 자식 깊이 init 보장. StateMachine 과 달리 transition 없으므로
	// 첫 활성 child OnBecomeRelevant 는 여기서 명시 호출.
	for (FAnimNode_Base* P : InputPoses)
	{
		if (P) P->Initialize(Context);
	}

	CurrentChildIndex  = IsValidIndex(ActiveChildIndex) ? ActiveChildIndex : -1;
	PreviousChildIndex = -1;
	BlendAlpha         = 1.0f;

	if (CurrentChildIndex >= 0)
	{
		InputPoses[CurrentChildIndex]->OnBecomeRelevant(Context);
	}
}

void FAnimNode_BlendListByEnum::OnBecomeRelevant(const FAnimationInitializeContext& Context)
{
	// 부모 그래프 (예: 상위 StateMachine) 가 이 노드를 활성화시킬 때 — 현재 child 활성화 통지.
	if (IsValidIndex(CurrentChildIndex))
	{
		InputPoses[CurrentChildIndex]->OnBecomeRelevant(Context);
	}
}

void FAnimNode_BlendListByEnum::OnDormant()
{
	// 비활성화될 때 — 진행중 전이 종료 + 현재/이전 child 모두 OnDormant.
	if (IsValidIndex(PreviousChildIndex))
	{
		InputPoses[PreviousChildIndex]->OnDormant();
	}
	if (IsValidIndex(CurrentChildIndex))
	{
		InputPoses[CurrentChildIndex]->OnDormant();
	}
	PreviousChildIndex = -1;
	BlendAlpha         = 1.0f;
}

void FAnimNode_BlendListByEnum::Update(const FAnimationUpdateContext& Context)
{
	// AnimGraph 컴파일러가 박은 SelectorFn 이 있으면 매 frame ActiveChildIndex 갱신.
	// UCharacterAnimInstance 처럼 외부가 ActiveChildIndex 직접 박는 흐름은 SelectorFn 미설정.
	if (SelectorFn)
	{
		ActiveChildIndex = SelectorFn(Context.AnimInstance);
	}

	// ── 1) ActiveChildIndex 변경 감지 → 전이 시작 ──
	// 진행중 전이가 있는 도중에 또 바뀌면 Previous 는 직전 Current 로 즉시 교체 (latest pair 만 유지).
	if (ActiveChildIndex != CurrentChildIndex && IsValidIndex(ActiveChildIndex))
	{
		// 진행중인 Previous 가 있었으면 그것은 OnDormant — 더 이상 추적 안 함.
		if (IsValidIndex(PreviousChildIndex) && PreviousChildIndex != CurrentChildIndex)
		{
			InputPoses[PreviousChildIndex]->OnDormant();
		}

		PreviousChildIndex = CurrentChildIndex;
		CurrentChildIndex  = ActiveChildIndex;
		BlendAlpha         = (BlendTime > 0.0f) ? 0.0f : 1.0f;

		// 새 Current 활성화 통지. context 는 update context 이지만 OnBecomeRelevant 는
		// initialize context — adapter 로 변환.
		FAnimationInitializeContext InitCtx;
		InitCtx.AnimInstance = Context.AnimInstance;
		InitCtx.SkeletalMesh = Context.AnimInstance ? Context.AnimInstance->GetSkeletalMesh() : nullptr;
		InputPoses[CurrentChildIndex]->OnBecomeRelevant(InitCtx);

		// Instant cut — Previous 즉시 OnDormant 후 잊음.
		if (BlendTime <= 0.0f && IsValidIndex(PreviousChildIndex))
		{
			InputPoses[PreviousChildIndex]->OnDormant();
			PreviousChildIndex = -1;
		}
	}

	// ── 2) 자식 Update + alpha 진행 ──
	const float ParentW = Context.FinalBlendWeight;
	const bool  bBlending = IsValidIndex(PreviousChildIndex);

	// Current child — visible weight = parent * alpha (전이중) 또는 parent * 1 (전이 종료).
	if (IsValidIndex(CurrentChildIndex))
	{
		FAnimationUpdateContext ChildCtx = Context;
		ChildCtx.FinalBlendWeight = ParentW * (bBlending ? BlendAlpha : 1.0f);
		InputPoses[CurrentChildIndex]->Update(ChildCtx);
	}

	// Previous child — visible weight = parent * (1 - alpha). alpha 1.0 도달 시 OnDormant + 제거.
	if (bBlending)
	{
		FAnimationUpdateContext ChildCtx = Context;
		ChildCtx.FinalBlendWeight = ParentW * (1.0f - BlendAlpha);
		InputPoses[PreviousChildIndex]->Update(ChildCtx);

		if (BlendTime > 0.0f)
		{
			BlendAlpha += Context.DeltaSeconds / BlendTime;
		}
		else
		{
			BlendAlpha = 1.0f;
		}

		if (BlendAlpha >= 1.0f)
		{
			BlendAlpha = 1.0f;
			InputPoses[PreviousChildIndex]->OnDormant();
			PreviousChildIndex = -1;
		}
	}

	// ── 3) Root motion lerp — 자기 LastRM 채움, 외부 누적 X (RootNode 한 곳에서만 누적). ──
	const bool bBlendingNow = IsValidIndex(PreviousChildIndex);
	if (!bBlendingNow)
	{
		LastRootMotionDelta = IsValidIndex(CurrentChildIndex)
			? InputPoses[CurrentChildIndex]->GetLastRootMotionDelta()
			: FTransform();
	}
	else
	{
		const FTransform& PrevRM = InputPoses[PreviousChildIndex]->GetLastRootMotionDelta();
		const FTransform& CurRM  = IsValidIndex(CurrentChildIndex)
			? InputPoses[CurrentChildIndex]->GetLastRootMotionDelta()
			: FTransform();
		LastRootMotionDelta.Location = PrevRM.Location * (1.0f - BlendAlpha) + CurRM.Location * BlendAlpha;
		LastRootMotionDelta.Rotation = FQuat::Slerp(PrevRM.Rotation.GetNormalized(),
		                                            CurRM.Rotation.GetNormalized(),
		                                            BlendAlpha).GetNormalized();
	}
}

void FAnimNode_BlendListByEnum::Evaluate(FPoseContext& Output)
{
	if (!IsValidIndex(CurrentChildIndex))
	{
		Output.ResetToRefPose();
		return;
	}

	// 전이중 아니면 Current 그대로 출력.
	if (!IsValidIndex(PreviousChildIndex))
	{
		InputPoses[CurrentChildIndex]->Evaluate(Output);
		return;
	}

	// 전이중 — 두 pose 평가 후 lerp. BlendTwoPosesTogether 는 (A, B, Alpha=B 의 가중치, Out) 시그니처.
	FPoseContext PrevPose;
	PrevPose.SkeletalMesh = Output.SkeletalMesh;
	PrevPose.ResetToRefPose();
	InputPoses[PreviousChildIndex]->Evaluate(PrevPose);

	FPoseContext CurPose;
	CurPose.SkeletalMesh = Output.SkeletalMesh;
	CurPose.ResetToRefPose();
	InputPoses[CurrentChildIndex]->Evaluate(CurPose);

	FAnimationRuntime::BlendTwoPosesTogether(PrevPose, CurPose, BlendAlpha, Output);
}
