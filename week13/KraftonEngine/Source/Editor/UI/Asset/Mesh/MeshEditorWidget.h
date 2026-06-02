#pragma once

#include "Editor/UI/Asset/AssetEditorWidget.h"
#include "Editor/Viewport/Asset/MeshEditorViewportClient.h"
#include "Object/FName.h"

#include <memory>

class FMeshEditorWidgetTab;
class FSelectionManager;

enum class EMeshEditorTab : uint8 { Skeleton, Mesh, Animation, PhysicsAsset };

class FMeshEditorWidget : public FAssetEditorWidget
{
public:
	FMeshEditorWidget();
	~FMeshEditorWidget() override;

	bool CanEdit(UObject* Object) const override;
	bool IsEditingObject(UObject* Object) const override;
	void OnReuseForObject(UObject* Object) override;

	void Open(UObject* Object) override;
	void Close() override;
	void Tick(float DeltaTime) override;

	void CollectPreviewViewports(TArray<IEditorPreviewViewportClient*>& OutClients) const override;

	bool AllowsMultipleInstances() const override { return true; }

	void Render(const FEditorPanelContext& Context) override;

	bool IsMouseOverViewport() const { return IsOpen() && ViewportClient.IsMouseOverViewport(); }

	FMeshEditorViewportClient* GetViewportClient() { return &ViewportClient; }
	const FMeshEditorViewportClient* GetViewportClient() const { return &ViewportClient; }
	FSelectionManager* GetSelectionManager() const { return SelectionManager; }

	uint32 GetInstanceId() const { return InstanceId; }
	EMeshEditorTab GetActiveTabType() const { return ActiveTab; }

	void MarkEditorDirty() { MarkDirty(); }
	void SetActiveTab(EMeshEditorTab Tab);

	static void RecordImportDurationForAsset(const FString& AssetPath, double Seconds);
	static void ClearImportDurationForAsset(const FString& AssetPath);
	static double GetRecordedImportDurationForAsset(const FString& AssetPath);

private:
	bool ResolveOpenTarget(UObject* Object, UObject*& OutObjectToEdit, EMeshEditorTab& OutInitialTab) const;
	void CreatePreviewScene();
	void InitializeViewportForPreview();
	void ActivateInitialTab(EMeshEditorTab InitialTab);
	void InitializeTabs();
	void ResetTabs();
	FMeshEditorWidgetTab* FindTab(EMeshEditorTab Tab) const;
	FMeshEditorWidgetTab* GetActiveTab() const;
	void RenderTabBar();

private:
	mutable FMeshEditorViewportClient ViewportClient;
	FSelectionManager* SelectionManager = nullptr;

	EMeshEditorTab ActiveTab = EMeshEditorTab::Skeleton;
	TArray<std::unique_ptr<FMeshEditorWidgetTab>> Tabs;

	uint32  InstanceId;
	FName   PreviewWorldHandle = FName::None;
	FString WindowIdSuffix;

	bool bPendingClose = false;
};
