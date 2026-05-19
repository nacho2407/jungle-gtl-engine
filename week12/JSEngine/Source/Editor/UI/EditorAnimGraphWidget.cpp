#include "Editor/UI/EditorAnimGraphWidget.h"

#include "Core/ResourceManager.h"
#include "Core/Paths.h"
#include "Editor/EditorEngine.h"
#include "Editor/Notification/EditorNotificationService.h"
#include "ImGui/imgui.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
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

	ImVec2 GetAnimGraphNodeInputPinPos(const FAnimGraphNodeDesc& Node, const ImVec2& CanvasOrigin)
	{
		return Add(CanvasOrigin, ImVec2(Node.Position.X, Node.Position.Y + GetAnimGraphNodeHeight(Node) * 0.5f));
	}

	ImVec2 GetAnimGraphNodeOutputPinPos(const FAnimGraphNodeDesc& Node, const ImVec2& CanvasOrigin)
	{
		return Add(CanvasOrigin, ImVec2(Node.Position.X + AnimGraphNodeWidth, Node.Position.Y + GetAnimGraphNodeHeight(Node) * 0.5f));
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

	const float DetailsWidth = 330.0f;
	const ImVec2 Available = ImGui::GetContentRegionAvail();
	const float CanvasWidth = std::max(260.0f, Available.x - DetailsWidth - 8.0f);

	if (ImGui::BeginChild("##AnimGraphCanvasPane", ImVec2(CanvasWidth, 0.0f), true))
	{
		RenderCanvas();
	}
	ImGui::EndChild();

	ImGui::SameLine();

	if (ImGui::BeginChild("##AnimGraphDetailsPane", ImVec2(0.0f, 0.0f), true))
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
		const bool bHoveredInputPin = DraggingOutputNodeId >= 0 && IsMouseNear(InputPin, AnimGraphPinHitRadius);
		DrawList->AddCircleFilled(InputPin, AnimGraphPinRadius + (bHoveredInputPin ? 2.0f : 0.0f), bHoveredInputPin ? PinHoverColor : PinColor);
		DrawList->AddCircle(InputPin, AnimGraphPinRadius + 1.0f, PinOutlineColor, 12, 1.5f);
		DrawList->AddText(Add(InputPin, ImVec2(8.0f, -7.0f)), ImGui::GetColorU32(ImGuiCol_TextDisabled), "In");
	}
	else
	{
		const ImVec2 OutputPin = GetAnimGraphNodeOutputPinPos(Node, CanvasOrigin);
		const bool bHoveredOutputPin = IsMouseNear(OutputPin, AnimGraphPinHitRadius);
		const bool bDraggingThisPin = DraggingOutputNodeId == Node.NodeId;
		DrawList->AddCircleFilled(OutputPin, AnimGraphPinRadius + ((bHoveredOutputPin || bDraggingThisPin) ? 2.0f : 0.0f), (bHoveredOutputPin || bDraggingThisPin) ? PinHoverColor : PinColor);
		DrawList->AddCircle(OutputPin, AnimGraphPinRadius + 1.0f, PinOutlineColor, 12, 1.5f);
		DrawList->AddText(Sub(OutputPin, ImVec2(30.0f, 7.0f)), ImGui::GetColorU32(ImGuiCol_TextDisabled), "Out");

		if (bHoveredOutputPin && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
		{
			SelectedNodeId = Node.NodeId;
			DraggingOutputNodeId = Node.NodeId;
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

		DrawList->AddBezierCubic(
			From,
			Add(From, ImVec2(90.0f, 0.0f)),
			Sub(To, ImVec2(90.0f, 0.0f)),
			To,
			LinkColor,
			bSelectedLink ? 4.0f : 3.0f);

		DrawList->AddCircleFilled(From, AnimGraphPinRadius + 1.0f, LinkColor);
		DrawList->AddCircleFilled(To, AnimGraphPinRadius + 1.0f, LinkColor);
	}
}

void FEditorAnimGraphWidget::RenderPendingLink(const ImVec2& CanvasOrigin)
{
	if (!EditingAsset || DraggingOutputNodeId < 0)
	{
		return;
	}

	const FAnimGraphNodeDesc* SourceNode = EditingAsset->FindNode(DraggingOutputNodeId);
	if (!SourceNode || SourceNode->Type == EAnimGraphNodeType::OutputPose)
	{
		DraggingOutputNodeId = -1;
		return;
	}

	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	const ImVec2 From = GetAnimGraphNodeOutputPinPos(*SourceNode, CanvasOrigin);
	const ImVec2 To = ImGui::GetIO().MousePos;
	const ImU32 LinkColor = ImGui::GetColorU32(ImVec4(1.0f, 0.82f, 0.35f, 1.0f));
	DrawList->AddBezierCubic(
		From,
		Add(From, ImVec2(90.0f, 0.0f)),
		Sub(To, ImVec2(90.0f, 0.0f)),
		To,
		LinkColor,
		3.0f);

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

	DraggingOutputNodeId = -1;
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

	char NameBuffer[128] = {};
	std::strncpy(NameBuffer, Node->Name.c_str(), sizeof(NameBuffer) - 1);
	if (ImGui::InputText("Name", NameBuffer, sizeof(NameBuffer)))
	{
		Node->Name = NameBuffer;
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
			char StateNameBuffer[128] = {};
			std::strncpy(StateNameBuffer, State.Name.c_str(), sizeof(StateNameBuffer) - 1);
			if (ImGui::InputText("Name", StateNameBuffer, sizeof(StateNameBuffer)))
			{
				State.Name = StateNameBuffer;
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
		bDirty = true;
	}

	ImGui::SeparatorText("Transitions");
	ImGui::BeginDisabled(Machine.States.size() < 2);
	if (ImGui::Button("Add Transition"))
	{
		FAnimStateTransitionDesc Transition;
		Transition.FromStateId = Machine.States[0].StateId;
		Transition.ToStateId = Machine.States.size() > 1 ? Machine.States[1].StateId : Machine.States[0].StateId;
		Transition.BlendTime = 0.2f;
		Transition.Condition.Type = EAnimTransitionConditionType::AlwaysTrue;
		Machine.Transitions.push_back(Transition);
		bDirty = true;
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
		if (ImGui::TreeNodeEx(Header.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
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
	if (StateMachine.EntryStateId < 0)
	{
		StateMachine.EntryStateId = State.StateId;
	}
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

void FEditorAnimGraphWidget::Save()
{
	if (!EditingAsset || EditingPath.empty())
	{
		return;
	}

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
