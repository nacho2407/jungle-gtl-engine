#pragma once

#include "Core/Types/PropertyTypes.h"
#include "Editor/UI/Dialog/FbxImportOptionsDialog.h"

class USceneComponent;

struct FEditorPropertyRenderOptions
{
	bool bDispatchChange = true;
	FString PropertyPath;
	USceneComponent* EditedSceneComponent = nullptr;
};

class FEditorPropertyRenderer
{
public:
	bool RenderPropertyWidget(TArray<FPropertyValue>& Props, int32& Index, FEditorPropertyRenderOptions Options = {});

	static const char* GetPropertyDisplayName(const FPropertyValue& Prop);

private:
	bool RenderSoftObjectPropertyWidget(FPropertyValue& Prop);
	bool RenderEnumPropertyWidget(FPropertyValue& Prop);
	bool RenderStructPropertyWidget(FPropertyValue& Prop, FEditorPropertyRenderOptions Options);
	bool RenderArrayPropertyWidget(FPropertyValue& Prop, FEditorPropertyRenderOptions Options);

	static FString OpenStaticMeshFileDialog();
	static FString OpenFbxFileDialog();

private:
	FString PendingStaticMeshImportPath;
	FString* PendingStaticMeshImportTarget = nullptr;
	int32 PendingStaticFbxSkinnedMeshPolicy = 0;
	FFbxSceneImportDialogState SkeletalFbxImportDialog;
};
