#pragma once

#include "Editor/UI/EditorWidget.h"
#include "Animation/AnimGraphAsset.h"
#include "ImGui/imgui.h"

#include <array>
#include <unordered_map>

class FEditorAnimGraphWidget : public FEditorWidget
{
public:
	void Initialize(UEditorEngine* InEditorEngine) override;
	void Render(float DeltaTime) override;
	void RenderEmbedded(float DeltaTime);

	void Open(const FString& InPath);
	void Close();
	void Save();
	void Reload();
	void SaveAndReload();

	const FString& GetEditingPath() const { return EditingPath; }
	bool IsDirty() const { return bDirty; }
	bool IsOpen() const { return bOpen; }

private:
	enum class EViewMode
	{
		AnimGraph,
		StateMachine,
	};

	void RenderToolbar();
	void RenderCanvas();
	void RenderAnimGraphCanvas();
	void RenderStateMachineCanvas();
	void RenderNode(FAnimGraphNodeDesc& Node, const ImVec2& CanvasOrigin, int32 NodeIndex);
	void RenderStateMachineStateNode(FAnimGraphNodeDesc& MachineNode, FAnimStateDesc& State, const ImVec2& CanvasOrigin, int32 StateIndex);
	void RenderStateMachineLinks(FAnimGraphNodeDesc& MachineNode, const ImVec2& CanvasOrigin);
	void RenderPendingStateMachineTransitionLink(FAnimGraphNodeDesc& MachineNode, const ImVec2& CanvasOrigin);
	void RenderSelectedStateMachineStateEditor(FAnimGraphNodeDesc& MachineNode, const ImVec2& CanvasOrigin, const ImVec2& CanvasSize);
	void RenderSelectedStateMachineTransitionEditor(FAnimGraphNodeDesc& MachineNode, const ImVec2& CanvasOrigin, const ImVec2& CanvasSize);
	void RenderLinks(const ImVec2& CanvasOrigin);
	void RenderPendingLink(const ImVec2& CanvasOrigin);
	void RenderDetails();
	void RenderOutputPoseDetails(FAnimGraphNodeDesc& Node);
	void RenderSequencePlayerDetails(FAnimGraphNodeDesc& Node);
	void RenderStateMachineDetails(FAnimGraphNodeDesc& Node);
	bool RenderOutputPoseSourceCombo(const char* Label, FAnimGraphNodeDesc& Node);
	bool RenderAnimationPathCombo(const char* Label, FString& Path, bool bShowPathInput = true);
	bool RenderStateMachineEntryCombo(const char* Label, FAnimStateMachineDesc& StateMachine);
	bool RenderStateNameInput(const char* Label, int32 MachineNodeId, FAnimStateDesc& State);
	void CommitStateNameEdits();

	FAnimGraphNodeDesc* FindSelectedNode();
	const FAnimGraphNodeDesc* FindSelectedNode() const;
	FAnimGraphNodeDesc* FindEditingStateMachineNode();
	const FAnimGraphNodeDesc* FindEditingStateMachineNode() const;
	FAnimGraphNodeDesc* FindFirstOutputPoseNode();
	const FAnimGraphNodeDesc* FindFirstOutputPoseNode() const;
	FAnimGraphNodeDesc* FindRootOutputPoseNode();
	const FAnimGraphNodeDesc* FindRootOutputPoseNode() const;
	FString GetNodeDisplayName(const FAnimGraphNodeDesc& Node) const;
	FString GetNodeComboLabel(const FAnimGraphNodeDesc& Node) const;
	bool SetOutputPoseInput(FAnimGraphNodeDesc& OutputNode, int32 InputNodeId);
	bool ConnectRootOutputToNode(int32 SourceNodeId);
	bool AutoConnectRootOutputIfEmpty(int32 SourceNodeId);
	int32 CountStateMachineStates() const;
	int32 CountStateMachineTransitions() const;
	int32 GenerateNodeId() const;
	int32 GenerateStateId(const FAnimStateMachineDesc& StateMachine) const;
	FString GetStateDisplayName(const FAnimStateMachineDesc& StateMachine, int32 StateId) const;
	void AddStateToStateMachine(FAnimStateMachineDesc& StateMachine);
	bool AddTransitionToStateMachine(FAnimStateMachineDesc& StateMachine, int32 FromStateId, int32 ToStateId);
	bool DeleteStateFromStateMachine(FAnimStateMachineDesc& StateMachine, int32 StateId);
	bool DeleteTransitionFromStateMachine(FAnimStateMachineDesc& StateMachine, int32 TransitionIndex);
	bool HasTransition(const FAnimStateMachineDesc& StateMachine, int32 FromStateId, int32 ToStateId) const;
	bool NormalizeGraphNodeIds();
	void NormalizeRootNode();
	void AddSequencePlayerNode();
	void AddOutputPoseNode();
	void AddStateMachineNode();
	void DeleteSelectedNode();
	void EnterStateMachineView(int32 StateMachineNodeId);
	void LeaveStateMachineView();
private:
	FString EditingPath;
	UAnimGraphAsset* EditingAsset = nullptr;
	int32 SelectedNodeId = -1;
	int32 DraggingOutputNodeId = -1;
	int32 DraggingInputNodeId = -1;
	int32 DraggingTransitionFromStateId = -1;
	int32 DraggingTransitionToStateId = -1;
	float DetailsPanelWidth = 330.0f;
	EViewMode ViewMode = EViewMode::AnimGraph;
	int32 EditingStateMachineNodeId = -1;
	int32 SelectedStateId = -1;
	int32 SelectedTransitionIndex = -1;
	long long EditingStateNameKey = -1;
	std::unordered_map<long long, std::array<char, 256>> StateNameEditBuffers;
	bool bOpen = false;
	bool bDirty = false;
};
