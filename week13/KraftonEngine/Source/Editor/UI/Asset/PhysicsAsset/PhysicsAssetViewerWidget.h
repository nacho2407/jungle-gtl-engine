#pragma once

#include "UI/Asset/AssetEditorWidget.h"

class UPhysicsAsset;

class FPhysicsAssetViewerWidget : public FAssetEditorWidget
{
public:
	bool CanEdit(UObject* Object) const override;
	void Open(UObject* Object) override;
	void Close() override;
	void Render(float DeltaTime) override;

private:
	void RenderBodyList(UPhysicsAsset* PhysicsAsset);
	void RenderBodyDetails(UPhysicsAsset* PhysicsAsset);

private:
	int32 SelectedBodyIndex = -1;
};
