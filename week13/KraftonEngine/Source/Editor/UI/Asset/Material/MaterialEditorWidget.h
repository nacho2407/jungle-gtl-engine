#pragma once

#include "Editor/UI/Asset/AssetEditorWidget.h"
#include "Materials/Graph/MaterialGraphTypes.h"

#include "imgui.h"

namespace ax { namespace NodeEditor { struct EditorContext; } }

class UMaterial;
class UTexture2D;

class FMaterialEditorWidget : public FAssetEditorWidget
{
public:
	FMaterialEditorWidget() = default;
	~FMaterialEditorWidget() override;

	bool CanEdit(UObject* Object) const override;
	void Open(UObject* Object) override;
	void Close() override;
	void Render(float DeltaTime) override;

private:
	void EnsureContext();
	void DestroyContext();

	void RenderToolbar(UMaterial* Material);
	void RenderGraphCanvas(UMaterial* Material, float Width, uint32& OutSelectedNodeId);
	void RenderInspector(UMaterial* Material, uint32 SelectedNodeId);
	void RenderErrorPanel();

	void RenderNodeBody(FMaterialGraphNode& Node);
	void RenderAddNodeMenu(FMaterialGraph& Graph, EMaterialDomain Domain);

	void CompileAndSave(UMaterial* Material);
	void CompileOnly(UMaterial* Material);
	void RebuildOutputPinsForDomain(UMaterial* Material);

	// 텍스처 경로 → UTexture2D 캐시. 노드 썸네일/인스펙터에서 공용.
	UTexture2D* GetOrLoadTexture(const FString& Path, bool bSRGB);

	// 드래그&드롭으로 받은 path를 노드에 반영(+ dirty 표시).
	void HandleTextureDropOnNode(FMaterialGraphNode& Node, const FString& ProjectRelativePath);

private:
	ax::NodeEditor::EditorContext* NodeEditorContext = nullptr;
	bool                           bPositionsPushed = false;
	ImVec2                         PendingNewNodePosition = ImVec2(0, 0);

	FString                        LastCompileError;
	char                           AddNodeSearchBuf[64] = {};

	TMap<FString, UTexture2D*>     TexturePreviewCache;
};
