#pragma once
#include "EditorWidget.h"
#include <functional>

class USceneComponent;
class UPrimitiveComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UDecalComponent;
class UMaterialInstanceDynamic;

class FEditorMaterialWidget :  public FEditorWidget
{
public:
	void Render(float DeltaTime) override;
	void ResetSelection();

private:
	void RenderMaterialEditor(UPrimitiveComponent* PrimitiveComp);

	void RenderSectionList(UPrimitiveComponent* PrimitiveComp);
	void RenderMaterialDetails(UPrimitiveComponent* PrimitiveComp);
	void RenderMaterialProperties();

private:
	int32 SelectedSectionIndex = -1;
	UMaterialInstanceDynamic* SelectedMaterialPtr = nullptr;  // 원본 포인터 (Apply 대상)

	USceneComponent* SelectedComponent = nullptr;
};

