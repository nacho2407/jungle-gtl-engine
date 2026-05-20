#include "Editor/UI/Debug/EditorAnimationDebugWidget.h"

#include "Editor/EditorEngine.h"
#include "Editor/Selection/SelectionManager.h"

#include "ImGui/imgui.h"

#include "Animation/AnimInstance.h"
#include "Animation/Montage/AnimMontage.h"
#include "Animation/Montage/AnimMontageInstance.h"
#include "Animation/Notify/AnimNotify.h"
#include "Animation/Sequence/AnimSequenceBase.h"
#include "Animation/StateMachine/AnimState.h"
#include "Animation/AnimationMode.h"
#include "Animation/Nodes/AnimNode_Base.h"
#include "Animation/Nodes/AnimNode_BlendListByEnum.h"
#include "Animation/Nodes/AnimNode_LayeredBlendPerBone.h"
#include "Animation/Nodes/AnimNode_Root.h"
#include "Animation/Nodes/AnimNode_SequencePlayer.h"
#include "Animation/Nodes/AnimNode_Slot.h"
#include "Animation/Nodes/AnimNode_StateMachine.h"
#include "Component/Primitive/SkeletalMeshComponent.h"
#include "Core/Types/PropertyTypes.h"
#include "GameFramework/AActor.h"
#include "Object/Reflection/UClass.h"

#include <cstring>
#include <string>

namespace
{
	// AnimGraph 트리 재귀 시각화 — RootNode 부터 자식까지 들여쓰기 + 노드별 추가 정보.
	// dynamic_cast 로 노드 타입 분기, 자식 노드 enumerate 후 재귀.
	void DrawAnimNode(FAnimNode_Base* Node, int Indent, UAnimInstance* AnimInst)
	{
		if (!Node) return;

		const std::string IndentStr(static_cast<size_t>(Indent) * 2, ' ');
		const char* TypeName = Node->GetDebugName();

		if (auto* Root = dynamic_cast<FAnimNode_Root*>(Node))
		{
			ImGui::Text("%s[%s]", IndentStr.c_str(), TypeName);
			DrawAnimNode(Root->ChildPose, Indent + 1, AnimInst);
		}
		else if (auto* SM = dynamic_cast<FAnimNode_StateMachine*>(Node))
		{
			const FName CurrentName = SM->GetCurrentStateName();
			UAnimState*  CurrentState = SM->GetCurrentState();
			const FString CurrentSeqName = (CurrentState && CurrentState->Sequence)
				? CurrentState->Sequence->GetName()
				: FString("(no seq)");
			ImGui::Text("%s[%s] current=%s  (%s)",
				IndentStr.c_str(), TypeName,
				CurrentName.ToString().c_str(),
				CurrentSeqName.c_str());

			// Multi-blend stack — 진행중 from 들 표시.
			const TArray<FBlendingFrom>& BlendingFroms = SM->GetBlendingFroms();
			for (size_t i = 0; i < BlendingFroms.size(); ++i)
			{
				const FBlendingFrom& BF = BlendingFroms[i];
				if (!BF.State) continue;
				ImGui::Text("%s  ↳ blending from [%zu]: %s  α=%.2f (%.2fs/%.2fs)",
					IndentStr.c_str(), i,
					BF.State->StateName.ToString().c_str(),
					BF.Alpha, BF.Alpha * BF.Duration, BF.Duration);
			}

			// CurrentState 의 SubGraphOverride 가 있으면 sub-SM 재귀 — sub-state-machine 표현.
			if (CurrentState && CurrentState->SubGraphOverride)
			{
				DrawAnimNode(CurrentState->SubGraphOverride, Indent + 1, AnimInst);
			}
		}
		else if (auto* Slot = dynamic_cast<FAnimNode_Slot*>(Node))
		{
			const float Effective = Slot->GetEffectiveBlendWeight();
			ImGui::Text("%s[%s] name=%s  effective=%.2f",
				IndentStr.c_str(), TypeName,
				Slot->SlotName.ToString().c_str(),
				Effective);

			// Active montage 정보.
			if (AnimInst)
			{
				if (UAnimMontageInstance* MI = AnimInst->GetMontageInstanceForSlot(Slot->SlotName))
				{
					if (MI->IsActive())
					{
						UAnimMontage* M = MI->GetCurrentMontage();
						ImGui::Text("%s  ↳ montage: %s  section=%s  W=%.2f",
							IndentStr.c_str(),
							M ? M->GetName().c_str() : "(?)",
							MI->GetCurrentSectionName().ToString().c_str(),
							MI->GetBlendWeight());
					}
				}
			}

			if (Slot->InputPose) DrawAnimNode(Slot->InputPose, Indent + 1, AnimInst);
		}
		else if (auto* Layer = dynamic_cast<FAnimNode_LayeredBlendPerBone*>(Node))
		{
			int32 MaskCount = 0;
			for (bool b : Layer->PerBoneMask) if (b) ++MaskCount;
			ImGui::Text("%s[%s] weight=%.2f  mask=%d/%zu bones",
				IndentStr.c_str(), TypeName,
				Layer->BlendWeight,
				MaskCount, Layer->PerBoneMask.size());
			ImGui::Text("%s  base:", IndentStr.c_str());
			DrawAnimNode(Layer->BasePose, Indent + 2, AnimInst);
			ImGui::Text("%s  blend:", IndentStr.c_str());
			DrawAnimNode(Layer->BlendPose, Indent + 2, AnimInst);
		}
		else if (auto* BL = dynamic_cast<FAnimNode_BlendListByEnum*>(Node))
		{
			const int32 NumPoses = static_cast<int32>(BL->InputPoses.size());
			const int32 Cur  = BL->GetCurrentChildIndex();
			const int32 Prev = BL->GetPreviousChildIndex();
			if (Prev >= 0)
			{
				ImGui::Text("%s[%s] poses=%d  active=%d  prev=%d  alpha=%.2f  blendT=%.2fs",
					IndentStr.c_str(), TypeName, NumPoses, Cur, Prev,
					BL->GetBlendAlpha(), BL->BlendTime);
			}
			else
			{
				ImGui::Text("%s[%s] poses=%d  active=%d  blendT=%.2fs",
					IndentStr.c_str(), TypeName, NumPoses, Cur, BL->BlendTime);
			}
			for (int32 i = 0; i < NumPoses; ++i)
			{
				const char* Tag = (i == Cur) ? "current" : (i == Prev) ? "prev" : "idle";
				ImGui::Text("%s  [%d] %s", IndentStr.c_str(), i, Tag);
				DrawAnimNode(BL->InputPoses[i], Indent + 2, AnimInst);
			}
		}
		else if (auto* Seq = dynamic_cast<FAnimNode_SequencePlayer*>(Node))
		{
			const float Len = Seq->Sequence ? Seq->Sequence->GetPlayLength() : 0.0f;
			const FString SeqName = Seq->Sequence ? Seq->Sequence->GetName() : FString("(no seq)");
			ImGui::Text("%s[%s] %s  t=%.2f/%.2fs  ×%.2f  %s",
				IndentStr.c_str(), TypeName,
				SeqName.c_str(),
				Seq->LocalTime, Len, Seq->PlayRate,
				Seq->bLooping ? "loop" : "once");
		}
		else
		{
			ImGui::Text("%s[%s]", IndentStr.c_str(), TypeName);
		}
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

	// RootNode 있으면 AnimGraph 트리 시각화 — sub-SM / Slot / LayeredBlend / SequencePlayer 재귀.
	// 없으면 anim graph 미구성 (SingleNode 등) — disabled label.
	if (AnimInst->GetRootNode())
	{
		RenderAnimGraphSection(AnimInst);
	}
	else
	{
		ImGui::TextDisabled("No anim graph (RootNode unset).");
	}
	ImGui::Separator();

	RenderVariablesSection(AnimInst);
	ImGui::Separator();

	RenderRecentNotifiesSection(AnimInst);

	ImGui::End();
}

void FEditorAnimationDebugWidget::RenderAnimGraphSection(UAnimInstance* AnimInst)
{
	ImGui::Text("Anim Graph");

	FAnimNode_Base* Root = AnimInst->GetRootNode();
	if (!Root)
	{
		ImGui::TextDisabled("  (no root node)");
		return;
	}

	DrawAnimNode(Root, 0, AnimInst);
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
