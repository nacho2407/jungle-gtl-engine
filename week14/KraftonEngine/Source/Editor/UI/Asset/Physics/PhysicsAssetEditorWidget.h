#pragma once

#include "Editor/UI/Asset/AssetEditorWidget.h"
#include "Editor/UI/Asset/Physics/PhysicsAssetEditorTypes.h"
#include "Editor/Viewport/Asset/PhysicsAssetEditorViewportClient.h"

struct ImVec2;
class AActor;
class UPhysicsAsset;
class UBodySetup;
class USkeletalMesh;
class USkeletalMeshComponent;
struct FSkeletalMesh;

class FPhysicsAssetEditorWidget : public FAssetEditorWidget
{
public:
	FPhysicsAssetEditorWidget();

	bool CanEdit(UObject* Object) const override;
	bool IsEditingObject(UObject* Object) const override;

	void Open(UObject* Object) override;
	void Close() override;
	void Tick(float DeltaTime) override;
	void CollectPreviewViewports(TArray<IEditorPreviewViewportClient*>& OutClients) const override;
	bool AllowsMultipleInstances() const override { return true; }

	void Render(float DeltaTime) override;

private:
	void RenderModeToolbar(UPhysicsAsset* Asset);
	void RenderAssetDetailsPanel(UPhysicsAsset* Asset);
	void RenderViewportPanel(ImVec2 Size);
	void RenderPhysicsListPanel(UPhysicsAsset* Asset, ImVec2 Size);
	void RenderSkeletonTreePanel(UPhysicsAsset* Asset);
	void RenderDetailsPanel(UPhysicsAsset* Asset);
	void RenderBoneTreeNode(const FSkeletalMesh* MeshAsset, UPhysicsAsset* Asset, int32 BoneIndex);
	void RenderShapeDetails(UPhysicsAsset* Asset, UBodySetup* Body);
	void RenderConstraintDetails(UPhysicsAsset* Asset);
	void HandleViewportSelectionClick();
	void SelectBody(int32 BodyIndex);
	void SelectShape(int32 BodyIndex, EPhysicsAssetShapeType ShapeType, int32 ShapeIndex);
	void SelectConstraint(int32 ConstraintIndex);
	void SelectBone(int32 BoneIndex);
	void ClearSelection();
	void SyncViewportHighlight();
	bool DeleteSelection(UPhysicsAsset* Asset);

private:
	FPhysicsAssetEditorViewportClient ViewportClient;
	FPhysicsAssetEditorSelection Selection;
	EPhysicsAssetEditorMode ActiveMode = EPhysicsAssetEditorMode::Body;

	USkeletalMesh* PreviewMesh = nullptr;
	USkeletalMeshComponent* PreviewMeshComponent = nullptr;
	AActor* PreviewActor = nullptr;

	float HierarchyWidth = 300.0f;
	float DetailsWidth = 360.0f;
	float ViewportListHeight = 220.0f;
	float AssetDetailsHeight = 128.0f;
	char TreeFilter[128] = {};
	char ListFilter[128] = {};

	FName PreviewWorldHandle = FName::None;
	FString WindowIdSuffix;
	uint32 InstanceId = 0;

	bool bPendingClose = false;
};
