#pragma once

#include "Core/Types/PropertyTypes.h"
#include "Editor/UI/Dialog/FbxImportOptionsDialog.h"

#include <functional>

class USceneComponent;

struct FEditorPropertyRenderOptions
{
	bool bDispatchChange = true;
	bool bUseExternalExpansion = false;
	bool bParentExpanded = true;
	FString PropertyPath;
	USceneComponent* EditedSceneComponent = nullptr;
	int32 IndentLevel = 0;

	/**
	 * @brief property 값이 실제로 변경되기 직전에 호출되는 callback
	 *
	 * @details Details undo가 변경 전 snapshot을 렌더 프레임마다 만들지 않고, 실제 변경 직전에만 지연 생성하기 위한 hook입니다.
	 */
	std::function<void()> OnBeforePropertyMutate;
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
	bool RenderSoftObjectPropertyWidget(FPropertyValue& Prop, const FEditorPropertyRenderOptions& Options);
	bool RenderEnumPropertyWidget(FPropertyValue& Prop, const FEditorPropertyRenderOptions& Options);
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
