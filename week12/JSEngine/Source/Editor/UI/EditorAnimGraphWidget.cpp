#include "Editor/UI/EditorAnimGraphWidget.h"

#include "Core/ResourceManager.h"
#include "Core/Paths.h"
#include "Editor/EditorEngine.h"
#include "Editor/Notification/EditorNotificationService.h"
#include "ImGui/imgui.h"

#include <algorithm>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <limits>
#include <unordered_set>

namespace
{
	ImVec2 Add(const ImVec2& A, const ImVec2& B)
	{
		return ImVec2(A.x + B.x, A.y + B.y);
	}

	ImVec2 Sub(const ImVec2& A, const ImVec2& B)
	{
		return ImVec2(A.x - B.x, A.y - B.y);
	}

	ImVec2 ToImVec2(const FVector2& Value)
	{
		return ImVec2(Value.X, Value.Y);
	}

	FString GetFileName(const FString& Path)
	{
		const size_t SlashIndex = Path.find_last_of("/\\");
		return SlashIndex == FString::npos ? Path : Path.substr(SlashIndex + 1);
	}

	constexpr float AnimGraphNodeWidth = 280.0f;
	constexpr float AnimGraphNodeDefaultHeight = 108.0f;
	constexpr float AnimGraphNodeSequenceHeight = 142.0f;
	constexpr float AnimGraphNodeStateMachineHeight = 154.0f;
	constexpr float AnimGraphNodeHeaderHeight = 26.0f;
	constexpr float AnimGraphPinRadius = 5.0f;
	constexpr float AnimGraphPinHitRadius = 13.0f;

	constexpr float StateMachineEntryNodeWidth = 96.0f;
	constexpr float StateMachineEntryNodeHeight = 48.0f;
	constexpr float StateMachineStateNodeWidth = 190.0f;
	constexpr float StateMachineStateNodeHeight = 70.0f;
	constexpr float StateMachineNodePinRadius = 5.0f;
	constexpr float StateMachineNodeHeaderHeight = 22.0f;

	float GetAnimGraphNodeHeight(const FAnimGraphNodeDesc& Node)
	{
		switch (Node.Type)
		{
		case EAnimGraphNodeType::SequencePlayer:
			return AnimGraphNodeSequenceHeight;
		case EAnimGraphNodeType::StateMachine:
			return AnimGraphNodeStateMachineHeight;
		case EAnimGraphNodeType::OutputPose:
		default:
			return AnimGraphNodeDefaultHeight;
		}
	}

	ImVec2 GetAnimGraphNodeSize(const FAnimGraphNodeDesc& Node)
	{
		return ImVec2(AnimGraphNodeWidth, GetAnimGraphNodeHeight(Node));
	}

	bool IsMouseNear(const ImVec2& Position, float Radius)
	{
		const ImVec2 Mouse = ImGui::GetIO().MousePos;
		const float Dx = Mouse.x - Position.x;
		const float Dy = Mouse.y - Position.y;
		return Dx * Dx + Dy * Dy <= Radius * Radius;
	}



	long long MakeStateNameEditKey(int32 MachineNodeId, int32 StateId)
	{
		return (static_cast<long long>(MachineNodeId) << 32) ^ static_cast<unsigned int>(StateId);
	}

	void CopyToFixedBuffer(std::array<char, 256>& Buffer, const FString& Value)
	{
		std::fill(Buffer.begin(), Buffer.end(), '\0');
		std::strncpy(Buffer.data(), Value.c_str(), Buffer.size() - 1);
	}

	FString FitTextToWidth(const FString& Text, float MaxWidth)
	{
		if (MaxWidth <= 0.0f || ImGui::CalcTextSize(Text.c_str()).x <= MaxWidth)
		{
			return Text;
		}

		constexpr const char* Ellipsis = "...";
		const float EllipsisWidth = ImGui::CalcTextSize(Ellipsis).x;
		FString Result;
		Result.reserve(Text.size());
		for (char C : Text)
		{
			Result.push_back(C);
			if (ImGui::CalcTextSize(Result.c_str()).x + EllipsisWidth > MaxWidth)
			{
				if (!Result.empty())
				{
					Result.pop_back();
				}
				break;
			}
		}
		Result += Ellipsis;
		return Result;
	}

	struct FInputTextResizeCallbackData
	{
		FString* Value = nullptr;
	};

	int32 InputTextResizeCallback(ImGuiInputTextCallbackData* Data)
	{
		FInputTextResizeCallbackData* UserData = static_cast<FInputTextResizeCallbackData*>(Data->UserData);
		if (!UserData || !UserData->Value)
		{
			return 0;
		}

		FString& Value = *UserData->Value;
		if (Data->EventFlag == ImGuiInputTextFlags_CallbackResize)
		{
			Value.resize(static_cast<size_t>(Data->BufTextLen));
			Data->Buf = const_cast<char*>(Value.c_str());
		}
		return 0;
	}

	bool InputFString(const char* Label, FString& Value, ImGuiInputTextFlags Flags = 0)
	{
		Value.reserve(std::max<size_t>(Value.capacity(), 64));

		Flags |= ImGuiInputTextFlags_CallbackResize;
		FInputTextResizeCallbackData CallbackData;
		CallbackData.Value = &Value;
		const bool bChanged = ImGui::InputText(
			Label,
			const_cast<char*>(Value.c_str()),
			Value.capacity() + 1,
			Flags,
			InputTextResizeCallback,
			&CallbackData);
		if (bChanged)
		{
			Value.resize(std::strlen(Value.c_str()));
		}
		return bChanged;
	}

	void DrawArrowHead(ImDrawList* DrawList, const ImVec2& Tip, const ImVec2& Direction, ImU32 Color, float Size = 12.0f)
	{
		float Length = std::sqrt(Direction.x * Direction.x + Direction.y * Direction.y);
		if (Length <= 0.001f)
		{
			return;
		}

		const ImVec2 Unit(Direction.x / Length, Direction.y / Length);
		const ImVec2 Normal(-Unit.y, Unit.x);
		const ImVec2 Base = Sub(Tip, ImVec2(Unit.x * Size, Unit.y * Size));
		const ImVec2 P1 = Add(Base, ImVec2(Normal.x * Size * 0.55f, Normal.y * Size * 0.55f));
		const ImVec2 P2 = Sub(Base, ImVec2(Normal.x * Size * 0.55f, Normal.y * Size * 0.55f));
		DrawList->AddTriangleFilled(Tip, P1, P2, Color);
	}

	float DistanceSquaredToSegment(const ImVec2& Point, const ImVec2& A, const ImVec2& B)
	{
		const float ABx = B.x - A.x;
		const float ABy = B.y - A.y;
		const float APx = Point.x - A.x;
		const float APy = Point.y - A.y;
		const float LenSq = ABx * ABx + ABy * ABy;
		float T = LenSq > 0.0001f ? (APx * ABx + APy * ABy) / LenSq : 0.0f;
		T = std::max(0.0f, std::min(1.0f, T));
		const float ClosestX = A.x + ABx * T;
		const float ClosestY = A.y + ABy * T;
		const float Dx = Point.x - ClosestX;
		const float Dy = Point.y - ClosestY;
		return Dx * Dx + Dy * Dy;
	}

	ImVec2 CubicBezierPoint(const ImVec2& P0, const ImVec2& P1, const ImVec2& P2, const ImVec2& P3, float T)
	{
		const float U = 1.0f - T;
		const float TT = T * T;
		const float UU = U * U;
		const float UUU = UU * U;
		const float TTT = TT * T;
		return ImVec2(
			UUU * P0.x + 3.0f * UU * T * P1.x + 3.0f * U * TT * P2.x + TTT * P3.x,
			UUU * P0.y + 3.0f * UU * T * P1.y + 3.0f * U * TT * P2.y + TTT * P3.y);
	}

	float DistanceSquaredToBezier(const ImVec2& Point, const ImVec2& P0, const ImVec2& P1, const ImVec2& P2, const ImVec2& P3)
	{
		float BestDistanceSq = std::numeric_limits<float>::max();
		ImVec2 Prev = P0;
		constexpr int32 SampleCount = 24;
		for (int32 SampleIndex = 1; SampleIndex <= SampleCount; ++SampleIndex)
		{
			const float T = static_cast<float>(SampleIndex) / static_cast<float>(SampleCount);
			const ImVec2 Current = CubicBezierPoint(P0, P1, P2, P3, T);
			BestDistanceSq = std::min(BestDistanceSq, DistanceSquaredToSegment(Point, Prev, Current));
			Prev = Current;
		}
		return BestDistanceSq;
	}

	ImVec2 GetAnimGraphNodeInputPinPos(const FAnimGraphNodeDesc& Node, const ImVec2& CanvasOrigin)
	{
		return Add(CanvasOrigin, ImVec2(Node.Position.X, Node.Position.Y + GetAnimGraphNodeHeight(Node) * 0.5f));
	}

	ImVec2 GetAnimGraphNodeOutputPinPos(const FAnimGraphNodeDesc& Node, const ImVec2& CanvasOrigin)
	{
		return Add(CanvasOrigin, ImVec2(Node.Position.X + AnimGraphNodeWidth, Node.Position.Y + GetAnimGraphNodeHeight(Node) * 0.5f));
	}

	ImVec2 GetStateMachineEntryNodePos(const ImVec2& CanvasOrigin)
	{
		return Add(CanvasOrigin, ImVec2(36.0f, 84.0f));
	}

	int32 FindStateIndexById(const FAnimStateMachineDesc& Machine, int32 StateId)
	{
		for (int32 Index = 0; Index < static_cast<int32>(Machine.States.size()); ++Index)
		{
			if (Machine.States[Index].StateId == StateId)
			{
				return Index;
			}
		}
		return -1;
	}

	ImVec2 GetStateMachineStateNodePos(const FAnimStateDesc& State, const ImVec2& CanvasOrigin, int32 StateIndex)
	{
		float X = State.Position.X;
		float Y = State.Position.Y;
		if (X == 0.0f && Y == 0.0f)
		{
			X = 190.0f + static_cast<float>(StateIndex % 3) * 190.0f;
			Y = 70.0f + static_cast<float>(StateIndex / 3) * 115.0f;
		}
		return Add(CanvasOrigin, ImVec2(X, Y));
	}

	ImVec2 GetStateMachineStateInputPinPos(const FAnimStateDesc& State, const ImVec2& CanvasOrigin, int32 StateIndex)
	{
		const ImVec2 Pos = GetStateMachineStateNodePos(State, CanvasOrigin, StateIndex);
		return Add(Pos, ImVec2(0.0f, StateMachineStateNodeHeight * 0.5f));
	}

	ImVec2 GetStateMachineStateOutputPinPos(const FAnimStateDesc& State, const ImVec2& CanvasOrigin, int32 StateIndex)
	{
		const ImVec2 Pos = GetStateMachineStateNodePos(State, CanvasOrigin, StateIndex);
		return Add(Pos, ImVec2(StateMachineStateNodeWidth, StateMachineStateNodeHeight * 0.5f));
	}
}

void FEditorAnimGraphWidget::Initialize(UEditorEngine* InEditorEngine)
{
	FEditorWidget::Initialize(InEditorEngine);
}

void FEditorAnimGraphWidget::Open(const FString& InPath)
{
	EditingPath = FPaths::Normalize(InPath);
	EditingAsset = FResourceManager::Get().LoadAnimGraph(EditingPath);
	SelectedNodeId = -1;
	DraggingOutputNodeId = -1;
	DraggingInputNodeId = -1;
	DraggingTransitionFromStateId = -1;
	DraggingTransitionToStateId = -1;
	ViewMode = EViewMode::AnimGraph;
	EditingStateMachineNodeId = -1;
	SelectedStateId = -1;
	SelectedTransitionIndex = -1;
	EditingStateNameKey = -1;
	StateNameEditBuffers.clear();
	bDirty = false;
	bOpen = EditingAsset != nullptr;
	if (EditingAsset)
	{
		const int32 PreviousRootNodeId = EditingAsset->RootNodeId;
		const bool bNodeIdsNormalized = NormalizeGraphNodeIds();
		NormalizeRootNode();
		bDirty = bDirty || bNodeIdsNormalized || PreviousRootNodeId != EditingAsset->RootNodeId;
	}

	if (!EditingAsset && EditorEngine)
	{
		EditorEngine->GetNotificationService().Warning("Failed to open anim graph.");
	}
}

void FEditorAnimGraphWidget::Close()
{
	EditingPath.clear();
	EditingAsset = nullptr;
	SelectedNodeId = -1;
	DraggingOutputNodeId = -1;
	DraggingInputNodeId = -1;
	DraggingTransitionFromStateId = -1;
	DraggingTransitionToStateId = -1;
	ViewMode = EViewMode::AnimGraph;
	EditingStateMachineNodeId = -1;
	SelectedStateId = -1;
	SelectedTransitionIndex = -1;
	EditingStateNameKey = -1;
	StateNameEditBuffers.clear();
	bDirty = false;
	bOpen = false;
}

void FEditorAnimGraphWidget::Reload()
{
	const FString PathToReload = EditingPath;
	if (PathToReload.empty())
	{
		return;
	}

	Open(PathToReload);
	if (bOpen && EditorEngine)
	{
		EditorEngine->GetNotificationService().Info("Anim graph reloaded.");
	}
}

void FEditorAnimGraphWidget::SaveAndReload()
{
	Save();
	if (!bDirty)
	{
		Reload();
	}
}

void FEditorAnimGraphWidget::Render(float DeltaTime)
{
	(void)DeltaTime;
	if (!bOpen || !EditingAsset)
	{
		return;
	}

	FString Title = "Anim Graph - " + GetFileName(EditingPath);
	if (bDirty)
	{
		Title += " *";
	}
	Title += "###AnimGraphEditor";

	if (!ImGui::Begin(Title.c_str(), &bOpen))
	{
		ImGui::End();
		return;
	}

	RenderEmbedded(DeltaTime);
	ImGui::End();
}

void FEditorAnimGraphWidget::RenderEmbedded(float DeltaTime)
{
	(void)DeltaTime;
	if (!EditingAsset)
	{
		ImGui::TextDisabled("No anim graph open.");
		return;
	}

	RenderToolbar();
	ImGui::Separator();

	const ImVec2 Available = ImGui::GetContentRegionAvail();
	const float SplitterWidth = 6.0f;
	const float MinDetailsWidth = 220.0f;
	const float MaxDetailsWidth = std::max(MinDetailsWidth, Available.x - 260.0f - SplitterWidth);
	DetailsPanelWidth = std::max(MinDetailsWidth, std::min(DetailsPanelWidth, MaxDetailsWidth));
	const float CanvasWidth = std::max(260.0f, Available.x - DetailsPanelWidth - SplitterWidth);

	if (ImGui::BeginChild("##AnimGraphCanvasPane", ImVec2(CanvasWidth, 0.0f), true))
	{
		RenderCanvas();
	}
	ImGui::EndChild();

	ImGui::SameLine(0.0f, 0.0f);
	ImGui::InvisibleButton("##AnimGraphDetailsSplitter", ImVec2(SplitterWidth, Available.y));
	if (ImGui::IsItemHovered() || ImGui::IsItemActive())
	{
		ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
	}
	if (ImGui::IsItemActive())
	{
		DetailsPanelWidth = std::max(MinDetailsWidth, std::min(DetailsPanelWidth - ImGui::GetIO().MouseDelta.x, MaxDetailsWidth));
	}

	ImGui::SameLine(0.0f, 0.0f);

	if (ImGui::BeginChild("##AnimGraphDetailsPane", ImVec2(DetailsPanelWidth, 0.0f), true, ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_HorizontalScrollbar))
	{
		RenderDetails();
	}
	ImGui::EndChild();
}

void FEditorAnimGraphWidget::RenderToolbar()
{
	if (ImGui::Button("Save"))
	{
		Save();
	}
	ImGui::SameLine();
	if (ImGui::Button("Reload"))
	{
		Reload();
	}
	ImGui::SameLine();
	if (ImGui::Button("Save + Reload"))
	{
		SaveAndReload();
	}

	if (ViewMode == EViewMode::StateMachine)
	{
		FAnimGraphNodeDesc* MachineNode = FindEditingStateMachineNode();

		ImGui::SameLine();
		ImGui::TextDisabled("|");
		ImGui::SameLine();
		if (ImGui::Button("< Back"))
		{
			LeaveStateMachineView();
		}

		if (MachineNode)
		{
			ImGui::SameLine();
			ImGui::TextDisabled("AnimGraph > %s #%d", GetNodeDisplayName(*MachineNode).c_str(), MachineNode->NodeId);
			ImGui::SameLine();
			if (ImGui::Button("Add State"))
			{
				AddStateToStateMachine(MachineNode->StateMachine);
				bDirty = true;
			}

			ImGui::Spacing();
			ImGui::TextDisabled("%s%s", EditingPath.c_str(), bDirty ? " *" : "");
			ImGui::SameLine();
			ImGui::TextDisabled(
				"StateMachine #%d | Entry: %s | States: %d | Transitions: %d | Dirty: %s",
				MachineNode->NodeId,
				GetStateDisplayName(MachineNode->StateMachine, MachineNode->StateMachine.EntryStateId).c_str(),
				static_cast<int32>(MachineNode->StateMachine.States.size()),
				static_cast<int32>(MachineNode->StateMachine.Transitions.size()),
				bDirty ? "true" : "false");
		}
		else
		{
			ImGui::SameLine();
			ImGui::TextDisabled("Missing StateMachine");
		}
		return;
	}

	ImGui::SameLine();
	ImGui::TextDisabled("|");
	ImGui::SameLine();
	if (ImGui::Button("Add Sequence"))
	{
		AddSequencePlayerNode();
	}
	ImGui::SameLine();
	if (ImGui::Button("Add StateMachine"))
	{
		AddStateMachineNode();
	}
	ImGui::SameLine();
	if (ImGui::Button("Add Output"))
	{
		AddOutputPoseNode();
	}
	ImGui::SameLine();
	ImGui::BeginDisabled(SelectedNodeId < 0);
	if (ImGui::Button("Delete"))
	{
		DeleteSelectedNode();
	}
	ImGui::EndDisabled();

	const FAnimGraphNodeDesc* RootOutput = FindRootOutputPoseNode();
	const int32 RootNodeId = RootOutput ? RootOutput->NodeId : (EditingAsset ? EditingAsset->RootNodeId : -1);
	const int32 NodeCount = EditingAsset ? static_cast<int32>(EditingAsset->Nodes.size()) : 0;

	ImGui::Spacing();
	ImGui::TextDisabled("%s%s", EditingPath.c_str(), bDirty ? " *" : "");
	ImGui::SameLine();
	ImGui::TextDisabled(
		"Root: OutputPose #%d | Nodes: %d | States: %d | Transitions: %d | Dirty: %s",
		RootNodeId,
		NodeCount,
		CountStateMachineStates(),
		CountStateMachineTransitions(),
		bDirty ? "true" : "false");
}

void FEditorAnimGraphWidget::RenderCanvas()
{
	if (ViewMode == EViewMode::StateMachine)
	{
		RenderStateMachineCanvas();
		return;
	}

	RenderAnimGraphCanvas();
}

void FEditorAnimGraphWidget::RenderAnimGraphCanvas()
{
	const ImVec2 CanvasOrigin = ImGui::GetCursorScreenPos();
	const ImVec2 CanvasSize = ImGui::GetContentRegionAvail();
	ImDrawList* DrawList = ImGui::GetWindowDrawList();

	DrawList->AddRectFilled(
		CanvasOrigin,
		Add(CanvasOrigin, CanvasSize),
		ImGui::GetColorU32(ImVec4(0.10f, 0.11f, 0.13f, 1.0f)),
		4.0f);

	ImGui::SetNextItemAllowOverlap();
	ImGui::InvisibleButton(
		"##AnimGraphCanvas",
		CanvasSize,
		ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);

	if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
	{
		SelectedNodeId = -1;
	}

	if (ImGui::BeginPopupContextItem("##AnimGraphCanvasContext"))
	{
		if (ImGui::MenuItem("Add Sequence Player"))
		{
			AddSequencePlayerNode();
		}
		if (ImGui::MenuItem("Add State Machine"))
		{
			AddStateMachineNode();
		}
		if (ImGui::MenuItem("Add Output Pose"))
		{
			AddOutputPoseNode();
		}
		ImGui::EndPopup();
	}

	RenderLinks(CanvasOrigin);

	for (int32 NodeIndex = 0; NodeIndex < static_cast<int32>(EditingAsset->Nodes.size()); ++NodeIndex)
	{
		RenderNode(EditingAsset->Nodes[NodeIndex], CanvasOrigin, NodeIndex);
	}

	RenderPendingLink(CanvasOrigin);
}

void FEditorAnimGraphWidget::RenderStateMachineCanvas()
{
	FAnimGraphNodeDesc* MachineNode = FindEditingStateMachineNode();
	if (!MachineNode || MachineNode->Type != EAnimGraphNodeType::StateMachine)
	{
		LeaveStateMachineView();
		ImGui::TextDisabled("StateMachine node is missing.");
		return;
	}

	const ImVec2 CanvasOrigin = ImGui::GetCursorScreenPos();
	const ImVec2 CanvasSize = ImGui::GetContentRegionAvail();
	ImDrawList* DrawList = ImGui::GetWindowDrawList();

	DrawList->AddRectFilled(
		CanvasOrigin,
		Add(CanvasOrigin, CanvasSize),
		ImGui::GetColorU32(ImVec4(0.10f, 0.09f, 0.12f, 1.0f)),
		4.0f);

	const bool bHasSelectedEditorPanel = SelectedStateId >= 0 || SelectedTransitionIndex >= 0;
	const float SelectedEditorPanelWidth = SelectedStateId >= 0 ? 330.0f : 310.0f;
	const float SelectedEditorPanelHeight = std::min(SelectedStateId >= 0 ? 430.0f : 360.0f, std::max(180.0f, CanvasSize.y - 58.0f));
	const ImVec2 SelectedEditorPanelMin = Add(CanvasOrigin, ImVec2(std::max(12.0f, CanvasSize.x - SelectedEditorPanelWidth - 14.0f), 44.0f));
	const ImVec2 SelectedEditorPanelMax = Add(SelectedEditorPanelMin, ImVec2(SelectedEditorPanelWidth, SelectedEditorPanelHeight));
	const bool bMouseOverSelectedEditorPanel = bHasSelectedEditorPanel
		&& ImGui::IsMouseHoveringRect(SelectedEditorPanelMin, SelectedEditorPanelMax, true);

	ImGui::SetNextItemAllowOverlap();
	ImGui::InvisibleButton(
		"##StateMachineCanvas",
		CanvasSize,
		ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);

	if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !bMouseOverSelectedEditorPanel)
	{
		SelectedStateId = -1;
		SelectedTransitionIndex = -1;
		SelectedNodeId = MachineNode->NodeId;
	}

	if (ImGui::BeginPopupContextItem("##StateMachineCanvasContext"))
	{
		if (ImGui::MenuItem("Add State"))
		{
			AddStateToStateMachine(MachineNode->StateMachine);
			bDirty = true;
		}
		ImGui::EndPopup();
	}

	DrawList->AddText(
		Add(CanvasOrigin, ImVec2(12.0f, 12.0f)),
		ImGui::GetColorU32(ImGuiCol_TextDisabled),
		"StateMachine View - drag states, connect Out -> In or In -> Out. Arrow shows transition direction.");

	if (!ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Delete, false))
	{
		if (SelectedTransitionIndex >= 0)
		{
			if (DeleteTransitionFromStateMachine(MachineNode->StateMachine, SelectedTransitionIndex))
			{
				bDirty = true;
			}
		}
		else if (SelectedStateId >= 0)
		{
			if (DeleteStateFromStateMachine(MachineNode->StateMachine, SelectedStateId))
			{
				bDirty = true;
			}
		}
	}

	RenderStateMachineLinks(*MachineNode, CanvasOrigin);

	const ImVec2 EntryPos = GetStateMachineEntryNodePos(CanvasOrigin);
	const ImVec2 EntryMax = Add(EntryPos, ImVec2(StateMachineEntryNodeWidth, StateMachineEntryNodeHeight));
	DrawList->AddRectFilled(EntryPos, EntryMax, ImGui::GetColorU32(ImVec4(0.28f, 0.28f, 0.18f, 1.0f)), 6.0f);
	DrawList->AddRect(EntryPos, EntryMax, ImGui::GetColorU32(ImVec4(0.70f, 0.64f, 0.32f, 1.0f)), 6.0f);
	DrawList->AddText(Add(EntryPos, ImVec2(12.0f, 8.0f)), ImGui::GetColorU32(ImGuiCol_Text), "Entry");
	DrawList->AddText(Add(EntryPos, ImVec2(12.0f, 27.0f)), ImGui::GetColorU32(ImGuiCol_TextDisabled), GetStateDisplayName(MachineNode->StateMachine, MachineNode->StateMachine.EntryStateId).c_str());
	DrawList->AddCircleFilled(
		Add(EntryPos, ImVec2(StateMachineEntryNodeWidth, StateMachineEntryNodeHeight * 0.5f)),
		StateMachineNodePinRadius,
		ImGui::GetColorU32(ImVec4(1.0f, 0.82f, 0.35f, 1.0f)));

	for (int32 StateIndex = 0; StateIndex < static_cast<int32>(MachineNode->StateMachine.States.size()); ++StateIndex)
	{
		RenderStateMachineStateNode(*MachineNode, MachineNode->StateMachine.States[StateIndex], CanvasOrigin, StateIndex);
	}

	RenderPendingStateMachineTransitionLink(*MachineNode, CanvasOrigin);
	RenderSelectedStateMachineStateEditor(*MachineNode, CanvasOrigin, CanvasSize);
	RenderSelectedStateMachineTransitionEditor(*MachineNode, CanvasOrigin, CanvasSize);
}

void FEditorAnimGraphWidget::RenderStateMachineLinks(FAnimGraphNodeDesc& MachineNode, const ImVec2& CanvasOrigin)
{
	FAnimStateMachineDesc& Machine = MachineNode.StateMachine;
	ImDrawList* DrawList = ImGui::GetWindowDrawList();

	const int32 EntryStateIndex = FindStateIndexById(Machine, Machine.EntryStateId);
	if (EntryStateIndex >= 0)
	{
		const FAnimStateDesc& EntryState = Machine.States[EntryStateIndex];
		const ImVec2 EntryPos = GetStateMachineEntryNodePos(CanvasOrigin);
		const ImVec2 From = Add(EntryPos, ImVec2(StateMachineEntryNodeWidth, StateMachineEntryNodeHeight * 0.5f));
		const ImVec2 To = GetStateMachineStateInputPinPos(EntryState, CanvasOrigin, EntryStateIndex);
		const ImU32 EntryLinkColor = ImGui::GetColorU32(ImVec4(1.0f, 0.82f, 0.35f, 1.0f));
		const ImVec2 EntryControlB = Sub(To, ImVec2(70.0f, 0.0f));
		DrawList->AddBezierCubic(From, Add(From, ImVec2(70.0f, 0.0f)), EntryControlB, To, EntryLinkColor, 3.0f);
		DrawArrowHead(DrawList, To, Sub(To, EntryControlB), EntryLinkColor, 16.0f);
	}

	int32 ClickedTransitionIndex = -1;
	const bool bCanSelectTransition = DraggingTransitionFromStateId < 0 && DraggingTransitionToStateId < 0 && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
	const ImVec2 MousePos = ImGui::GetIO().MousePos;

	for (int32 TransitionIndex = 0; TransitionIndex < static_cast<int32>(Machine.Transitions.size()); ++TransitionIndex)
	{
		const FAnimStateTransitionDesc& Transition = Machine.Transitions[TransitionIndex];
		const int32 FromIndex = FindStateIndexById(Machine, Transition.FromStateId);
		const int32 ToIndex = FindStateIndexById(Machine, Transition.ToStateId);
		if (FromIndex < 0 || ToIndex < 0)
		{
			continue;
		}

		const FAnimStateDesc& FromState = Machine.States[FromIndex];
		const FAnimStateDesc& ToState = Machine.States[ToIndex];
		const ImVec2 From = GetStateMachineStateOutputPinPos(FromState, CanvasOrigin, FromIndex);
		const ImVec2 To = GetStateMachineStateInputPinPos(ToState, CanvasOrigin, ToIndex);
		const ImVec2 ControlA = Add(From, ImVec2(80.0f, 0.0f));
		const ImVec2 ControlB = Sub(To, ImVec2(80.0f, 0.0f));
		const bool bSelectedTransition = SelectedTransitionIndex == TransitionIndex;
		const bool bRelatedToSelectedState = SelectedStateId == Transition.FromStateId || SelectedStateId == Transition.ToStateId;
		const ImU32 LinkColor = ImGui::GetColorU32(
			bSelectedTransition
			? ImVec4(1.0f, 0.82f, 0.35f, 1.0f)
			: bRelatedToSelectedState
			? ImVec4(0.95f, 0.72f, 1.0f, 1.0f)
			: ImVec4(0.72f, 0.60f, 0.95f, 1.0f));

		DrawList->AddBezierCubic(
			From,
			ControlA,
			ControlB,
			To,
			LinkColor,
			bSelectedTransition ? 4.0f : (bRelatedToSelectedState ? 3.5f : 2.5f));
		DrawArrowHead(DrawList, To, Sub(To, ControlB), LinkColor, bSelectedTransition ? 20.0f : 16.0f);

		if (bCanSelectTransition && DistanceSquaredToBezier(MousePos, From, ControlA, ControlB, To) <= 14.0f * 14.0f)
		{
			ClickedTransitionIndex = TransitionIndex;
		}
	}

	if (ClickedTransitionIndex >= 0)
	{
		SelectedNodeId = MachineNode.NodeId;
		SelectedStateId = -1;
		SelectedTransitionIndex = ClickedTransitionIndex;
	}
}

void FEditorAnimGraphWidget::RenderPendingStateMachineTransitionLink(FAnimGraphNodeDesc& MachineNode, const ImVec2& CanvasOrigin)
{
	if (DraggingTransitionFromStateId < 0 && DraggingTransitionToStateId < 0)
	{
		return;
	}

	FAnimStateMachineDesc& Machine = MachineNode.StateMachine;
	const ImVec2 MousePos = ImGui::GetIO().MousePos;
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	const ImU32 LinkColor = ImGui::GetColorU32(ImVec4(1.0f, 0.82f, 0.35f, 1.0f));

	if (DraggingTransitionFromStateId >= 0)
	{
		const int32 FromIndex = FindStateIndexById(Machine, DraggingTransitionFromStateId);
		if (FromIndex < 0)
		{
			DraggingTransitionFromStateId = -1;
			return;
		}

		const FAnimStateDesc& FromState = Machine.States[FromIndex];
		const ImVec2 From = GetStateMachineStateOutputPinPos(FromState, CanvasOrigin, FromIndex);
		const ImVec2 ControlB = Sub(MousePos, ImVec2(80.0f, 0.0f));
		DrawList->AddBezierCubic(From, Add(From, ImVec2(80.0f, 0.0f)), ControlB, MousePos, LinkColor, 3.0f);
		DrawArrowHead(DrawList, MousePos, Sub(MousePos, ControlB), LinkColor, 16.0f);
	}
	else if (DraggingTransitionToStateId >= 0)
	{
		const int32 ToIndex = FindStateIndexById(Machine, DraggingTransitionToStateId);
		if (ToIndex < 0)
		{
			DraggingTransitionToStateId = -1;
			return;
		}

		const FAnimStateDesc& ToState = Machine.States[ToIndex];
		const ImVec2 To = GetStateMachineStateInputPinPos(ToState, CanvasOrigin, ToIndex);
		const ImVec2 ControlB = Sub(To, ImVec2(80.0f, 0.0f));
		DrawList->AddBezierCubic(MousePos, Add(MousePos, ImVec2(80.0f, 0.0f)), ControlB, To, LinkColor, 3.0f);
		DrawArrowHead(DrawList, To, Sub(To, ControlB), LinkColor, 16.0f);
	}

	if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
	{
		DraggingTransitionFromStateId = -1;
		DraggingTransitionToStateId = -1;
	}
}

void FEditorAnimGraphWidget::RenderSelectedStateMachineStateEditor(FAnimGraphNodeDesc& MachineNode, const ImVec2& CanvasOrigin, const ImVec2& CanvasSize)
{
	FAnimStateMachineDesc& Machine = MachineNode.StateMachine;
	const int32 StateIndex = FindStateIndexById(Machine, SelectedStateId);
	if (StateIndex < 0)
	{
		SelectedStateId = -1;
		return;
	}

	FAnimStateDesc& State = Machine.States[StateIndex];
	const float PanelWidth = 330.0f;
	const float PanelHeight = std::min(430.0f, std::max(180.0f, CanvasSize.y - 58.0f));
	ImGui::SetCursorScreenPos(Add(CanvasOrigin, ImVec2(std::max(12.0f, CanvasSize.x - PanelWidth - 14.0f), 44.0f)));
	ImGui::SetNextItemAllowOverlap();
	ImGui::PushID("SelectedStateMachineStateEditor");
	if (ImGui::BeginChild("##SelectedStateEditor", ImVec2(PanelWidth, PanelHeight), true, ImGuiWindowFlags_AlwaysVerticalScrollbar))
	{
		ImGui::TextUnformatted("Selected State");
		ImGui::SameLine();
		ImGui::TextDisabled("#%d", State.StateId);
		ImGui::Separator();

		if (RenderStateNameInput("Name", MachineNode.NodeId, State))
		{
			bDirty = true;
		}

		if (RenderAnimationPathCombo("Animation", State.AnimationPath))
		{
			bDirty = true;
		}

		if (ImGui::DragFloat2("Position", &State.Position.X, 1.0f))
		{
			bDirty = true;
		}

		const bool bIsEntry = Machine.EntryStateId == State.StateId;
		if (bIsEntry)
		{
			ImGui::TextDisabled("Entry State");
		}
		else if (ImGui::Button("Set As Entry"))
		{
			Machine.EntryStateId = State.StateId;
			bDirty = true;
		}

		ImGui::SeparatorText("Outgoing");
		bool bHasOutgoing = false;
		int32 TransitionToDelete = -1;
		for (int32 TransitionIndex = 0; TransitionIndex < static_cast<int32>(Machine.Transitions.size()); ++TransitionIndex)
		{
			const FAnimStateTransitionDesc& Transition = Machine.Transitions[TransitionIndex];
			if (Transition.FromStateId != State.StateId)
			{
				continue;
			}

			bHasOutgoing = true;
			ImGui::PushID(TransitionIndex);
			ImGui::Text("-> %s", GetStateDisplayName(Machine, Transition.ToStateId).c_str());
			ImGui::SameLine();
			if (ImGui::SmallButton("Select"))
			{
				SelectedTransitionIndex = TransitionIndex;
				SelectedStateId = -1;
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("Delete"))
			{
				TransitionToDelete = TransitionIndex;
			}
			ImGui::PopID();
		}
		if (!bHasOutgoing)
		{
			ImGui::TextDisabled("No outgoing transitions.");
		}

		ImGui::SeparatorText("Incoming");
		bool bHasIncoming = false;
		for (int32 TransitionIndex = 0; TransitionIndex < static_cast<int32>(Machine.Transitions.size()); ++TransitionIndex)
		{
			const FAnimStateTransitionDesc& Transition = Machine.Transitions[TransitionIndex];
			if (Transition.ToStateId != State.StateId)
			{
				continue;
			}

			bHasIncoming = true;
			ImGui::PushID(TransitionIndex + 10000);
			ImGui::Text("<- %s", GetStateDisplayName(Machine, Transition.FromStateId).c_str());
			ImGui::SameLine();
			if (ImGui::SmallButton("Select"))
			{
				SelectedTransitionIndex = TransitionIndex;
				SelectedStateId = -1;
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("Delete"))
			{
				TransitionToDelete = TransitionIndex;
			}
			ImGui::PopID();
		}
		if (!bHasIncoming)
		{
			ImGui::TextDisabled("No incoming transitions.");
		}

		if (TransitionToDelete >= 0)
		{
			if (DeleteTransitionFromStateMachine(Machine, TransitionToDelete))
			{
				SelectedStateId = State.StateId;
				bDirty = true;
			}
		}

		ImGui::Separator();
		if (ImGui::Button("Delete State"))
		{
			if (DeleteStateFromStateMachine(Machine, State.StateId))
			{
				bDirty = true;
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Close"))
		{
			SelectedStateId = -1;
		}
	}
	ImGui::EndChild();
	ImGui::PopID();
}

void FEditorAnimGraphWidget::RenderSelectedStateMachineTransitionEditor(FAnimGraphNodeDesc& MachineNode, const ImVec2& CanvasOrigin, const ImVec2& CanvasSize)
{
	FAnimStateMachineDesc& Machine = MachineNode.StateMachine;
	if (SelectedTransitionIndex < 0 || SelectedTransitionIndex >= static_cast<int32>(Machine.Transitions.size()))
	{
		return;
	}

	FAnimStateTransitionDesc& Transition = Machine.Transitions[SelectedTransitionIndex];
	const float PanelWidth = 310.0f;
	const float PanelHeight = std::min(360.0f, std::max(180.0f, CanvasSize.y - 58.0f));
	ImGui::SetCursorScreenPos(Add(CanvasOrigin, ImVec2(std::max(12.0f, CanvasSize.x - PanelWidth - 14.0f), 44.0f)));
	ImGui::SetNextItemAllowOverlap();
	ImGui::PushID("SelectedStateMachineTransitionEditor");
	if (ImGui::BeginChild("##SelectedTransitionEditor", ImVec2(PanelWidth, PanelHeight), true, ImGuiWindowFlags_AlwaysVerticalScrollbar))
	{
		ImGui::TextUnformatted("Selected Transition");
		ImGui::SameLine();
		ImGui::TextDisabled("#%d", SelectedTransitionIndex);
		ImGui::Separator();

		if (ImGui::BeginCombo("From", GetStateDisplayName(Machine, Transition.FromStateId).c_str()))
		{
			for (const FAnimStateDesc& State : Machine.States)
			{
				const FString Label = GetStateDisplayName(Machine, State.StateId);
				const bool bSelected = Transition.FromStateId == State.StateId;
				if (ImGui::Selectable(Label.c_str(), bSelected))
				{
					if (State.StateId != Transition.ToStateId && !HasTransition(Machine, State.StateId, Transition.ToStateId))
					{
						Transition.FromStateId = State.StateId;
						bDirty = true;
					}
				}
				if (bSelected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		if (ImGui::BeginCombo("To", GetStateDisplayName(Machine, Transition.ToStateId).c_str()))
		{
			for (const FAnimStateDesc& State : Machine.States)
			{
				const FString Label = GetStateDisplayName(Machine, State.StateId);
				const bool bSelected = Transition.ToStateId == State.StateId;
				if (ImGui::Selectable(Label.c_str(), bSelected))
				{
					if (State.StateId != Transition.FromStateId && !HasTransition(Machine, Transition.FromStateId, State.StateId))
					{
						Transition.ToStateId = State.StateId;
						bDirty = true;
					}
				}
				if (bSelected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		if (ImGui::DragFloat("Blend", &Transition.BlendTime, 0.01f, 0.0f, 5.0f))
		{
			bDirty = true;
		}
		if (ImGui::InputInt("Priority", &Transition.Priority))
		{
			bDirty = true;
		}

		const char* ConditionLabels[] = {
			"AlwaysTrue",
			"BoolParameter",
			"FloatGreater",
			"FloatLess",
			"LuaFunction"
		};
		int32 ConditionIndex = static_cast<int32>(Transition.Condition.Type);
		if (ConditionIndex < 0 || ConditionIndex >= static_cast<int32>(std::size(ConditionLabels)))
		{
			ConditionIndex = 0;
		}
		if (ImGui::Combo("Condition", &ConditionIndex, ConditionLabels, static_cast<int32>(std::size(ConditionLabels))))
		{
			Transition.Condition.Type = static_cast<EAnimTransitionConditionType>(ConditionIndex);
			bDirty = true;
		}

		if (Transition.Condition.Type == EAnimTransitionConditionType::BoolParameter
			|| Transition.Condition.Type == EAnimTransitionConditionType::FloatGreater
			|| Transition.Condition.Type == EAnimTransitionConditionType::FloatLess)
		{
			char ParameterBuffer[128] = {};
			std::strncpy(ParameterBuffer, Transition.Condition.ParameterName.c_str(), sizeof(ParameterBuffer) - 1);
			if (ImGui::InputText("Parameter", ParameterBuffer, sizeof(ParameterBuffer)))
			{
				Transition.Condition.ParameterName = ParameterBuffer;
				bDirty = true;
			}
		}

		if (Transition.Condition.Type == EAnimTransitionConditionType::BoolParameter)
		{
			if (ImGui::Checkbox("Bool Value", &Transition.Condition.BoolValue))
			{
				bDirty = true;
			}
		}
		else if (Transition.Condition.Type == EAnimTransitionConditionType::FloatGreater
			|| Transition.Condition.Type == EAnimTransitionConditionType::FloatLess)
		{
			if (ImGui::DragFloat("Threshold", &Transition.Condition.Threshold, 0.1f))
			{
				bDirty = true;
			}
		}
		else if (Transition.Condition.Type == EAnimTransitionConditionType::LuaFunction)
		{
			char LuaFunctionBuffer[128] = {};
			std::strncpy(LuaFunctionBuffer, Transition.Condition.LuaFunctionName.c_str(), sizeof(LuaFunctionBuffer) - 1);
			if (ImGui::InputText("Lua Function", LuaFunctionBuffer, sizeof(LuaFunctionBuffer)))
			{
				Transition.Condition.LuaFunctionName = LuaFunctionBuffer;
				bDirty = true;
			}
		}

		if (ImGui::Button("Delete Transition"))
		{
			if (DeleteTransitionFromStateMachine(Machine, SelectedTransitionIndex))
			{
				bDirty = true;
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Close"))
		{
			SelectedTransitionIndex = -1;
		}
	}
	ImGui::EndChild();
	ImGui::PopID();
}

void FEditorAnimGraphWidget::RenderStateMachineStateNode(FAnimGraphNodeDesc& MachineNode, FAnimStateDesc& State, const ImVec2& CanvasOrigin, int32 StateIndex)
{
	const ImVec2 NodePos = GetStateMachineStateNodePos(State, CanvasOrigin, StateIndex);
	const ImVec2 NodeSize = ImVec2(StateMachineStateNodeWidth, StateMachineStateNodeHeight);
	const ImVec2 NodeMax = Add(NodePos, NodeSize);
	const ImVec2 InputPin = Add(NodePos, ImVec2(0.0f, StateMachineStateNodeHeight * 0.5f));
	const ImVec2 OutputPin = Add(NodePos, ImVec2(StateMachineStateNodeWidth, StateMachineStateNodeHeight * 0.5f));
	const bool bSelected = SelectedStateId == State.StateId;
	const bool bEntry = MachineNode.StateMachine.EntryStateId == State.StateId;
	const bool bInputPinHovered = IsMouseNear(InputPin, AnimGraphPinHitRadius);
	const bool bOutputPinHovered = IsMouseNear(OutputPin, AnimGraphPinHitRadius);
	const bool bDraggingFromThisOutput = DraggingTransitionFromStateId == State.StateId;
	const bool bDraggingToThisInput = DraggingTransitionToStateId == State.StateId;
	const bool bValidInputDrop = DraggingTransitionFromStateId >= 0 && DraggingTransitionFromStateId != State.StateId && bInputPinHovered;
	const bool bValidOutputDrop = DraggingTransitionToStateId >= 0 && DraggingTransitionToStateId != State.StateId && bOutputPinHovered;
	const bool bAnyTransitionDrag = DraggingTransitionFromStateId >= 0 || DraggingTransitionToStateId >= 0;

	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	const ImU32 BodyColor = ImGui::GetColorU32(bSelected ? ImVec4(0.30f, 0.24f, 0.44f, 1.0f) : ImVec4(0.22f, 0.20f, 0.28f, 1.0f));
	const ImU32 HeaderColor = ImGui::GetColorU32(bEntry ? ImVec4(0.38f, 0.32f, 0.18f, 1.0f) : ImVec4(0.35f, 0.25f, 0.45f, 1.0f));
	DrawList->AddRectFilled(NodePos, NodeMax, BodyColor, 6.0f);
	DrawList->AddRectFilled(NodePos, ImVec2(NodeMax.x, NodePos.y + StateMachineNodeHeaderHeight), HeaderColor, 6.0f);
	DrawList->AddRect(NodePos, NodeMax, ImGui::GetColorU32(bSelected ? ImVec4(1.0f, 0.82f, 0.35f, 1.0f) : ImVec4(0.55f, 0.45f, 0.70f, 1.0f)), 6.0f, 0, bSelected ? 2.0f : 1.0f);

	ImGui::SetCursorScreenPos(NodePos);
	char StateButtonId[96];
	std::snprintf(StateButtonId, sizeof(StateButtonId), "##StateMachineState_%d_%d", MachineNode.NodeId, State.StateId);
	ImGui::InvisibleButton(StateButtonId, NodeSize, ImGuiButtonFlags_MouseButtonLeft);
	if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !bInputPinHovered && !bOutputPinHovered)
	{
		SelectedNodeId = MachineNode.NodeId;
		SelectedStateId = State.StateId;
		SelectedTransitionIndex = -1;
		if (State.Position.X == 0.0f && State.Position.Y == 0.0f)
		{
			State.Position.X = NodePos.x - CanvasOrigin.x;
			State.Position.Y = NodePos.y - CanvasOrigin.y;
		}
	}

	if (ImGui::IsItemActive() && !bInputPinHovered && !bOutputPinHovered && !bAnyTransitionDrag && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
	{
		const ImVec2 Delta = ImGui::GetIO().MouseDelta;
		if (State.Position.X == 0.0f && State.Position.Y == 0.0f)
		{
			State.Position.X = NodePos.x - CanvasOrigin.x;
			State.Position.Y = NodePos.y - CanvasOrigin.y;
		}
		State.Position.X += Delta.x;
		State.Position.Y += Delta.y;
		bDirty = true;
	}

	if (bOutputPinHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		SelectedNodeId = MachineNode.NodeId;
		SelectedStateId = State.StateId;
		SelectedTransitionIndex = -1;
		DraggingTransitionFromStateId = State.StateId;
		DraggingTransitionToStateId = -1;
	}

	if (bInputPinHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		SelectedNodeId = MachineNode.NodeId;
		SelectedStateId = State.StateId;
		SelectedTransitionIndex = -1;
		DraggingTransitionToStateId = State.StateId;
		DraggingTransitionFromStateId = -1;
	}

	if (bValidInputDrop && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
	{
		if (AddTransitionToStateMachine(MachineNode.StateMachine, DraggingTransitionFromStateId, State.StateId))
		{
			SelectedNodeId = MachineNode.NodeId;
			SelectedStateId = -1;
			SelectedTransitionIndex = static_cast<int32>(MachineNode.StateMachine.Transitions.size()) - 1;
			bDirty = true;
		}
		DraggingTransitionFromStateId = -1;
		DraggingTransitionToStateId = -1;
	}
	else if (bValidOutputDrop && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
	{
		if (AddTransitionToStateMachine(MachineNode.StateMachine, State.StateId, DraggingTransitionToStateId))
		{
			SelectedNodeId = MachineNode.NodeId;
			SelectedStateId = -1;
			SelectedTransitionIndex = static_cast<int32>(MachineNode.StateMachine.Transitions.size()) - 1;
			bDirty = true;
		}
		DraggingTransitionFromStateId = -1;
		DraggingTransitionToStateId = -1;
	}

	DrawList->PushClipRect(Add(NodePos, ImVec2(8.0f, 2.0f)), Sub(NodeMax, ImVec2(8.0f, 2.0f)), true);
	const float StateTextWidth = StateMachineStateNodeWidth - 18.0f;
	const FString StateTitle = FitTextToWidth(GetStateDisplayName(MachineNode.StateMachine, State.StateId), StateTextWidth);
	DrawList->AddText(Add(NodePos, ImVec2(9.0f, 4.0f)), ImGui::GetColorU32(ImGuiCol_Text), StateTitle.c_str());
	const FString AnimName = State.AnimationPath.empty() ? FString("<No Animation>") : GetFileName(State.AnimationPath);
	const FString FittedAnimName = FitTextToWidth(AnimName, StateTextWidth);
	DrawList->AddText(Add(NodePos, ImVec2(9.0f, 28.0f)), ImGui::GetColorU32(ImGuiCol_TextDisabled), FittedAnimName.c_str());
	if (bEntry)
	{
		DrawList->AddText(Add(NodePos, ImVec2(9.0f, 48.0f)), ImGui::GetColorU32(ImVec4(1.0f, 0.82f, 0.35f, 1.0f)), "Entry State");
	}
	else
	{
		DrawList->AddText(Add(NodePos, ImVec2(9.0f, 48.0f)), ImGui::GetColorU32(ImGuiCol_TextDisabled), "Drag to move");
	}
	DrawList->PopClipRect();

	const ImU32 PinOutlineColor = ImGui::GetColorU32(ImVec4(0.12f, 0.10f, 0.16f, 1.0f));
	const ImU32 PinColor = ImGui::GetColorU32(ImVec4(0.78f, 0.76f, 1.0f, 1.0f));
	const ImU32 PinHoverColor = ImGui::GetColorU32(ImVec4(1.0f, 0.82f, 0.35f, 1.0f));
	const bool bInputHighlighted = bInputPinHovered || bValidInputDrop || bDraggingToThisInput;
	const bool bOutputHighlighted = bOutputPinHovered || bValidOutputDrop || bDraggingFromThisOutput;
	DrawList->AddCircleFilled(InputPin, StateMachineNodePinRadius + (bInputHighlighted ? 2.0f : 0.0f), bInputHighlighted ? PinHoverColor : PinColor);
	DrawList->AddCircle(InputPin, StateMachineNodePinRadius + 1.0f, PinOutlineColor, 12, 1.5f);
	DrawList->AddText(Sub(InputPin, ImVec2(24.0f, 19.0f)), ImGui::GetColorU32(ImGuiCol_TextDisabled), "In");
	DrawList->AddCircleFilled(OutputPin, StateMachineNodePinRadius + (bOutputHighlighted ? 2.0f : 0.0f), bOutputHighlighted ? PinHoverColor : PinColor);
	DrawList->AddCircle(OutputPin, StateMachineNodePinRadius + 1.0f, PinOutlineColor, 12, 1.5f);
	DrawList->AddText(Add(OutputPin, ImVec2(8.0f, -19.0f)), ImGui::GetColorU32(ImGuiCol_TextDisabled), "Out");
}

void FEditorAnimGraphWidget::RenderNode(FAnimGraphNodeDesc& Node, const ImVec2& CanvasOrigin, int32 NodeIndex)
{
	const ImVec2 NodePos = Add(CanvasOrigin, ToImVec2(Node.Position));
	const ImVec2 NodeSize = GetAnimGraphNodeSize(Node);
	const ImVec2 NodeMax = Add(NodePos, NodeSize);
	const bool bSelected = SelectedNodeId == Node.NodeId;

	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	const ImU32 BodyColor = ImGui::GetColorU32(
		bSelected ? ImVec4(0.22f, 0.35f, 0.58f, 1.0f) : ImVec4(0.21f, 0.23f, 0.28f, 1.0f));
	const ImU32 HeaderColor = ImGui::GetColorU32(
		Node.Type == EAnimGraphNodeType::OutputPose
			? ImVec4(0.30f, 0.40f, 0.27f, 1.0f)
			: Node.Type == EAnimGraphNodeType::StateMachine
			? ImVec4(0.36f, 0.25f, 0.45f, 1.0f)
			: ImVec4(0.22f, 0.28f, 0.40f, 1.0f));

	DrawList->AddRectFilled(NodePos, NodeMax, BodyColor, 6.0f);
	DrawList->AddRectFilled(NodePos, ImVec2(NodeMax.x, NodePos.y + AnimGraphNodeHeaderHeight), HeaderColor, 6.0f);
	DrawList->AddRect(NodePos, NodeMax, ImGui::GetColorU32(ImVec4(0.48f, 0.55f, 0.68f, 1.0f)), 6.0f);

	if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::IsMouseHoveringRect(NodePos, NodeMax, true))
	{
		SelectedNodeId = Node.NodeId;
	}

	ImGui::SetCursorScreenPos(NodePos);
	char HeaderId[96];
	std::snprintf(HeaderId, sizeof(HeaderId), "##AnimGraphNodeHeader_%d_%d", Node.NodeId, NodeIndex);
	ImGui::InvisibleButton(HeaderId, ImVec2(NodeSize.x, AnimGraphNodeHeaderHeight));

	if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
	{
		SelectedNodeId = Node.NodeId;
	}

	if (Node.Type == EAnimGraphNodeType::StateMachine
		&& ImGui::IsItemHovered()
		&& ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
	{
		EnterStateMachineView(Node.NodeId);
	}

	if (bSelected && ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
	{
		const ImVec2 Delta = ImGui::GetIO().MouseDelta;
		Node.Position.X += Delta.x;
		Node.Position.Y += Delta.y;
		bDirty = true;
	}

	const FString TypeText = AnimGraphNodeTypeToString(Node.Type);
	const FString NameText = GetNodeDisplayName(Node);
	DrawList->AddText(Add(NodePos, ImVec2(10.0f, 6.0f)), ImGui::GetColorU32(ImGuiCol_Text), NameText.c_str());

	const FString NodeIdText = "#" + std::to_string(Node.NodeId);
	const ImVec2 NodeIdSize = ImGui::CalcTextSize(NodeIdText.c_str());
	DrawList->AddText(
		ImVec2(NodeMax.x - NodeIdSize.x - 10.0f, NodePos.y + 6.0f),
		ImGui::GetColorU32(ImGuiCol_TextDisabled),
		NodeIdText.c_str());

	DrawList->AddText(Add(NodePos, ImVec2(10.0f, 31.0f)), ImGui::GetColorU32(ImGuiCol_TextDisabled), TypeText.c_str());

	ImGui::PushID(Node.NodeId);
	ImGui::SetCursorScreenPos(Add(NodePos, ImVec2(10.0f, 50.0f)));
	// 노드 내부 Combo는 오른쪽에 표시되는 라벨이 핀/노드 경계를 침범하지 않도록
	// Details 패널보다 좁게 잡습니다.
	ImGui::PushItemWidth(AnimGraphNodeWidth - 90.0f);

	if (Node.Type == EAnimGraphNodeType::OutputPose)
	{
		if (RenderOutputPoseSourceCombo("Source", Node))
		{
			bDirty = true;
		}
	}
	else if (Node.Type == EAnimGraphNodeType::SequencePlayer)
	{
		if (RenderAnimationPathCombo("Animation", Node.AnimationPath, false))
		{
			bDirty = true;
		}

		ImGui::SetCursorScreenPos(Add(NodePos, ImVec2(10.0f, 80.0f)));
		ImGui::SetNextItemWidth(96.0f);
		if (ImGui::DragFloat("Play Rate", &Node.PlayRate, 0.01f, 0.0f, 10.0f))
		{
			bDirty = true;
		}

		ImGui::SetCursorScreenPos(Add(NodePos, ImVec2(10.0f, 110.0f)));
		if (ImGui::Checkbox("Loop", &Node.bLoop))
		{
			bDirty = true;
		}
	}
	else if (Node.Type == EAnimGraphNodeType::StateMachine)
	{
		if (RenderStateMachineEntryCombo("Entry", Node.StateMachine))
		{
			bDirty = true;
		}

		ImGui::SetCursorScreenPos(Add(NodePos, ImVec2(10.0f, 80.0f)));
		if (ImGui::SmallButton("+ State"))
		{
			AddStateToStateMachine(Node.StateMachine);
			bDirty = true;
		}

		ImGui::SameLine();
		ImGui::TextDisabled(
			"States: %d  Transitions: %d",
			static_cast<int32>(Node.StateMachine.States.size()),
			static_cast<int32>(Node.StateMachine.Transitions.size()));

		const float SummaryY = 110.0f;
		FString Summary = "Entry: " + GetStateDisplayName(Node.StateMachine, Node.StateMachine.EntryStateId);
		DrawList->AddText(Add(NodePos, ImVec2(10.0f, SummaryY)), ImGui::GetColorU32(ImGuiCol_TextDisabled), Summary.c_str());

		if (!Node.StateMachine.Transitions.empty())
		{
			const FAnimStateTransitionDesc& Transition = Node.StateMachine.Transitions.front();
			FString TransitionSummary = GetStateDisplayName(Node.StateMachine, Transition.FromStateId)
				+ " -> "
				+ GetStateDisplayName(Node.StateMachine, Transition.ToStateId);
			if (Node.StateMachine.Transitions.size() > 1)
			{
				TransitionSummary += " ...";
			}
			DrawList->AddText(Add(NodePos, ImVec2(10.0f, SummaryY + 20.0f)), ImGui::GetColorU32(ImGuiCol_TextDisabled), TransitionSummary.c_str());
		}
	}

	ImGui::PopItemWidth();
	ImGui::PopID();

	const ImU32 PinOutlineColor = ImGui::GetColorU32(ImVec4(0.12f, 0.15f, 0.20f, 1.0f));
	const ImU32 PinColor = ImGui::GetColorU32(ImVec4(0.72f, 0.82f, 1.0f, 1.0f));
	const ImU32 PinHoverColor = ImGui::GetColorU32(ImVec4(1.0f, 0.82f, 0.35f, 1.0f));

	if (Node.Type == EAnimGraphNodeType::OutputPose)
	{
		const ImVec2 InputPin = GetAnimGraphNodeInputPinPos(Node, CanvasOrigin);
		const bool bHoveredInputPin = IsMouseNear(InputPin, AnimGraphPinHitRadius);
		const bool bDraggingThisInputPin = DraggingInputNodeId == Node.NodeId;
		DrawList->AddCircleFilled(InputPin, AnimGraphPinRadius + ((bHoveredInputPin || bDraggingThisInputPin) ? 2.0f : 0.0f), (bHoveredInputPin || bDraggingThisInputPin) ? PinHoverColor : PinColor);
		DrawList->AddCircle(InputPin, AnimGraphPinRadius + 1.0f, PinOutlineColor, 12, 1.5f);
		DrawList->AddText(Sub(InputPin, ImVec2(24.0f, 19.0f)), ImGui::GetColorU32(ImGuiCol_TextDisabled), "In");

		if (bHoveredInputPin && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
		{
			SelectedNodeId = Node.NodeId;
			DraggingInputNodeId = Node.NodeId;
			DraggingOutputNodeId = -1;
		}
	}
	else
	{
		const ImVec2 OutputPin = GetAnimGraphNodeOutputPinPos(Node, CanvasOrigin);
		const bool bHoveredOutputPin = IsMouseNear(OutputPin, AnimGraphPinHitRadius);
		const bool bDraggingThisPin = DraggingOutputNodeId == Node.NodeId;
		DrawList->AddCircleFilled(OutputPin, AnimGraphPinRadius + ((bHoveredOutputPin || bDraggingThisPin) ? 2.0f : 0.0f), (bHoveredOutputPin || bDraggingThisPin) ? PinHoverColor : PinColor);
		DrawList->AddCircle(OutputPin, AnimGraphPinRadius + 1.0f, PinOutlineColor, 12, 1.5f);
		DrawList->AddText(Add(OutputPin, ImVec2(8.0f, -19.0f)), ImGui::GetColorU32(ImGuiCol_TextDisabled), "Out");

		if (bHoveredOutputPin && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
		{
			SelectedNodeId = Node.NodeId;
			DraggingOutputNodeId = Node.NodeId;
			DraggingInputNodeId = -1;
		}
	}
}

void FEditorAnimGraphWidget::RenderLinks(const ImVec2& CanvasOrigin)
{
	ImDrawList* DrawList = ImGui::GetWindowDrawList();

	for (const FAnimGraphNodeDesc& Node : EditingAsset->Nodes)
	{
		if (Node.InputPoseNodeId < 0)
		{
			continue;
		}

		const FAnimGraphNodeDesc* InputNode = EditingAsset->FindNode(Node.InputPoseNodeId);
		if (!InputNode)
		{
			continue;
		}

		const ImVec2 From = GetAnimGraphNodeOutputPinPos(*InputNode, CanvasOrigin);
		const ImVec2 To = GetAnimGraphNodeInputPinPos(Node, CanvasOrigin);
		const bool bSelectedLink = SelectedNodeId == Node.NodeId || SelectedNodeId == InputNode->NodeId;
		const ImU32 LinkColor = ImGui::GetColorU32(
			bSelectedLink ? ImVec4(1.0f, 0.82f, 0.35f, 1.0f) : ImVec4(0.55f, 0.72f, 1.0f, 1.0f));

		const ImVec2 ControlB = Sub(To, ImVec2(90.0f, 0.0f));
		DrawList->AddBezierCubic(
			From,
			Add(From, ImVec2(90.0f, 0.0f)),
			ControlB,
			To,
			LinkColor,
			bSelectedLink ? 4.0f : 3.0f);
		DrawArrowHead(DrawList, To, Sub(To, ControlB), LinkColor, bSelectedLink ? 15.0f : 12.0f);

		DrawList->AddCircleFilled(From, AnimGraphPinRadius + 1.0f, LinkColor);
		DrawList->AddCircleFilled(To, AnimGraphPinRadius + 1.0f, LinkColor);
	}
}

void FEditorAnimGraphWidget::RenderPendingLink(const ImVec2& CanvasOrigin)
{
	if (!EditingAsset || (DraggingOutputNodeId < 0 && DraggingInputNodeId < 0))
	{
		return;
	}

	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	const ImVec2 MousePos = ImGui::GetIO().MousePos;
	const ImU32 LinkColor = ImGui::GetColorU32(ImVec4(1.0f, 0.82f, 0.35f, 1.0f));

	if (DraggingOutputNodeId >= 0)
	{
		const FAnimGraphNodeDesc* SourceNode = EditingAsset->FindNode(DraggingOutputNodeId);
		if (!SourceNode || SourceNode->Type == EAnimGraphNodeType::OutputPose)
		{
			DraggingOutputNodeId = -1;
			return;
		}

		const ImVec2 From = GetAnimGraphNodeOutputPinPos(*SourceNode, CanvasOrigin);
		const ImVec2 ControlB = Sub(MousePos, ImVec2(90.0f, 0.0f));
		DrawList->AddBezierCubic(From, Add(From, ImVec2(90.0f, 0.0f)), ControlB, MousePos, LinkColor, 3.0f);
		DrawArrowHead(DrawList, MousePos, Sub(MousePos, ControlB), LinkColor, 16.0f);

		if (!ImGui::IsMouseReleased(ImGuiMouseButton_Left))
		{
			return;
		}

		for (FAnimGraphNodeDesc& Candidate : EditingAsset->Nodes)
		{
			if (Candidate.Type != EAnimGraphNodeType::OutputPose)
			{
				continue;
			}

			const ImVec2 InputPin = GetAnimGraphNodeInputPinPos(Candidate, CanvasOrigin);
			if (!IsMouseNear(InputPin, AnimGraphPinHitRadius))
			{
				continue;
			}

			SelectedNodeId = Candidate.NodeId;
			if (SetOutputPoseInput(Candidate, SourceNode->NodeId))
			{
				bDirty = true;
			}
			break;
		}
	}
	else if (DraggingInputNodeId >= 0)
	{
		FAnimGraphNodeDesc* OutputNode = EditingAsset->FindNode(DraggingInputNodeId);
		if (!OutputNode || OutputNode->Type != EAnimGraphNodeType::OutputPose)
		{
			DraggingInputNodeId = -1;
			return;
		}

		const ImVec2 To = GetAnimGraphNodeInputPinPos(*OutputNode, CanvasOrigin);
		const ImVec2 ControlB = Sub(To, ImVec2(90.0f, 0.0f));
		DrawList->AddBezierCubic(MousePos, Add(MousePos, ImVec2(90.0f, 0.0f)), ControlB, To, LinkColor, 3.0f);
		DrawArrowHead(DrawList, To, Sub(To, ControlB), LinkColor, 16.0f);

		if (!ImGui::IsMouseReleased(ImGuiMouseButton_Left))
		{
			return;
		}

		for (const FAnimGraphNodeDesc& Candidate : EditingAsset->Nodes)
		{
			if (Candidate.NodeId == OutputNode->NodeId || Candidate.Type == EAnimGraphNodeType::OutputPose)
			{
				continue;
			}

			const ImVec2 OutputPin = GetAnimGraphNodeOutputPinPos(Candidate, CanvasOrigin);
			if (!IsMouseNear(OutputPin, AnimGraphPinHitRadius))
			{
				continue;
			}

			SelectedNodeId = OutputNode->NodeId;
			if (SetOutputPoseInput(*OutputNode, Candidate.NodeId))
			{
				bDirty = true;
			}
			break;
		}
	}

	if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
	{
		DraggingOutputNodeId = -1;
		DraggingInputNodeId = -1;
	}
}

void FEditorAnimGraphWidget::RenderDetails()
{
	FAnimGraphNodeDesc* Node = FindSelectedNode();
	if (!Node)
	{
		ImGui::TextDisabled("No node selected.");
		return;
	}

	ImGui::TextUnformatted("Node");
	ImGui::Separator();

	const float DetailItemWidth = std::max(120.0f, ImGui::GetContentRegionAvail().x - 92.0f);
	ImGui::PushItemWidth(DetailItemWidth);

	if (InputFString("Name", Node->Name))
	{
		bDirty = true;
	}

	ImGui::TextDisabled("%s", AnimGraphNodeTypeToString(Node->Type).c_str());

	// Details 패널에서 노드 위치를 수정할 필요는 없을 것 같아 일단 주석 처리합니다.
	//if (ImGui::DragFloat2("Position", &Node->Position.X, 1.0f))
	//{
	//	bDirty = true;
	//}

	if (Node->Type == EAnimGraphNodeType::SequencePlayer)
	{
		RenderSequencePlayerDetails(*Node);
	}
	else if (Node->Type == EAnimGraphNodeType::OutputPose)
	{
		RenderOutputPoseDetails(*Node);
	}
	else if (Node->Type == EAnimGraphNodeType::StateMachine)
	{
		RenderStateMachineDetails(*Node);
	}

	ImGui::PopItemWidth();
}

void FEditorAnimGraphWidget::RenderOutputPoseDetails(FAnimGraphNodeDesc& Node)
{
	if (EditingAsset && EditingAsset->RootNodeId != Node.NodeId)
	{
		ImGui::TextDisabled("This Output Pose is not the root.");
		if (ImGui::Button("Set As Root"))
		{
			EditingAsset->RootNodeId = Node.NodeId;
			bDirty = true;
		}
	}
	else
	{
		ImGui::TextDisabled("Root Output Pose");
	}

	ImGui::Spacing();
	ImGui::SeparatorText("Input Pose");
	ImGui::TextDisabled("This combo is the source of the visible canvas link.");

	if (RenderOutputPoseSourceCombo("Source", Node))
	{
		bDirty = true;
	}

	ImGui::BeginDisabled(Node.InputPoseNodeId < 0);
	if (ImGui::Button("Clear Input"))
	{
		if (SetOutputPoseInput(Node, -1))
		{
			bDirty = true;
		}
	}
	ImGui::EndDisabled();
}

void FEditorAnimGraphWidget::RenderSequencePlayerDetails(FAnimGraphNodeDesc& Node)
{
	ImGui::Spacing();
	ImGui::TextUnformatted("Sequence Player");

	if (RenderAnimationPathCombo("Animation", Node.AnimationPath))
	{
		bDirty = true;
	}

	if (ImGui::DragFloat("Play Rate", &Node.PlayRate, 0.01f, 0.0f, 10.0f))
	{
		bDirty = true;
	}

	if (ImGui::Checkbox("Loop", &Node.bLoop))
	{
		bDirty = true;
	}

	ImGui::Spacing();
	ImGui::SeparatorText("Output Link");
	if (FAnimGraphNodeDesc* RootOutput = FindRootOutputPoseNode())
	{
		const bool bConnected = RootOutput->InputPoseNodeId == Node.NodeId;
		ImGui::TextDisabled("Root Output: %s", GetNodeComboLabel(*RootOutput).c_str());
		ImGui::BeginDisabled(bConnected);
		if (ImGui::Button("Connect To Root Output"))
		{
			if (ConnectRootOutputToNode(Node.NodeId))
			{
				bDirty = true;
			}
		}
		ImGui::EndDisabled();
		if (bConnected)
		{
			ImGui::SameLine();
			ImGui::TextDisabled("Connected");
		}
	}
	else
	{
		ImGui::TextDisabled("No OutputPose node exists.");
	}
}

bool FEditorAnimGraphWidget::RenderOutputPoseSourceCombo(const char* Label, FAnimGraphNodeDesc& Node)
{
	if (!EditingAsset || Node.Type != EAnimGraphNodeType::OutputPose)
	{
		return false;
	}

	bool bChanged = false;
	FString CurrentLabel = "None";
	if (const FAnimGraphNodeDesc* InputNode = EditingAsset->FindNode(Node.InputPoseNodeId))
	{
		CurrentLabel = GetNodeComboLabel(*InputNode);
	}

	if (ImGui::BeginCombo(Label, CurrentLabel.c_str()))
	{
		const bool bNoneSelected = Node.InputPoseNodeId < 0;
		if (ImGui::Selectable("None", bNoneSelected))
		{
			bChanged = SetOutputPoseInput(Node, -1) || bChanged;
		}

		for (const FAnimGraphNodeDesc& Candidate : EditingAsset->Nodes)
		{
			if (Candidate.NodeId == Node.NodeId || Candidate.Type == EAnimGraphNodeType::OutputPose)
			{
				continue;
			}

			const FString CandidateLabel = GetNodeComboLabel(Candidate);
			const bool bSelected = Node.InputPoseNodeId == Candidate.NodeId;
			if (ImGui::Selectable(CandidateLabel.c_str(), bSelected))
			{
				bChanged = SetOutputPoseInput(Node, Candidate.NodeId) || bChanged;
			}
			if (bSelected)
			{
				ImGui::SetItemDefaultFocus();
			}
		}

		ImGui::EndCombo();
	}

	return bChanged;
}

bool FEditorAnimGraphWidget::RenderAnimationPathCombo(const char* Label, FString& Path, bool bShowPathInput)
{
	bool bChanged = false;
	TArray<FString> AnimPaths = FResourceManager::Get().GetAnimSequencePaths();
	const FString PreviewText = Path.empty() ? FString("<None>") : (bShowPathInput ? Path : GetFileName(Path));
	FString SelectedPath = Path;
	if (ImGui::BeginCombo(Label, PreviewText.c_str()))
	{
		if (ImGui::Selectable("<None>", Path.empty()))
		{
			SelectedPath.clear();
			bChanged = true;
		}
		for (const FString& AnimPath : AnimPaths)
		{
			const bool bSelected = Path == AnimPath;
			if (ImGui::Selectable(AnimPath.c_str(), bSelected))
			{
				SelectedPath = AnimPath;
				bChanged = true;
			}
			if (bSelected)
			{
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}

	if (bChanged)
	{
		Path = SelectedPath;
	}

	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload("AnimSequenceContentItem"))
		{
			if (Payload->Data && Payload->DataSize > 0)
			{
				const FString PayloadPath = static_cast<const char*>(Payload->Data);
				const std::filesystem::path DroppedPath = FPaths::ToWide(PayloadPath);
				Path = DroppedPath.is_absolute()
					? FPaths::Normalize(FPaths::ToRelativeString(DroppedPath.wstring()))
					: FPaths::Normalize(PayloadPath);
				bChanged = true;
			}
		}
		ImGui::EndDragDropTarget();
	}

	if (bShowPathInput)
	{
		char PathBuffer[512] = {};
		std::strncpy(PathBuffer, Path.c_str(), sizeof(PathBuffer) - 1);
		if (ImGui::InputText("Path", PathBuffer, sizeof(PathBuffer)))
		{
			Path = PathBuffer;
			bChanged = true;
		}
	}

	return bChanged;
}

bool FEditorAnimGraphWidget::RenderStateMachineEntryCombo(const char* Label, FAnimStateMachineDesc& StateMachine)
{
	bool bChanged = false;
	const FString EntryLabel = GetStateDisplayName(StateMachine, StateMachine.EntryStateId);
	if (ImGui::BeginCombo(Label, EntryLabel.c_str()))
	{
		const bool bNoneSelected = StateMachine.EntryStateId < 0;
		if (ImGui::Selectable("<None>", bNoneSelected))
		{
			if (StateMachine.EntryStateId != -1)
			{
				StateMachine.EntryStateId = -1;
				bChanged = true;
			}
		}

		for (const FAnimStateDesc& State : StateMachine.States)
		{
			const FString StateLabel = GetStateDisplayName(StateMachine, State.StateId);
			const bool bSelected = StateMachine.EntryStateId == State.StateId;
			if (ImGui::Selectable(StateLabel.c_str(), bSelected))
			{
				if (StateMachine.EntryStateId != State.StateId)
				{
					StateMachine.EntryStateId = State.StateId;
					bChanged = true;
				}
			}
			if (bSelected)
			{
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}

	return bChanged;
}


bool FEditorAnimGraphWidget::RenderStateNameInput(const char* Label, int32 MachineNodeId, FAnimStateDesc& State)
{
	const long long Key = MakeStateNameEditKey(MachineNodeId, State.StateId);
	std::array<char, 256>& Buffer = StateNameEditBuffers[Key];

	// State 이름은 Details의 Tree header, Entry combo, Transition label 등 여러 UI에 동시에 쓰입니다.
	// 입력 중 매 글자마다 실제 State.Name을 바꾸면 그 주변 UI가 즉시 갱신되며 InputText 포커스가 끊길 수 있으므로,
	// 편집 중에는 고정 버퍼를 사용하고 입력 완료 시 실제 이름에 반영합니다.
	if (EditingStateNameKey != Key)
	{
		CopyToFixedBuffer(Buffer, State.Name);
	}

	bool bCommitted = false;
	const bool bChanged = ImGui::InputText(Label, Buffer.data(), Buffer.size());
	if (ImGui::IsItemActive())
	{
		EditingStateNameKey = Key;
	}
	if (ImGui::IsItemDeactivatedAfterEdit())
	{
		const FString NewName = Buffer.data();
		if (State.Name != NewName)
		{
			State.Name = NewName;
			bCommitted = true;
		}
		EditingStateNameKey = -1;
	}
	else if (ImGui::IsItemDeactivated() && EditingStateNameKey == Key)
	{
		EditingStateNameKey = -1;
	}

	// Enter 키로 입력을 마쳤을 때도 바로 반영합니다.
	if (EditingStateNameKey == Key && ImGui::IsItemFocused() && ImGui::IsKeyPressed(ImGuiKey_Enter, false))
	{
		const FString NewName = Buffer.data();
		if (State.Name != NewName)
		{
			State.Name = NewName;
			bCommitted = true;
		}
		EditingStateNameKey = -1;
	}

	return bCommitted || bChanged;
}

void FEditorAnimGraphWidget::CommitStateNameEdits()
{
	if (!EditingAsset || StateNameEditBuffers.empty())
	{
		return;
	}

	for (FAnimGraphNodeDesc& Node : EditingAsset->Nodes)
	{
		if (Node.Type != EAnimGraphNodeType::StateMachine)
		{
			continue;
		}

		for (FAnimStateDesc& State : Node.StateMachine.States)
		{
			const long long Key = MakeStateNameEditKey(Node.NodeId, State.StateId);
			auto It = StateNameEditBuffers.find(Key);
			if (It == StateNameEditBuffers.end())
			{
				continue;
			}

			const FString NewName = It->second.data();
			if (State.Name != NewName)
			{
				State.Name = NewName;
				bDirty = true;
			}
		}
	}
}

void FEditorAnimGraphWidget::RenderStateMachineDetails(FAnimGraphNodeDesc& Node)
{
	FAnimStateMachineDesc& Machine = Node.StateMachine;

	ImGui::Spacing();
	ImGui::TextUnformatted("State Machine");

	if (FAnimGraphNodeDesc* RootOutput = FindRootOutputPoseNode())
	{
		const bool bConnected = RootOutput->InputPoseNodeId == Node.NodeId;
		ImGui::TextDisabled("Root Output: %s", GetNodeComboLabel(*RootOutput).c_str());
		ImGui::BeginDisabled(bConnected);
		if (ImGui::Button("Connect To Root Output"))
		{
			if (ConnectRootOutputToNode(Node.NodeId))
			{
				bDirty = true;
			}
		}
		ImGui::EndDisabled();
		if (bConnected)
		{
			ImGui::SameLine();
			ImGui::TextDisabled("Connected");
		}
	}
	else
	{
		ImGui::TextDisabled("No OutputPose node exists.");
	}

	if (RenderStateMachineEntryCombo("Entry State", Machine))
	{
		bDirty = true;
	}

	ImGui::SeparatorText("States");
	if (ImGui::Button("Add State"))
	{
		AddStateToStateMachine(Machine);
		bDirty = true;
	}

	int32 StateToDelete = -1;
	for (int32 StateIndex = 0; StateIndex < static_cast<int32>(Machine.States.size()); ++StateIndex)
	{
		FAnimStateDesc& State = Machine.States[StateIndex];
		ImGui::PushID(State.StateId);

		const FString Header = GetStateDisplayName(Machine, State.StateId) + "##State";
		if (ImGui::TreeNodeEx(Header.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (RenderStateNameInput("Name", Node.NodeId, State))
			{
				bDirty = true;
			}

			if (RenderAnimationPathCombo("Animation", State.AnimationPath))
			{
				bDirty = true;
			}

			if (ImGui::DragFloat2("Position", &State.Position.X, 1.0f))
			{
				bDirty = true;
			}

			const bool bIsEntry = Machine.EntryStateId == State.StateId;
			if (!bIsEntry && ImGui::Button("Set Entry"))
			{
				Machine.EntryStateId = State.StateId;
				bDirty = true;
			}
			if (bIsEntry)
			{
				ImGui::TextDisabled("Entry State");
			}

			ImGui::SameLine();
			if (ImGui::Button("Delete State"))
			{
				StateToDelete = State.StateId;
			}

			ImGui::TreePop();
		}

		ImGui::PopID();
	}

	if (StateToDelete >= 0)
	{
		Machine.States.erase(
			std::remove_if(
				Machine.States.begin(),
				Machine.States.end(),
				[StateToDelete](const FAnimStateDesc& State)
				{
					return State.StateId == StateToDelete;
				}),
			Machine.States.end());

		Machine.Transitions.erase(
			std::remove_if(
				Machine.Transitions.begin(),
				Machine.Transitions.end(),
				[StateToDelete](const FAnimStateTransitionDesc& Transition)
				{
					return Transition.FromStateId == StateToDelete || Transition.ToStateId == StateToDelete;
				}),
			Machine.Transitions.end());

		if (Machine.EntryStateId == StateToDelete)
		{
			Machine.EntryStateId = Machine.States.empty() ? -1 : Machine.States.front().StateId;
		}
		if (SelectedStateId == StateToDelete)
		{
			SelectedStateId = -1;
		}
		SelectedTransitionIndex = -1;
		bDirty = true;
	}

	ImGui::SeparatorText("Transitions");
	ImGui::BeginDisabled(Machine.States.size() < 2);
	if (ImGui::Button("Add Transition"))
	{
		const int32 FromStateId = Machine.States[0].StateId;
		const int32 ToStateId = Machine.States.size() > 1 ? Machine.States[1].StateId : Machine.States[0].StateId;
		if (AddTransitionToStateMachine(Machine, FromStateId, ToStateId))
		{
			SelectedNodeId = Node.NodeId;
			SelectedStateId = -1;
			SelectedTransitionIndex = static_cast<int32>(Machine.Transitions.size()) - 1;
			bDirty = true;
		}
	}
	ImGui::EndDisabled();

	int32 TransitionToDelete = -1;
	for (int32 TransitionIndex = 0; TransitionIndex < static_cast<int32>(Machine.Transitions.size()); ++TransitionIndex)
	{
		FAnimStateTransitionDesc& Transition = Machine.Transitions[TransitionIndex];
		ImGui::PushID(TransitionIndex);

		FString Header = GetStateDisplayName(Machine, Transition.FromStateId)
			+ " -> "
			+ GetStateDisplayName(Machine, Transition.ToStateId)
			+ "##Transition";
		const bool bSelectedTransitionInDetails = SelectedTransitionIndex == TransitionIndex;
		if (bSelectedTransitionInDetails)
		{
			ImGui::SetNextItemOpen(true, ImGuiCond_Always);
		}
		if (ImGui::TreeNodeEx(Header.c_str(), bSelectedTransitionInDetails ? ImGuiTreeNodeFlags_DefaultOpen : 0))
		{
			if (ImGui::BeginCombo("From", GetStateDisplayName(Machine, Transition.FromStateId).c_str()))
			{
				for (const FAnimStateDesc& State : Machine.States)
				{
					const FString Label = GetStateDisplayName(Machine, State.StateId);
					const bool bSelected = Transition.FromStateId == State.StateId;
					if (ImGui::Selectable(Label.c_str(), bSelected))
					{
						Transition.FromStateId = State.StateId;
						bDirty = true;
					}
				}
				ImGui::EndCombo();
			}

			if (ImGui::BeginCombo("To", GetStateDisplayName(Machine, Transition.ToStateId).c_str()))
			{
				for (const FAnimStateDesc& State : Machine.States)
				{
					const FString Label = GetStateDisplayName(Machine, State.StateId);
					const bool bSelected = Transition.ToStateId == State.StateId;
					if (ImGui::Selectable(Label.c_str(), bSelected))
					{
						Transition.ToStateId = State.StateId;
						bDirty = true;
					}
				}
				ImGui::EndCombo();
			}

			if (ImGui::DragFloat("Blend Time", &Transition.BlendTime, 0.01f, 0.0f, 5.0f))
			{
				bDirty = true;
			}

			if (ImGui::InputInt("Priority", &Transition.Priority))
			{
				bDirty = true;
			}

			const char* ConditionLabels[] = {
				"AlwaysTrue",
				"BoolParameter",
				"FloatGreater",
				"FloatLess",
				"LuaFunction"
			};
			int32 ConditionIndex = static_cast<int32>(Transition.Condition.Type);
			if (ConditionIndex < 0 || ConditionIndex >= static_cast<int32>(std::size(ConditionLabels)))
			{
				ConditionIndex = 0;
			}
			if (ImGui::Combo("Condition", &ConditionIndex, ConditionLabels, static_cast<int32>(std::size(ConditionLabels))))
			{
				Transition.Condition.Type = static_cast<EAnimTransitionConditionType>(ConditionIndex);
				bDirty = true;
			}

			if (Transition.Condition.Type == EAnimTransitionConditionType::BoolParameter
				|| Transition.Condition.Type == EAnimTransitionConditionType::FloatGreater
				|| Transition.Condition.Type == EAnimTransitionConditionType::FloatLess)
			{
				char ParameterBuffer[128] = {};
				std::strncpy(ParameterBuffer, Transition.Condition.ParameterName.c_str(), sizeof(ParameterBuffer) - 1);
				if (ImGui::InputText("Parameter", ParameterBuffer, sizeof(ParameterBuffer)))
				{
					Transition.Condition.ParameterName = ParameterBuffer;
					bDirty = true;
				}
			}

			if (Transition.Condition.Type == EAnimTransitionConditionType::BoolParameter)
			{
				if (ImGui::Checkbox("Bool Value", &Transition.Condition.BoolValue))
				{
					bDirty = true;
				}
			}
			else if (Transition.Condition.Type == EAnimTransitionConditionType::FloatGreater
				|| Transition.Condition.Type == EAnimTransitionConditionType::FloatLess)
			{
				if (ImGui::DragFloat("Threshold", &Transition.Condition.Threshold, 0.1f))
				{
					bDirty = true;
				}
			}
			else if (Transition.Condition.Type == EAnimTransitionConditionType::LuaFunction)
			{
				char LuaFunctionBuffer[128] = {};
				std::strncpy(LuaFunctionBuffer, Transition.Condition.LuaFunctionName.c_str(), sizeof(LuaFunctionBuffer) - 1);
				if (ImGui::InputText("Lua Function", LuaFunctionBuffer, sizeof(LuaFunctionBuffer)))
				{
					Transition.Condition.LuaFunctionName = LuaFunctionBuffer;
					bDirty = true;
				}
				ImGui::TextDisabled("Lua transition conditions are not evaluated yet.");
			}

			if (ImGui::Button("Delete Transition"))
			{
				TransitionToDelete = TransitionIndex;
			}

			ImGui::TreePop();
		}

		ImGui::PopID();
	}

	if (TransitionToDelete >= 0 && TransitionToDelete < static_cast<int32>(Machine.Transitions.size()))
	{
		Machine.Transitions.erase(Machine.Transitions.begin() + TransitionToDelete);
		SelectedTransitionIndex = -1;
		bDirty = true;
	}
}

FAnimGraphNodeDesc* FEditorAnimGraphWidget::FindSelectedNode()
{
	return EditingAsset ? EditingAsset->FindNode(SelectedNodeId) : nullptr;
}

const FAnimGraphNodeDesc* FEditorAnimGraphWidget::FindSelectedNode() const
{
	return EditingAsset ? EditingAsset->FindNode(SelectedNodeId) : nullptr;
}

FAnimGraphNodeDesc* FEditorAnimGraphWidget::FindEditingStateMachineNode()
{
	if (!EditingAsset || EditingStateMachineNodeId < 0)
	{
		return nullptr;
	}

	FAnimGraphNodeDesc* Node = EditingAsset->FindNode(EditingStateMachineNodeId);
	return Node && Node->Type == EAnimGraphNodeType::StateMachine ? Node : nullptr;
}

const FAnimGraphNodeDesc* FEditorAnimGraphWidget::FindEditingStateMachineNode() const
{
	if (!EditingAsset || EditingStateMachineNodeId < 0)
	{
		return nullptr;
	}

	const FAnimGraphNodeDesc* Node = EditingAsset->FindNode(EditingStateMachineNodeId);
	return Node && Node->Type == EAnimGraphNodeType::StateMachine ? Node : nullptr;
}

FAnimGraphNodeDesc* FEditorAnimGraphWidget::FindFirstOutputPoseNode()
{
	if (!EditingAsset)
	{
		return nullptr;
	}

	for (FAnimGraphNodeDesc& Node : EditingAsset->Nodes)
	{
		if (Node.Type == EAnimGraphNodeType::OutputPose)
		{
			return &Node;
		}
	}
	return nullptr;
}

const FAnimGraphNodeDesc* FEditorAnimGraphWidget::FindFirstOutputPoseNode() const
{
	if (!EditingAsset)
	{
		return nullptr;
	}

	for (const FAnimGraphNodeDesc& Node : EditingAsset->Nodes)
	{
		if (Node.Type == EAnimGraphNodeType::OutputPose)
		{
			return &Node;
		}
	}
	return nullptr;
}

FAnimGraphNodeDesc* FEditorAnimGraphWidget::FindRootOutputPoseNode()
{
	if (!EditingAsset)
	{
		return nullptr;
	}

	FAnimGraphNodeDesc* RootNode = EditingAsset->FindNode(EditingAsset->RootNodeId);
	if (RootNode && RootNode->Type == EAnimGraphNodeType::OutputPose)
	{
		return RootNode;
	}

	return FindFirstOutputPoseNode();
}

const FAnimGraphNodeDesc* FEditorAnimGraphWidget::FindRootOutputPoseNode() const
{
	if (!EditingAsset)
	{
		return nullptr;
	}

	const FAnimGraphNodeDesc* RootNode = EditingAsset->FindNode(EditingAsset->RootNodeId);
	if (RootNode && RootNode->Type == EAnimGraphNodeType::OutputPose)
	{
		return RootNode;
	}

	return FindFirstOutputPoseNode();
}

FString FEditorAnimGraphWidget::GetNodeDisplayName(const FAnimGraphNodeDesc& Node) const
{
	return Node.Name.empty() ? AnimGraphNodeTypeToString(Node.Type) : Node.Name;
}

FString FEditorAnimGraphWidget::GetNodeComboLabel(const FAnimGraphNodeDesc& Node) const
{
	FString Label = GetNodeDisplayName(Node);
	Label += " #";
	Label += std::to_string(Node.NodeId);
	return Label;
}

bool FEditorAnimGraphWidget::SetOutputPoseInput(FAnimGraphNodeDesc& OutputNode, int32 InputNodeId)
{
	if (!EditingAsset || OutputNode.Type != EAnimGraphNodeType::OutputPose)
	{
		return false;
	}

	if (InputNodeId >= 0)
	{
		const FAnimGraphNodeDesc* InputNode = EditingAsset->FindNode(InputNodeId);
		if (!InputNode || InputNode->NodeId == OutputNode.NodeId || InputNode->Type == EAnimGraphNodeType::OutputPose)
		{
			return false;
		}
	}

	if (OutputNode.InputPoseNodeId == InputNodeId)
	{
		return false;
	}

	OutputNode.InputPoseNodeId = InputNodeId;
	return true;
}

bool FEditorAnimGraphWidget::ConnectRootOutputToNode(int32 SourceNodeId)
{
	FAnimGraphNodeDesc* RootOutput = FindRootOutputPoseNode();
	if (!RootOutput)
	{
		return false;
	}

	return SetOutputPoseInput(*RootOutput, SourceNodeId);
}

bool FEditorAnimGraphWidget::AutoConnectRootOutputIfEmpty(int32 SourceNodeId)
{
	FAnimGraphNodeDesc* RootOutput = FindRootOutputPoseNode();
	if (!RootOutput || RootOutput->InputPoseNodeId >= 0)
	{
		return false;
	}

	return SetOutputPoseInput(*RootOutput, SourceNodeId);
}

int32 FEditorAnimGraphWidget::CountStateMachineStates() const
{
	int32 Count = 0;
	if (!EditingAsset)
	{
		return Count;
	}

	for (const FAnimGraphNodeDesc& Node : EditingAsset->Nodes)
	{
		if (Node.Type == EAnimGraphNodeType::StateMachine)
		{
			Count += static_cast<int32>(Node.StateMachine.States.size());
		}
	}
	return Count;
}

int32 FEditorAnimGraphWidget::CountStateMachineTransitions() const
{
	int32 Count = 0;
	if (!EditingAsset)
	{
		return Count;
	}

	for (const FAnimGraphNodeDesc& Node : EditingAsset->Nodes)
	{
		if (Node.Type == EAnimGraphNodeType::StateMachine)
		{
			Count += static_cast<int32>(Node.StateMachine.Transitions.size());
		}
	}
	return Count;
}

int32 FEditorAnimGraphWidget::GenerateNodeId() const
{
	int32 MaxId = 0;
	if (EditingAsset)
	{
		for (const FAnimGraphNodeDesc& Node : EditingAsset->Nodes)
		{
			MaxId = std::max(MaxId, Node.NodeId);
		}
	}
	return MaxId + 1;
}

int32 FEditorAnimGraphWidget::GenerateStateId(const FAnimStateMachineDesc& StateMachine) const
{
	int32 MaxId = 0;
	for (const FAnimStateDesc& State : StateMachine.States)
	{
		MaxId = std::max(MaxId, State.StateId);
	}
	return MaxId + 1;
}

FString FEditorAnimGraphWidget::GetStateDisplayName(const FAnimStateMachineDesc& StateMachine, int32 StateId) const
{
	for (const FAnimStateDesc& State : StateMachine.States)
	{
		if (State.StateId == StateId)
		{
			FString Label = State.Name.empty() ? "State" : State.Name;
			Label += " (" + std::to_string(State.StateId) + ")";
			return Label;
		}
	}

	return StateId < 0 ? FString("<None>") : FString("Missing (") + std::to_string(StateId) + ")";
}

void FEditorAnimGraphWidget::AddStateToStateMachine(FAnimStateMachineDesc& StateMachine)
{
	FAnimStateDesc State;
	State.StateId = GenerateStateId(StateMachine);
	State.Name = "State " + std::to_string(State.StateId);
	State.Position = FVector2(80.0f + static_cast<float>(StateMachine.States.size()) * 40.0f, 80.0f);
	StateMachine.States.push_back(State);
	SelectedStateId = State.StateId;
	SelectedTransitionIndex = -1;
	if (StateMachine.EntryStateId < 0)
	{
		StateMachine.EntryStateId = State.StateId;
	}
}


bool FEditorAnimGraphWidget::AddTransitionToStateMachine(FAnimStateMachineDesc& StateMachine, int32 FromStateId, int32 ToStateId)
{
	if (FromStateId < 0 || ToStateId < 0 || FromStateId == ToStateId)
	{
		return false;
	}

	if (FindStateIndexById(StateMachine, FromStateId) < 0 || FindStateIndexById(StateMachine, ToStateId) < 0)
	{
		return false;
	}

	if (HasTransition(StateMachine, FromStateId, ToStateId))
	{
		return false;
	}

	FAnimStateTransitionDesc Transition;
	Transition.FromStateId = FromStateId;
	Transition.ToStateId = ToStateId;
	Transition.BlendTime = 0.2f;
	Transition.Condition.Type = EAnimTransitionConditionType::AlwaysTrue;
	StateMachine.Transitions.push_back(Transition);
	return true;
}

bool FEditorAnimGraphWidget::DeleteStateFromStateMachine(FAnimStateMachineDesc& StateMachine, int32 StateId)
{
	if (FindStateIndexById(StateMachine, StateId) < 0)
	{
		return false;
	}

	StateMachine.States.erase(
		std::remove_if(
			StateMachine.States.begin(),
			StateMachine.States.end(),
			[StateId](const FAnimStateDesc& State)
			{
				return State.StateId == StateId;
			}),
		StateMachine.States.end());

	StateMachine.Transitions.erase(
		std::remove_if(
			StateMachine.Transitions.begin(),
			StateMachine.Transitions.end(),
			[StateId](const FAnimStateTransitionDesc& Transition)
			{
				return Transition.FromStateId == StateId || Transition.ToStateId == StateId;
			}),
		StateMachine.Transitions.end());

	if (StateMachine.EntryStateId == StateId)
	{
		StateMachine.EntryStateId = StateMachine.States.empty() ? -1 : StateMachine.States.front().StateId;
	}

	if (SelectedStateId == StateId)
	{
		SelectedStateId = -1;
	}
	SelectedTransitionIndex = -1;
	DraggingTransitionFromStateId = -1;
	return true;
}

bool FEditorAnimGraphWidget::DeleteTransitionFromStateMachine(FAnimStateMachineDesc& StateMachine, int32 TransitionIndex)
{
	if (TransitionIndex < 0 || TransitionIndex >= static_cast<int32>(StateMachine.Transitions.size()))
	{
		return false;
	}

	StateMachine.Transitions.erase(StateMachine.Transitions.begin() + TransitionIndex);
	SelectedTransitionIndex = -1;
	return true;
}

bool FEditorAnimGraphWidget::HasTransition(const FAnimStateMachineDesc& StateMachine, int32 FromStateId, int32 ToStateId) const
{
	for (const FAnimStateTransitionDesc& Transition : StateMachine.Transitions)
	{
		if (Transition.FromStateId == FromStateId && Transition.ToStateId == ToStateId)
		{
			return true;
		}
	}
	return false;
}

bool FEditorAnimGraphWidget::NormalizeGraphNodeIds()
{
	if (!EditingAsset)
	{
		return false;
	}

	std::unordered_set<int32> UsedIds;
	int32 NextId = 1;
	bool bChanged = false;

	for (FAnimGraphNodeDesc& Node : EditingAsset->Nodes)
	{
		const int32 OldId = Node.NodeId;
		const bool bValidUniqueId = OldId > 0 && UsedIds.find(OldId) == UsedIds.end();
		if (bValidUniqueId)
		{
			UsedIds.insert(OldId);
			NextId = std::max(NextId, OldId + 1);
			continue;
		}

		while (UsedIds.find(NextId) != UsedIds.end())
		{
			++NextId;
		}

		Node.NodeId = NextId;
		UsedIds.insert(Node.NodeId);
		++NextId;
		bChanged = true;
	}

	return bChanged;
}

void FEditorAnimGraphWidget::NormalizeRootNode()
{
	if (!EditingAsset)
	{
		return;
	}

	const FAnimGraphNodeDesc* CurrentRoot = EditingAsset->FindNode(EditingAsset->RootNodeId);
	if (CurrentRoot && CurrentRoot->Type == EAnimGraphNodeType::OutputPose)
	{
		return;
	}

	if (const FAnimGraphNodeDesc* OutputNode = FindFirstOutputPoseNode())
	{
		EditingAsset->RootNodeId = OutputNode->NodeId;
	}
	else if (!EditingAsset->Nodes.empty())
	{
		EditingAsset->RootNodeId = EditingAsset->Nodes.front().NodeId;
	}
	else
	{
		EditingAsset->RootNodeId = -1;
	}
}

void FEditorAnimGraphWidget::AddSequencePlayerNode()
{
	if (!EditingAsset)
	{
		return;
	}

	FAnimGraphNodeDesc Node;
	Node.NodeId = GenerateNodeId();
	Node.Type = EAnimGraphNodeType::SequencePlayer;
	Node.Name = "Sequence Player";
	Node.Position = FVector2(120.0f, 120.0f);
	EditingAsset->Nodes.push_back(Node);
	AutoConnectRootOutputIfEmpty(Node.NodeId);
	SelectedNodeId = Node.NodeId;
	bDirty = true;
}

void FEditorAnimGraphWidget::AddOutputPoseNode()
{
	if (!EditingAsset)
	{
		return;
	}

	const int32 PreferredInputNodeId = SelectedNodeId;

	FAnimGraphNodeDesc Node;
	Node.NodeId = GenerateNodeId();
	Node.Type = EAnimGraphNodeType::OutputPose;
	Node.Name = "Output Pose";
	Node.Position = FVector2(420.0f, 120.0f);

	if (const FAnimGraphNodeDesc* PreferredInput = EditingAsset->FindNode(PreferredInputNodeId))
	{
		if (PreferredInput->Type != EAnimGraphNodeType::OutputPose)
		{
			Node.InputPoseNodeId = PreferredInput->NodeId;
		}
	}

	if (Node.InputPoseNodeId < 0)
	{
		for (const FAnimGraphNodeDesc& Candidate : EditingAsset->Nodes)
		{
			if (Candidate.Type != EAnimGraphNodeType::OutputPose)
			{
				Node.InputPoseNodeId = Candidate.NodeId;
				break;
			}
		}
	}

	EditingAsset->Nodes.push_back(Node);
	EditingAsset->RootNodeId = Node.NodeId;
	SelectedNodeId = Node.NodeId;
	bDirty = true;
}

void FEditorAnimGraphWidget::AddStateMachineNode()
{
	if (!EditingAsset)
	{
		return;
	}

	FAnimGraphNodeDesc Node;
	Node.NodeId = GenerateNodeId();
	Node.Type = EAnimGraphNodeType::StateMachine;
	Node.Name = "State Machine";
	Node.Position = FVector2(240.0f, 220.0f);

	FAnimStateDesc EntryState;
	EntryState.StateId = 1;
	EntryState.Name = "Idle";
	EntryState.Position = FVector2(80.0f, 100.0f);
	Node.StateMachine.States.push_back(EntryState);
	Node.StateMachine.EntryStateId = EntryState.StateId;

	EditingAsset->Nodes.push_back(Node);
	AutoConnectRootOutputIfEmpty(Node.NodeId);
	SelectedNodeId = Node.NodeId;
	bDirty = true;
}

void FEditorAnimGraphWidget::DeleteSelectedNode()
{
	if (!EditingAsset || SelectedNodeId < 0)
	{
		return;
	}

	const int32 DeletedNodeId = SelectedNodeId;
	EditingAsset->Nodes.erase(
		std::remove_if(
			EditingAsset->Nodes.begin(),
			EditingAsset->Nodes.end(),
			[DeletedNodeId](const FAnimGraphNodeDesc& Node)
			{
				return Node.NodeId == DeletedNodeId;
			}),
		EditingAsset->Nodes.end());

	for (FAnimGraphNodeDesc& Node : EditingAsset->Nodes)
	{
		if (Node.InputPoseNodeId == DeletedNodeId)
		{
			Node.InputPoseNodeId = -1;
		}
	}

	if (EditingAsset->RootNodeId == DeletedNodeId)
	{
		NormalizeRootNode();
	}

	SelectedNodeId = -1;
	bDirty = true;
}

void FEditorAnimGraphWidget::EnterStateMachineView(int32 StateMachineNodeId)
{
	if (!EditingAsset)
	{
		return;
	}

	const FAnimGraphNodeDesc* Node = EditingAsset->FindNode(StateMachineNodeId);
	if (!Node || Node->Type != EAnimGraphNodeType::StateMachine)
	{
		return;
	}

	SelectedNodeId = StateMachineNodeId;
	EditingStateMachineNodeId = StateMachineNodeId;
	SelectedStateId = -1;
	SelectedTransitionIndex = -1;
	DraggingOutputNodeId = -1;
	DraggingInputNodeId = -1;
	DraggingTransitionFromStateId = -1;
	DraggingTransitionToStateId = -1;
	ViewMode = EViewMode::StateMachine;
}

void FEditorAnimGraphWidget::LeaveStateMachineView()
{
	ViewMode = EViewMode::AnimGraph;
	EditingStateMachineNodeId = -1;
	SelectedStateId = -1;
	SelectedTransitionIndex = -1;
	DraggingOutputNodeId = -1;
	DraggingInputNodeId = -1;
	DraggingTransitionFromStateId = -1;
	DraggingTransitionToStateId = -1;
}

void FEditorAnimGraphWidget::Save()
{
	if (!EditingAsset || EditingPath.empty())
	{
		return;
	}

	CommitStateNameEdits();
	NormalizeGraphNodeIds();
	NormalizeRootNode();
	if (FResourceManager::Get().SaveAnimGraph(EditingAsset, EditingPath))
	{
		bDirty = false;
		if (EditorEngine)
		{
			EditorEngine->GetNotificationService().Info("Anim graph saved.");
		}
	}
	else if (EditorEngine)
	{
		EditorEngine->GetNotificationService().Warning("Failed to save anim graph.");
	}
}
