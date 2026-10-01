// 에디터 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once
#include "EditorPanel.h"
#include <functional>

class USceneComponent;
class UPrimitiveComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UDecalComponent;
class UMaterialInstanceDynamic;

// FEditorMaterialPanel는 머티리얼 파라미터와 렌더 리소스를 다룹니다.
class FEditorMaterialPanel : public FEditorPanel
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
    UMaterialInstanceDynamic* SelectedMaterialPtr = nullptr; // 원본 포인터 (Apply 대상)

    USceneComponent* SelectedComponent = nullptr;
};
