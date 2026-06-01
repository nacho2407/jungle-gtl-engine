#pragma once

#include "Editor/UI/Asset/AssetEditorWidget.h"
#include "Editor/UI/Asset/Physics/PhysicsAssetEditorTypes.h"
#include "Editor/Viewport/Asset/PhysicsAssetEditorViewportClient.h"

struct ImVec2;
class AActor;
class UPhysicsAsset;
class USkeletalMesh;
class USkeletalMeshComponent;

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
	void RenderViewportPanel(ImVec2 Size);
	void RenderBodyTreePanel(UPhysicsAsset* Asset);
	void RenderDetailsPanel(UPhysicsAsset* Asset);
	void SelectBody(int32 BodyIndex);
	void SelectShape(int32 BodyIndex, EPhysicsAssetShapeType ShapeType, int32 ShapeIndex);
	void SelectConstraint(int32 ConstraintIndex);
	void ClearSelection();

private:
	FPhysicsAssetEditorViewportClient ViewportClient;
	FPhysicsAssetEditorSelection Selection;

	USkeletalMesh* PreviewMesh = nullptr;
	USkeletalMeshComponent* PreviewMeshComponent = nullptr;
	AActor* PreviewActor = nullptr;

	FName PreviewWorldHandle = FName::None;
	FString WindowIdSuffix;
	uint32 InstanceId = 0;

	bool bPendingClose = false;
};
