#pragma once

#include "Editor/UI/EditorWidget.h"
#include "Render/Resource/Material.h"
#include <functional>

class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UDecalComponent;

/**
 * @brief 섹션 기준 머테리얼 편집 패널.
 *
 * 레이아웃:
 *   왼쪽 패널 - 섹션 목록 (각 섹션이 참조하는 머테리얼 슬롯 표시)
 *   오른쪽 패널 - 선택된 섹션의 머테리얼 복사본 편집
 */
class FEditorMaterialWidget : public FEditorWidget
{
public:
    void Render(float DeltaTime) override;

private:
	void RenderMeshMaterialEditor(UStaticMeshComponent* MeshComp);
	void RenderDecalMaterialEditor(UDecalComponent* DecalComp);

    void RenderSectionList(UStaticMeshComponent* MeshComp);
    void RenderMaterialDetails(UStaticMeshComponent* MeshComp);
	void RenderMaterialDetails(FMaterial* Mat, std::function<void(FMaterial*)> OnMaterialChanged);

	void RenderMaterialProperties();
    void RenderColorSection(FMaterial& Mat);
    void RenderScalarSection(FMaterial& Mat);
    void RenderTextureSection(FMaterial& Mat);

    UStaticMeshComponent* GetSelectedMeshComponent() const;
	UDecalComponent* GetSelectedDecalComponent() const;

private:
    int32 SelectedSectionIndex    = -1;
    UMaterialInstance* SelectedMaterialPtr = nullptr;  // 원본 포인터 (Apply 대상)

	USceneComponent* SelectedComponent = nullptr;
};
