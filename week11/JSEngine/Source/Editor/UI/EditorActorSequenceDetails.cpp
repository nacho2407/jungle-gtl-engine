#include "Editor/UI/EditorActorSequenceDetails.h"

#include "Animation/ActorSequence.h"
#include "Component/ActorSequenceComponent.h"
#include "Editor/EditorEngine.h"
#include "Editor/UI/EditorMainPanel.h"
#include "ImGui/imgui.h"
#include <algorithm>

#ifdef GetCurrentTime
#undef GetCurrentTime
#endif

namespace
{
	static void DrawDetailsSeparator()
	{
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();
	}

	static void DrawDetailsSectionLabel(const char* Label)
	{
		ImVec2 Pos = ImGui::GetCursorScreenPos();
		ImDrawList* DrawList = ImGui::GetWindowDrawList();
		const ImU32 Color = ImGui::GetColorU32(ImGuiCol_Text);
		DrawList->AddText(ImVec2(Pos.x + 0.75f, Pos.y), Color, Label);
		ImGui::TextUnformatted(Label);
	}

}

void FEditorActorSequenceDetails::Initialize(UEditorEngine* InEditorEngine, bool* InUndoCaptureFlag)
{
	EditorEngine = InEditorEngine;
	UndoCaptureFlag = InUndoCaptureFlag;
}

void FEditorActorSequenceDetails::MarkEdited(
	UActorSequenceComponent* SequenceComp,
	const char* UndoLabel)
{
	if (!SequenceComp)
	{
		return;
	}

	if (EditorEngine && UndoCaptureFlag && !*UndoCaptureFlag)
	{
		EditorEngine->GetUndoSystem().CaptureSnapshot(UndoLabel ? UndoLabel : "Edit Actor Sequence");
		*UndoCaptureFlag = true;
	}

	SequenceComp->MarkSequenceDirty();
	SequenceComp->PostEditChangeProperty({ "Sequence", EPropertyChangeType::ValueSet });

	if (EditorEngine)
	{
		EditorEngine->GetSceneService().MarkDirty();
	}
}

void FEditorActorSequenceDetails::Render(UActorSequenceComponent* SequenceComp, float DeltaTime)
{
	(void)DeltaTime;

	UActorSequence* Sequence = SequenceComp ? SequenceComp->GetSequence() : nullptr;
	if (!Sequence)
	{
		return;
	}

	DrawDetailsSeparator();
	DrawDetailsSectionLabel("Actor Sequence");

	if (ImGui::Button("Open Sequencer"))
	{
		EditorEngine->GetMainPanel().OpenActorSequencer(SequenceComp);
	}
	ImGui::SameLine();
	ImGui::TextDisabled("Timeline editor");

	bool bAutoPlay = SequenceComp->IsAutoPlay();
	if (ImGui::Checkbox("Auto Play", &bAutoPlay))
	{
		SequenceComp->SetAutoPlay(bAutoPlay);
		MarkEdited(SequenceComp, "Edit Actor Sequence");
	}

	bool bLooping = SequenceComp->IsLooping();
	if (ImGui::Checkbox("Looping", &bLooping))
	{
		SequenceComp->SetLooping(bLooping);
		MarkEdited(SequenceComp, "Edit Actor Sequence");
	}

	float PlayRate = SequenceComp->GetPlayRate();
	if (ImGui::DragFloat("Play Rate", &PlayRate, 0.01f, 0.001f, 100.0f))
	{
		SequenceComp->SetPlayRate(std::max(0.001f, PlayRate));
		MarkEdited(SequenceComp, "Edit Actor Sequence");
	}

	bool bPauseAtEnd = SequenceComp->ShouldPauseAtEnd();
	if (ImGui::Checkbox("Pause at End", &bPauseAtEnd))
	{
		SequenceComp->SetPauseAtEnd(bPauseAtEnd);
		MarkEdited(SequenceComp, "Edit Actor Sequence");
	}

	float StartOffsetSeconds = SequenceComp->GetStartOffsetSeconds();
	if (ImGui::DragFloat("Start Offset (seconds)", &StartOffsetSeconds, 0.01f, 0.0f, 100000.0f))
	{
		SequenceComp->SetStartOffsetSeconds(std::max(0.0f, StartOffsetSeconds));
		MarkEdited(SequenceComp, "Edit Actor Sequence");
	}

	if ((ImGui::IsItemDeactivatedAfterEdit() || !ImGui::IsAnyItemActive()) && UndoCaptureFlag)
	{
		*UndoCaptureFlag = false;
	}
}

