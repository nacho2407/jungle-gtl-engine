#pragma once

#include "Editor/Subsystem/AssetFactory.h"
#include "Editor/UI/Asset/AssetEditorWidget.h"
#include "Editor/UI/Asset/Physics/PhysicsAssetEditorTypes.h"
#include "Editor/Viewport/Asset/PhysicsAssetEditorViewportClient.h"
#include "Math/Transform.h"
#include "Physics/PhysicsConstraintTemplate.h"

struct ImDrawList;
struct ImVec2;
class AActor;
class UPhysicsAsset;
class UBodySetup;
class UPhysicsConstraintTemplate;
class USkeletalMesh;
class USkeletalMeshComponent;
struct FSkeletalMesh;

namespace ax
{
	namespace NodeEditor
	{
		struct EditorContext;
	}
}

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
	void RenderViewportPanel(UPhysicsAsset* Asset, ImVec2 Size);
	void RenderSolidBodiesOverlay(ImDrawList* DrawList, const ImVec2& ViewportPos, const ImVec2& ViewportSize, UPhysicsAsset* Asset) const;
	void RenderPhysicsListPanel(UPhysicsAsset* Asset, ImVec2 Size);
	void RenderSkeletonTreePanel(UPhysicsAsset* Asset);
	void RenderGraphPanel(UPhysicsAsset* Asset, ImVec2 Size);
	void RenderDetailsPanel(UPhysicsAsset* Asset);
	void RenderToolsPanel(UPhysicsAsset* Asset, ImVec2 Size);
	void RenderBoneTreeNode(const FSkeletalMesh* MeshAsset, UPhysicsAsset* Asset, int32 BoneIndex);
	void RenderShapeDetails(UPhysicsAsset* Asset, UBodySetup* Body);
	void RenderConstraintDetails(UPhysicsAsset* Asset);
	bool CreateBodyForBone(UPhysicsAsset* Asset, int32 BoneIndex);
	bool RegenerateBodies(UPhysicsAsset* Asset);
	void SetEditorMode(EPhysicsAssetEditorMode Mode);
	void ApplyViewPreset(EPhysicsAssetEditorViewPreset Preset);
	void TickPreviewSimulation(float DeltaTime);
	void CapturePreviewSimulationStartPose();
	void RestorePreviewSimulationStartPose();
	void StopPreviewSimulation();
	void HandleViewportSelectionClick();
	void SelectBody(int32 BodyIndex);
	void SelectShape(int32 BodyIndex, EPhysicsAssetShapeType ShapeType, int32 ShapeIndex);
	void SelectConstraint(int32 ConstraintIndex);
	void SelectBone(int32 BoneIndex);
	void ClearSelection();
	void ValidateSelection(UPhysicsAsset* Asset);
	void SyncViewportHighlight();
	bool DeleteSelection(UPhysicsAsset* Asset);
	bool GetSelectedBodyBoneName(UPhysicsAsset* Asset, FName& OutBoneName) const;
	bool SetSelectedCollisionEnabled(UPhysicsAsset* Asset, bool bEnabled);
	bool SetSelectedPhysicalMaterialPath(UPhysicsAsset* Asset, const FString& Path);
	bool ApplyConstraintPreset(UPhysicsAsset* Asset, int32 ConstraintIndex, EAngularConstraintMode Mode, float Swing1, float Swing2, float Twist);
	UPhysicsConstraintTemplate* CreateConstraintBetweenBodies(UPhysicsAsset* Asset, int32 ParentBodyIndex, int32 ChildBodyIndex);
	void EnsureGraphEditorContext();
	void DestroyGraphEditorContext();

private:
	FPhysicsAssetEditorViewportClient ViewportClient;
	FPhysicsAssetEditorSelection Selection;
	EPhysicsAssetEditorMode ActiveMode = EPhysicsAssetEditorMode::Body;
	EPhysicsAssetEditorViewPreset ActiveViewPreset = EPhysicsAssetEditorViewPreset::Physics;

	USkeletalMesh* PreviewMesh = nullptr;
	USkeletalMeshComponent* PreviewMeshComponent = nullptr;
	AActor* PreviewActor = nullptr;

	float HierarchyWidth = 380.0f;
	float DetailsWidth = 380.0f;
	float ViewportListHeight = 220.0f;
	float GraphHeight = 190.0f;
	float ToolsHeight = 330.0f;
	FPhysicsAssetCreationParams BodyCreationParams;
	char TreeFilter[128] = {};
	char ListFilter[128] = {};
	char DetailsFilter[128] = {};

	FName PreviewWorldHandle = FName::None;
	FString WindowIdSuffix;
	uint32 InstanceId = 0;

	bool bPendingClose = false;
	bool bPreviewSimulationActive = false;
	TArray<FTransform> PreviewSimulationStartLocalPose;
	ax::NodeEditor::EditorContext* GraphEditorContext = nullptr;
	bool bGraphPositionsPushed = false;
};
