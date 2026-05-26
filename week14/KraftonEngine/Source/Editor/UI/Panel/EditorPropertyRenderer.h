#pragma once

#include "Core/Types/PropertyTypes.h"
#include "Editor/UI/Dialog/FbxImportOptionsDialog.h"

class USceneComponent;

struct FEditorPropertyRenderOptions
{
	bool bDispatchChange = true;
	bool bUseExternalExpansion = false;
	bool bParentExpanded = true;
	FString PropertyPath;
	USceneComponent* EditedSceneComponent = nullptr;
	int32 IndentLevel = 0;
};

enum class EPropertyAssetPreviewType
{
	StaticMesh,
	SkeletalMesh,
	Material
};

struct FPropertyAssetFieldContext
{
	FString CurrentPath;
	FString PreviewName;
	EPropertyAssetPreviewType Type;
	std::function<bool()> RenderPicker;
	std::function<UObject* ()> LoadObject;
};

class FEditorPropertyRenderer
{
public:
	bool RenderPropertyWidget(TArray<FPropertyValue>& Props, int32& Index, FEditorPropertyRenderOptions Options = {});

	static const char* GetPropertyDisplayName(const FPropertyValue& Prop);
	static bool IsExpandableProperty(const FPropertyValue& Prop);
	static bool DrawPropertyLabel(const FPropertyValue& Prop, int32 IndentLevel = 0);

private:
	bool RenderSoftObjectPropertyWidget(FPropertyValue& Prop);
	bool RenderEnumPropertyWidget(FPropertyValue& Prop);
	bool RenderStructPropertyWidget(FPropertyValue& Prop, FEditorPropertyRenderOptions Options);
	bool RenderArrayPropertyWidget(FPropertyValue& Prop, FEditorPropertyRenderOptions Options);

	bool RenderAssetReferenceField(const FPropertyAssetFieldContext& Context);

	static FString OpenStaticMeshFileDialog();
	static FString OpenFbxFileDialog();

private:
	FString PendingStaticMeshImportPath;
	FString* PendingStaticMeshImportTarget = nullptr;
	int32 PendingStaticFbxSkinnedMeshPolicy = 0;
	FFbxSceneImportDialogState SkeletalFbxImportDialog;
};
