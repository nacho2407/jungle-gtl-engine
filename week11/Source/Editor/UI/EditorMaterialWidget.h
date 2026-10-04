#pragma once

#include "Editor/UI/EditorWidget.h"
#include "Render/Resource/Material.h"
#include <functional>

class UPrimitiveComponent;
class UStaticMesh;
class AActor;

/**
 * @brief 단일 머테리얼/머테리얼 인스턴스 편집 패널.
 */
class FEditorMaterialWidget : public FEditorWidget
{
public:
    void Render(float DeltaTime) override;
    void ResetSelection();
	void OpenMaterialAsset(UMaterialInterface* Material);
	void OpenMaterialSlot(UPrimitiveComponent* PrimitiveComp, int32 SlotIndex);
	void OnActorDestroyed(AActor* Actor);

private:
	void RenderSingleMaterialEditor(UPrimitiveComponent* SlotOwnerComp);
	void RenderAssetMaterialEditor();
	void RenderMaterialDetails(UPrimitiveComponent* SlotOwnerComp);
	void RenderMaterialPreviewSummary();
	void RenderMaterialPreview(UPrimitiveComponent* PrimitiveComp);
	void RenderMaterialProperties();
	UStaticMesh* ResolvePreviewMesh(UPrimitiveComponent* PrimitiveComp);
	void RefreshEditingMaterialFromSlot();
	bool CreateInstanceForCurrentMaterial();

private:
	int32 EditingSlotIndex = -1;
	UMaterialInterface* SelectedMaterialPtr = nullptr;
	UPrimitiveComponent* EditingSlotOwner = nullptr;
	UMaterialInterface* AssetEditingMaterialPtr = nullptr;
	UStaticMesh* PreviewMesh = nullptr;
	float PreviewYawRad = 0.8f;
	float PreviewPitchRad = 0.25f;
	float PreviewDistance = 4.0f;
	bool bFocusWindowNextFrame = false;
};
