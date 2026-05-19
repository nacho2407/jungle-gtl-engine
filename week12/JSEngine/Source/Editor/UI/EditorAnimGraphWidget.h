#pragma once

#include "Editor/UI/EditorWidget.h"
#include "Animation/AnimGraphAsset.h"
#include "ImGui/imgui.h"

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
	void RenderToolbar();
	void RenderCanvas();
	void RenderNode(FAnimGraphNodeDesc& Node, const ImVec2& CanvasOrigin, int32 NodeIndex);
	void RenderLinks(const ImVec2& CanvasOrigin);
	void RenderPendingLink(const ImVec2& CanvasOrigin);
	void RenderDetails();
	void RenderOutputPoseDetails(FAnimGraphNodeDesc& Node);
	void RenderSequencePlayerDetails(FAnimGraphNodeDesc& Node);
	void RenderStateMachineDetails(FAnimGraphNodeDesc& Node);
	bool RenderOutputPoseSourceCombo(const char* Label, FAnimGraphNodeDesc& Node);
	bool RenderAnimationPathCombo(const char* Label, FString& Path, bool bShowPathInput = true);
	bool RenderStateMachineEntryCombo(const char* Label, FAnimStateMachineDesc& StateMachine);

	FAnimGraphNodeDesc* FindSelectedNode();
	const FAnimGraphNodeDesc* FindSelectedNode() const;
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
	bool NormalizeGraphNodeIds();
	void NormalizeRootNode();
	void AddSequencePlayerNode();
	void AddOutputPoseNode();
	void AddStateMachineNode();
	void DeleteSelectedNode();
private:
	FString EditingPath;
	UAnimGraphAsset* EditingAsset = nullptr;
	int32 SelectedNodeId = -1;
	int32 DraggingOutputNodeId = -1;
	bool bOpen = false;
	bool bDirty = false;
};
