#pragma once

#include "AssetEditorWidget.h"
#include "Object/FName.h"
#include "Editor/Viewport/StaticMeshEditorViewportClient.h"
#include "Slate/SWindow.h"

struct FStaticMesh;
struct ImDrawList;
struct ImVec2;

class FStaticMeshEditorWidget : public FAssetEditorWidget
{
public:
	FStaticMeshEditorWidget();

	bool CanEdit(UObject* Object) const override;
	bool IsEditingObject(UObject* Object) const override;

	void Open(UObject* Object) override;
	void Close() override;
	void Tick(float DeltaTime) override;

	void CollectPreviewViewports(TArray<IEditorPreviewViewportClient*>& OutClients) const override;

	bool AllowsMultipleInstances() const override { return true; }

	void Render(float DeltaTime) override;

private:
	void RenderMeshStatsOverlay(ImDrawList* DrawList, const ImVec2& ViewportPos) const;
	void RenderDetailsPanel(FStaticMesh* Asset) const;

private:
	SWindow MeshViewportWindow;
	FStaticMeshEditorViewportClient ViewportClient;

	uint32 InstanceId;
	FName PreviewWorldHandle = FName::None;
	FString WindowIdSuffix;
};
