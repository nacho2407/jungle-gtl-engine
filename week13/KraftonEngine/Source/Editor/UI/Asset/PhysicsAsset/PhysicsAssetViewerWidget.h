#pragma once

#include "Editor/Viewport/Asset/MeshEditorViewportClient.h"
#include "Object/FName.h"
#include "UI/Asset/AssetEditorWidget.h"

struct FSkeletalMesh;
struct ImVec2;
class UPhysicsAsset;
class USkeletalMesh;

class FPhysicsAssetViewerWidget : public FAssetEditorWidget
{
public:
	FPhysicsAssetViewerWidget();

	bool CanEdit(UObject* Object) const override;
	void Open(UObject* Object) override;
	void Close() override;
	void Tick(float DeltaTime) override;
	void Render(float DeltaTime) override;
	void CollectPreviewViewports(TArray<IEditorPreviewViewportClient*>& OutClients) const override;
	void AddReferencedObjects(FReferenceCollector& Collector) override;

private:
	void CreatePreviewWorld();
	void DestroyPreviewWorld();
	void RenderViewportPanel(ImVec2 Size);
	void RenderBodyList(UPhysicsAsset* PhysicsAsset);
	bool RenderBodyTree(const FSkeletalMesh* Asset, UPhysicsAsset* PhysicsAsset, int32 BoneIndex);
	void RenderBodyDetails(UPhysicsAsset* PhysicsAsset);
	bool SavePhysicsAsset(UPhysicsAsset* PhysicsAsset);

private:
	FMeshEditorViewportClient ViewportClient;
	FName PreviewWorldHandle = FName::None;
	uint32 InstanceId = 0;
	FString WindowIdSuffix;
	USkeletalMesh* SourceSkeletalMesh = nullptr;
	int32 SelectedBodyIndex = -1;
	int32 SelectedConstraintIndex = -1;
};
