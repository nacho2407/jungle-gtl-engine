#pragma once

#include "UI/Asset/AssetEditorWidget.h"

struct FSkeletalMesh;
class UPhysicsAsset;
class USkeletalMesh;

class FPhysicsAssetViewerWidget : public FAssetEditorWidget
{
public:
	bool CanEdit(UObject* Object) const override;
	void Open(UObject* Object) override;
	void Close() override;
	void Render(float DeltaTime) override;
	void AddReferencedObjects(FReferenceCollector& Collector) override;

private:
	void RenderBodyList(UPhysicsAsset* PhysicsAsset);
	bool RenderBodyTree(const FSkeletalMesh* Asset, UPhysicsAsset* PhysicsAsset, int32 BoneIndex);
	void RenderBodyDetails(UPhysicsAsset* PhysicsAsset);

private:
	USkeletalMesh* SourceSkeletalMesh = nullptr;
	int32 SelectedBodyIndex = -1;
};
