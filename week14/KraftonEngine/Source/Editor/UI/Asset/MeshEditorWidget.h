#pragma once
#include "AssetEditorWidget.h"
#include "Editor/Viewport/MeshEditorViewportClient.h"
#include "Slate/SWindow.h"

struct FSkeletalMesh;

class FMeshEditorWidget : public FAssetEditorWidget
{
public:
	FMeshEditorWidget() = default;

	bool CanEdit(UObject* Object) const override;

	void Open(UObject* Object) override;
	void Close() override;
	void Tick(float DeltaTime) override;

	void CollectPreviewViewports(TArray<IEditorPreviewViewportClient*>& OutClients) const override;

	void Render(float DeltaTime) override;

	bool IsMouseOverViewport() const { return IsOpen() && ViewportClient.IsMouseOverViewport(); }

	FMeshEditorViewportClient* GetViewportClient() { return &ViewportClient; }

private:
	void RenderBoneTree(const FSkeletalMesh* Asset, int32 Index);

private:
	SWindow MeshViewportWindow;
	FMeshEditorViewportClient ViewportClient;

	int32 SelectedBoneIndex = -1;
};
