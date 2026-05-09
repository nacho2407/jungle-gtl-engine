#pragma once
#include "AssetEditorWidget.h"
#include "Editor/Viewport/MeshEditorViewportClient.h"

class FMeshEditorWidget : public FAssetEditorWidget
{
public:
	FMeshEditorWidget() = default;

	bool CanEdit(UObject* Object) const override;

	void Open(UObject* Object) override;
	void Close() override;

	void Render(float DeltaTime) override;

	FMeshEditorViewportClient* GetViewportClient() { return &ViewportClient; }

private:
	FMeshEditorViewportClient ViewportClient;
};
