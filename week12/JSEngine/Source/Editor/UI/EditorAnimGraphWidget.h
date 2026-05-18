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

	const FString& GetEditingPath() const { return EditingPath; }
	bool IsDirty() const { return bDirty; }
	bool IsOpen() const { return bOpen; }

private:
	void RenderToolbar();
	void RenderCanvas();
	void RenderNode(FAnimGraphNodeDesc& Node, const ImVec2& CanvasOrigin);
	void RenderLinks(const ImVec2& CanvasOrigin);
	void RenderDetails();
	void RenderOutputPoseDetails(FAnimGraphNodeDesc& Node);
	void RenderSequencePlayerDetails(FAnimGraphNodeDesc& Node);

	FAnimGraphNodeDesc* FindSelectedNode();
	const FAnimGraphNodeDesc* FindSelectedNode() const;
	int32 GenerateNodeId() const;
	void AddSequencePlayerNode();
	void AddOutputPoseNode();
	void DeleteSelectedNode();
	void Save();

private:
	FString EditingPath;
	UAnimGraphAsset* EditingAsset = nullptr;
	int32 SelectedNodeId = -1;
	bool bOpen = false;
	bool bDirty = false;
};
