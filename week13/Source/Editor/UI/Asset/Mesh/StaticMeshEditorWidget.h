#pragma once

#include "Editor/UI/Asset/AssetEditorWidget.h"
#include "Object/FName.h"
#include "Editor/Viewport/Asset/StaticMeshEditorViewportClient.h"

struct FStaticMesh;
struct ImDrawList;
struct ImVec2;
class UBodySetup;
class UStaticMesh;
struct FKShapeElem;

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

	void Render(const FEditorPanelContext& Context) override;

private:
	void RenderMeshStatsOverlay(ImDrawList* DrawList, const ImVec2& ViewportPos) const;
	void RenderDetailsPanel(FStaticMesh* Asset) const;
	void RenderTabBar();
	void RenderViewTab(UStaticMesh* StaticMesh, float DetailsWidth);
	void RenderAggregateGeometryTab(UStaticMesh* StaticMesh);
	void RenderViewportPanel(ImVec2 Size, bool bShowGizmoControls);
	void RenderAggregateShapeList(UStaticMesh* StaticMesh);
	void RenderAggregateShapeDetails(UStaticMesh* StaticMesh);
	bool RenderAddShapeContextMenu(UStaticMesh* StaticMesh);
	bool RenderShapeSelectable(const char* TypeLabel, FBodySetupShapeSelection Selection);
	void AddAggregateShape(UStaticMesh* StaticMesh, EAggCollisionShape Type);
	bool DeleteSelectedAggregateShape(UStaticMesh* StaticMesh);
	FKShapeElem* GetSelectedShape(UStaticMesh* StaticMesh) const;
	void SetSelectedShape(FBodySetupShapeSelection Selection);
	void SaveStaticMeshChange(const char* LogPrefix);
	void OnBodySetupShapeEdited();

private:
	enum class EStaticMeshEditorTab : uint8
	{
		View,
		AggregateGeometry,
	};

private:
	FStaticMeshEditorViewportClient ViewportClient;

	uint32 InstanceId;
	FName PreviewWorldHandle = FName::None;
	FString WindowIdSuffix;
	EStaticMeshEditorTab ActiveTab = EStaticMeshEditorTab::View;
	FBodySetupShapeSelection SelectedShape;
};
