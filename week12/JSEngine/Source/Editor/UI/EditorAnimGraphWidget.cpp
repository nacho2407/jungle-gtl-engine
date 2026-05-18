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

	ImGui::SameLine();
	ImGui::TextDisabled("%s%s", EditingPath.c_str(), bDirty ? " *" : "");
	ImGui::SameLine();
	ImGui::TextDisabled("Root: %d", EditingAsset ? EditingAsset->RootNodeId : -1);
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
}

void FEditorAnimGraphWidget::RenderNode(FAnimGraphNodeDesc& Node, const ImVec2& CanvasOrigin, int32 NodeIndex)
{
	const ImVec2 NodePos = Add(CanvasOrigin, ToImVec2(Node.Position));
	const ImVec2 NodeSize(190.0f, 76.0f);
	const ImVec2 NodeMax = Add(NodePos, NodeSize);
	const bool bSelected = SelectedNodeId == Node.NodeId;

	ImGui::SetCursorScreenPos(NodePos);
	char Id[96];
	std::snprintf(Id, sizeof(Id), "##AnimGraphNode_%d_%d", Node.NodeId, NodeIndex);
	ImGui::InvisibleButton(Id, NodeSize);

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
	DrawList->AddRectFilled(NodePos, ImVec2(NodeMax.x, NodePos.y + 26.0f), HeaderColor, 6.0f);
	DrawList->AddRect(NodePos, NodeMax, ImGui::GetColorU32(ImVec4(0.48f, 0.55f, 0.68f, 1.0f)), 6.0f);

	const FString TypeText = AnimGraphNodeTypeToString(Node.Type);
	const FString NameText = Node.Name.empty() ? TypeText : Node.Name;
	DrawList->AddText(Add(NodePos, ImVec2(10.0f, 6.0f)), ImGui::GetColorU32(ImGuiCol_Text), NameText.c_str());
	DrawList->AddText(Add(NodePos, ImVec2(10.0f, 36.0f)), ImGui::GetColorU32(ImGuiCol_TextDisabled), TypeText.c_str());

	if (Node.Type == EAnimGraphNodeType::SequencePlayer && !Node.AnimationPath.empty())
	{
		const FString FileName = GetFileName(Node.AnimationPath);
		DrawList->AddText(Add(NodePos, ImVec2(10.0f, 55.0f)), ImGui::GetColorU32(ImGuiCol_TextDisabled), FileName.c_str());
	}
	else if (Node.Type == EAnimGraphNodeType::StateMachine)
	{
		const FString Summary = std::to_string(Node.StateMachine.States.size()) + " states, "
			+ std::to_string(Node.StateMachine.Transitions.size()) + " transitions";
		DrawList->AddText(Add(NodePos, ImVec2(10.0f, 55.0f)), ImGui::GetColorU32(ImGuiCol_TextDisabled), Summary.c_str());
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

		const ImVec2 From = Add(CanvasOrigin, ImVec2(InputNode->Position.X + 190.0f, InputNode->Position.Y + 38.0f));
		const ImVec2 To = Add(CanvasOrigin, ImVec2(Node.Position.X, Node.Position.Y + 38.0f));

		DrawList->AddBezierCubic(
			From,
			Add(From, ImVec2(80.0f, 0.0f)),
			Sub(To, ImVec2(80.0f, 0.0f)),
			To,
			ImGui::GetColorU32(ImVec4(0.55f, 0.72f, 1.0f, 1.0f)),
			3.0f);
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
	ImGui::TextUnformatted("Input Pose");

	FString CurrentLabel = "None";
	if (const FAnimGraphNodeDesc* InputNode = EditingAsset->FindNode(Node.InputPoseNodeId))
	{
		CurrentLabel = InputNode->Name.empty()
			? AnimGraphNodeTypeToString(InputNode->Type)
			: InputNode->Name;
	}

	if (ImGui::BeginCombo("Source", CurrentLabel.c_str()))
	{
		const bool bNoneSelected = Node.InputPoseNodeId < 0;
		if (ImGui::Selectable("None", bNoneSelected))
		{
			Node.InputPoseNodeId = -1;
			bDirty = true;
		}

		for (const FAnimGraphNodeDesc& Candidate : EditingAsset->Nodes)
		{
			if (Candidate.NodeId == Node.NodeId || Candidate.Type == EAnimGraphNodeType::OutputPose)
			{
				continue;
			}

			FString Label = Candidate.Name.empty()
				? AnimGraphNodeTypeToString(Candidate.Type)
				: Candidate.Name;
			Label += " (" + std::to_string(Candidate.NodeId) + ")";

			const bool bSelected = Node.InputPoseNodeId == Candidate.NodeId;
			if (ImGui::Selectable(Label.c_str(), bSelected))
			{
				Node.InputPoseNodeId = Candidate.NodeId;
				bDirty = true;
			}
		}

		ImGui::EndCombo();
	}
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
}

bool FEditorAnimGraphWidget::RenderAnimationPathCombo(const char* Label, FString& Path)
{
	bool bChanged = false;
	TArray<FString> AnimPaths = FResourceManager::Get().GetAnimSequencePaths();
	const char* Preview = Path.empty() ? "<None>" : Path.c_str();
	FString SelectedPath = Path;
	if (ImGui::BeginCombo(Label, Preview))
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

	char PathBuffer[512] = {};
	std::strncpy(PathBuffer, Path.c_str(), sizeof(PathBuffer) - 1);
	if (ImGui::InputText("Path", PathBuffer, sizeof(PathBuffer)))
	{
		Path = PathBuffer;
		bChanged = true;
	}

	return bChanged;
}

void FEditorAnimGraphWidget::RenderStateMachineDetails(FAnimGraphNodeDesc& Node)
{
	FAnimStateMachineDesc& Machine = Node.StateMachine;

	ImGui::Spacing();
	ImGui::TextUnformatted("State Machine");

	FString EntryLabel = GetStateDisplayName(Machine, Machine.EntryStateId);
	if (ImGui::BeginCombo("Entry State", EntryLabel.c_str()))
	{
		const bool bNoneSelected = Machine.EntryStateId < 0;
		if (ImGui::Selectable("<None>", bNoneSelected))
		{
			Machine.EntryStateId = -1;
			bDirty = true;
		}

		for (const FAnimStateDesc& State : Machine.States)
		{
			const FString Label = GetStateDisplayName(Machine, State.StateId);
			const bool bSelected = Machine.EntryStateId == State.StateId;
			if (ImGui::Selectable(Label.c_str(), bSelected))
			{
				Machine.EntryStateId = State.StateId;
				bDirty = true;
			}
		}
		ImGui::EndCombo();
	}

	ImGui::SeparatorText("States");
	if (ImGui::Button("Add State"))
	{
		FAnimStateDesc State;
		State.StateId = GenerateStateId(Machine);
		State.Name = "State " + std::to_string(State.StateId);
		State.Position = FVector2(80.0f + static_cast<float>(Machine.States.size()) * 40.0f, 80.0f);
		Machine.States.push_back(State);
		if (Machine.EntryStateId < 0)
		{
			Machine.EntryStateId = State.StateId;
		}
		bDirty = true;
	}

	int32 StateToDelete = -1;
	for (int32 StateIndex = 0; StateIndex < static_cast<int32>(Machine.States.size()); ++StateIndex)
	{
		FAnimStateDesc& State = Machine.States[StateIndex];
		ImGui::PushID(State.StateId);

		const FString Header = GetStateDisplayName(Machine, State.StateId);
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
			+ GetStateDisplayName(Machine, Transition.ToStateId);
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
	SelectedNodeId = Node.NodeId;
	bDirty = true;
}

void FEditorAnimGraphWidget::AddOutputPoseNode()
{
	if (!EditingAsset)
	{
		return;
	}

	FAnimGraphNodeDesc Node;
	Node.NodeId = GenerateNodeId();
	Node.Type = EAnimGraphNodeType::OutputPose;
	Node.Name = "Output Pose";
	Node.Position = FVector2(420.0f, 120.0f);
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
