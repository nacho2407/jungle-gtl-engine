#pragma once

#include "Editor/UI/Panel/EditorPanelWidget.h"
#include "Object/Object.h"
#include "Asset/AssetRegistry.h"
#include "Editor/UI/Dialog/FbxImportOptionsDialog.h"

class UActorComponent;
class AActor;

class FEditorPropertyWidget : public FEditorPanelWidget
{
public:
	virtual void Render(const FEditorPanelContext& Context) override;

private:
	void RenameActor(AActor* PrimaryActor);
	void RenderAddComponentMenu(AActor* Actor);
	void RenderDetails(AActor* PrimaryActor, const TArray<AActor*>& SelectedActors);
	void RenderComponentProperties(AActor* Actor, const TArray<AActor*>& SelectedActors);
	void RenderActorProperties(AActor* PrimaryActor, const TArray<AActor*>& SelectedActors);
	bool RenderPropertyWidget(TArray<struct FPropertyValue>& Props, int32& Index, bool bDispatchChange = true, const FString& PropertyPath = {});
	bool RenderSoftObjectPropertyWidget(struct FPropertyValue& Prop);
	bool RenderEnumPropertyWidget(struct FPropertyValue& Prop);
	bool RenderStructPropertyWidget(struct FPropertyValue& Prop, bool bDispatchChange, const FString& PropertyPath);
	bool RenderArrayPropertyWidget(struct FPropertyValue& Prop, bool bDispatchChange, const FString& PropertyPath);
	void RenderCallInEditorFunctions(UObject* Object);

	void PropagatePropertyChange(const FString& PropName, const TArray<AActor*>& SelectedActors);

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

	FFbxSceneImportDialogState SkeletalFbxImportDialog;
};
