#include "Editor/UI/EditorAnimationDebugWidget.h"

#include "Editor/EditorEngine.h"
#include "Editor/Selection/SelectionManager.h"

#include "ImGui/imgui.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimNotify.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/AnimState.h"
#include "Animation/AnimationMode.h"
#include "Animation/AnimationStateMachine.h"
#include "Animation/CharacterAnimInstance.h"
#include "Component/SkeletalMeshComponent.h"
#include "Core/PropertyTypes.h"
#include "GameFramework/AActor.h"
#include "Object/UClass.h"

#include <cstring>

namespace
{
	// AnimInstance 자식이 FSM 을 들고 있으면 반환. 현재는 UCharacterAnimInstance 만 대응 —
	// 추후 UAnimInstance 에 virtual GetFSM() 같은 후크 두면 일반화 가능.
	UAnimationStateMachine* GetFSMOf(UAnimInstance* AnimInst)
	{
		if (UCharacterAnimInstance* Char = Cast<UCharacterAnimInstance>(AnimInst))
		{
			return Char->GetFSM();
		}
		return nullptr;
	}
}

void FEditorAnimationDebugWidget::Render(float /*DeltaTime*/)
{
	ImGui::SetNextWindowSize(ImVec2(420.0f, 540.0f), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Animation Debug"))
	{
		ImGui::End();
		return;
	}

	AActor* PrimaryActor = EditorEngine
		? EditorEngine->GetSelectionManager().GetPrimarySelection()
		: nullptr;

	if (!PrimaryActor)
	{
		ImGui::TextDisabled("No actor selected.");
		ImGui::End();
		return;
	}

	USkeletalMeshComponent* SMC = PrimaryActor->GetComponentByClass<USkeletalMeshComponent>();
	if (!SMC)
	{
		ImGui::TextDisabled("Selected actor has no SkeletalMeshComponent.");
		ImGui::End();
		return;
	}

	UAnimInstance* AnimInst = SMC->GetAnimInstance();
	const EAnimationMode Mode = SMC->GetAnimationMode();

	ImGui::Text("Target: %s", PrimaryActor->GetName().c_str());
	{
		const int32 ModeIdx = static_cast<int32>(Mode);
		const char* ModeName = (ModeIdx >= 0 && static_cast<uint32>(ModeIdx) < GAnimationModeCount)
			? GAnimationModeNames[ModeIdx] : "?";
		ImGui::Text("  Mode: %s", ModeName);
	}
	ImGui::Text("  AnimInstance: %s",
		AnimInst ? AnimInst->GetClass()->GetName() : "(none)");

	ImGui::Separator();

	if (!AnimInst)
	{
		ImGui::TextDisabled("No AnimInstance (Mode=None or AnimInstanceClass unset).");
		ImGui::End();
		return;
	}

	RenderFSMSection(AnimInst);
	ImGui::Separator();

	RenderVariablesSection(AnimInst);
	ImGui::Separator();

	RenderRecentNotifiesSection(AnimInst);

	ImGui::End();
}

void FEditorAnimationDebugWidget::RenderFSMSection(UAnimInstance* AnimInst)
{
	UAnimationStateMachine* FSM = GetFSMOf(AnimInst);
	if (!FSM)
	{
		ImGui::TextDisabled("AnimInstance has no state machine (FSM).");
		return;
	}

	ImGui::Text("State Machine");

	UAnimState* Current = FSM->GetCurrentState();
	if (Current)
	{
		const float SeqLen = Current->Sequence ? Current->Sequence->GetPlayLength() : 0.0f;
		ImGui::Text("  Current: %s  (t %.2fs / %.2fs, x%.2f, %s)",
			Current->StateName.ToString().c_str(),
			Current->GetLocalTime(), SeqLen,
			Current->PlayRate,
			Current->bLooping ? "loop" : "once");
	}
	else
	{
		ImGui::TextDisabled("  No current state.");
	}

	// Multi-blend 시각화 — BlendingFroms 의 모든 진행중 from 항목 표시.
	// oldest=[0] → latest=back 순서. latest 가 CurrentState 와 직접 blend 중인 from.
	const TArray<FBlendingFrom>& BlendingFroms = FSM->GetBlendingFroms();
	for (size_t i = 0; i < BlendingFroms.size(); ++i)
	{
		const FBlendingFrom& BF = BlendingFroms[i];
		if (!BF.State) continue;
		ImGui::Text("  Blending from [%zu]: %s  (alpha=%.2f, %.2fs / %.2fs)",
			i,
			BF.State->StateName.ToString().c_str(),
			BF.Alpha, BF.Alpha * BF.Duration, BF.Duration);
		ImGui::ProgressBar(BF.Alpha, ImVec2(-1.0f, 6.0f), "");
	}

	const TArray<UAnimState*>& States = FSM->GetStates();
	if (ImGui::TreeNodeEx("States", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed))
	{
		for (UAnimState* S : States)
		{
			if (!S) continue;
			const bool bIsCurrent = (S == Current);
			const ImVec4 Color = bIsCurrent
				? ImVec4(0.4f, 1.0f, 0.4f, 1.0f)
				: ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
			ImGui::TextColored(Color, "  %s %s   [%s, x%.2f, %s]",
				bIsCurrent ? "*" : " ",
				S->StateName.ToString().c_str(),
				S->Sequence ? S->Sequence->GetName().c_str() : "(no seq)",
				S->PlayRate,
				S->bLooping ? "loop" : "once");
		}
		ImGui::TreePop();
	}

	const TArray<FStateTransition>& Transitions = FSM->GetTransitions();
	if (ImGui::TreeNodeEx("Transitions", ImGuiTreeNodeFlags_Framed))
	{
		for (const FStateTransition& T : Transitions)
		{
			const FString FromStr = (T.From == FName::None)
				? FString("(AnyState)")
				: T.From.ToString();
			ImGui::Text("  %s  ->  %s   (Blend %.2fs)",
				FromStr.c_str(),
				T.To.ToString().c_str(),
				T.BlendTime);
		}
		ImGui::TreePop();
	}
}

void FEditorAnimationDebugWidget::RenderVariablesSection(UAnimInstance* AnimInst)
{
	ImGui::Text("Variables");

	TArray<FPropertyValue> Props;
	AnimInst->GetEditableProperties(Props);

	if (Props.empty())
	{
		ImGui::TextDisabled("  (none exposed)");
		return;
	}

	for (const FPropertyValue& P : Props)
	{
		RenderPropertyReadOnly(P);
	}
}

void FEditorAnimationDebugWidget::RenderPropertyReadOnly(const FPropertyValue& P)
{
	void* ValuePtr = P.GetValuePtr();
	if (!ValuePtr) return;

	switch (P.GetType())
	{
	case EPropertyType::Bool:
		ImGui::Text("  %s: %s", P.GetDisplayName(),
			*static_cast<bool*>(ValuePtr) ? "true" : "false");
		break;

	case EPropertyType::ByteBool:
		ImGui::Text("  %s: %s", P.GetDisplayName(),
			*static_cast<uint8_t*>(ValuePtr) ? "true" : "false");
		break;

	case EPropertyType::Int:
		ImGui::Text("  %s: %d", P.GetDisplayName(),
			*static_cast<int32*>(ValuePtr));
		break;

	case EPropertyType::Float:
		ImGui::Text("  %s: %.3f", P.GetDisplayName(),
			*static_cast<float*>(ValuePtr));
		break;

	case EPropertyType::Enum:
		if (const FEnum* EnumType = P.GetEnumType())
		{
			int32 Val = 0;
			std::memcpy(&Val, ValuePtr, EnumType->GetSize());
			const char* Name = (Val >= 0 && static_cast<uint32>(Val) < EnumType->GetCount())
				? EnumType->GetNames()[Val] : "(out of range)";
			ImGui::Text("  %s: %s", P.GetDisplayName(), Name);
		}
		break;

	default:
		// 다른 타입 (Vec3/String/ObjectRef/ClassRef 등) 은 이번 패널 범위에선 생략.
		ImGui::Text("  %s: (type not displayed)", P.GetDisplayName());
		break;
	}
}

void FEditorAnimationDebugWidget::RenderRecentNotifiesSection(UAnimInstance* AnimInst)
{
	ImGui::Text("Recent Notifies");

	const TArray<FQueuedAnimNotify>& Notifies = AnimInst->GetRecentNotifies();
	if (Notifies.empty())
	{
		ImGui::TextDisabled("  (none yet)");
		return;
	}

	// 최근 순으로 위→아래 표시 (가장 최근이 맨 위).
	for (auto It = Notifies.rbegin(); It != Notifies.rend(); ++It)
	{
		const FQueuedAnimNotify& Q = *It;
		const char* NotifyClass = Q.Event.Notify
			? Q.Event.Notify->GetClass()->GetName()
			: "(no class)";
		const FString SeqName = Q.Sequence ? Q.Sequence->GetName() : FString("(no seq)");
		ImGui::Text("  %.2fs  %s  @%s  -> %s",
			Q.Event.TriggerTime,
			Q.Event.NotifyName.ToString().c_str(),
			SeqName.c_str(),
			NotifyClass);
	}
}
