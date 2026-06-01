#pragma once

#include "Editor/Selection/SelectionManager.h"
#include "Editor/UI/Panel/EditorPanelWidget.h"
#include "Object/Object.h"
#include "Asset/AssetRegistry.h"
#include "Editor/UI/Dialog/FbxImportOptionsDialog.h"

#include <string>

class UActorComponent;
class AActor;

class FEditorPropertyWidget : public FEditorPanelWidget
{
public:
	virtual void Render(const FEditorPanelContext& Context) override;

private:
	void RenameActor(AActor* PrimaryActor);
	void RenderAddComponentMenu(AActor* Actor);
	void RenderDetails(const FSelectionDetailTarget& PrimaryTarget, const TArray<FSelectionDetailTarget>& SelectedTargets);
	void RenderTargetProperties(const FSelectionDetailTarget& PrimaryTarget, const TArray<FSelectionDetailTarget>& SelectedTargets);
	void CollectEditableProperties(const FSelectionDetailTarget& Target, TArray<struct FPropertyValue>& OutProps) const;
	bool RenderTransformCategory(TArray<struct FPropertyValue>& Props, const std::string& Category, const TArray<FSelectionDetailTarget>& SelectedTargets);
	bool RenderTransformVectorRow(struct FPropertyValue& Prop, bool bScaleRow);
	bool DrawTransformScaleLinkToggle();
	bool RenderPropertyWidget(TArray<struct FPropertyValue>& Props, int32& Index, bool bDispatchChange = true, const FString& PropertyPath = {});
	bool RenderSoftObjectPropertyWidget(struct FPropertyValue& Prop);
	bool RenderEnumPropertyWidget(struct FPropertyValue& Prop);
	bool RenderStructPropertyWidget(struct FPropertyValue& Prop, bool bDispatchChange, const FString& PropertyPath);
	bool RenderArrayPropertyWidget(struct FPropertyValue& Prop, bool bDispatchChange, const FString& PropertyPath);
	void RenderCallInEditorFunctions(UObject* Object);

	void PropagatePropertyChange(const FPropertyValue& SourceProp, const TArray<FSelectionDetailTarget>& SelectedTargets);

	void AddComponentToActor(AActor* Actor, UClass* ComponentClass);

	static FString OpenObjFileDialog();
	static FString OpenStaticMeshFileDialog();
	static FString OpenFbxFileDialog();

	AActor* LastSelectedActor = nullptr;

	char RenameBuffer[256] = {};
	bool bShowDuplicateWarning = false;
	FString PendingStaticMeshImportPath;
	FString* PendingStaticMeshImportTarget = nullptr;
	int32 PendingStaticFbxSkinnedMeshPolicy = 0;
	bool bTransformScaleLinked = false;

	FFbxSceneImportDialogState SkeletalFbxImportDialog;
	FSelectionManager* SelectionManager = nullptr;
};
