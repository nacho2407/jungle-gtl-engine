// 에디터 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Editor/UI/EditorPanel.h"
#include "Render/Execute/Passes/Scene/ShadowMapPass.h"
#include "Object/Object.h"

class UActorComponent;
class AActor;
class ULightComponent;
class FEditorDetailsPanel;

void RenderEditorShadowAtlasDebugWindow(FEditorDetailsPanel& DetailsPanel);

// FEditorDetailsPanel는 에디터 UI 표시와 입력 처리를 담당합니다.
class FEditorDetailsPanel : public FEditorPanel
{
public:
    virtual void Render(float DeltaTime) override;

    UActorComponent* GetSelectedComponent() const { return SelectedComponent; }
    bool IsShadowAtlasDebugWindowOpen() const { return bShowShadowAtlasDebugWindow; }
    void SetShadowAtlasDebugWindowOpen(bool bOpen) { bShowShadowAtlasDebugWindow = bOpen; }
    void RenderShadowAtlasDebugWindow();

private:
    friend void RenderEditorShadowAtlasDebugWindow(FEditorDetailsPanel& DetailsPanel);

    struct FShadowAtlasSliceDebugCache
    {
        uint64 Hash = 0;
        TArray<FShadowMapData> Allocations;
    };

    void FocusComponentDetails(UActorComponent* Component, int32 CascadeIndex = -1, int32 FaceIndex = -1);
    void RenderComponentTree(AActor* Actor);
    void RenderSceneComponentNode(class USceneComponent* Comp, UActorComponent*& OutComponentToRemove);
    void RenderDetails(AActor* PrimaryActor, const TArray<AActor*>& SelectedActors);
    void RenderComponentProperties(AActor* Actor);
    void RenderActorProperties(AActor* PrimaryActor, const TArray<AActor*>& SelectedActors);
    void RenderLightShadowSettings(ULightComponent* LightComponent);
    bool RenderDetailsPanel(TArray<struct FPropertyDescriptor>& Props, int32& Index);

    static FString OpenObjFileDialog();

    UActorComponent* SelectedComponent = nullptr;
    AActor* LastSelectedActor = nullptr;
    bool bActorSelected = true; // true: Actor details, false: Component details
    bool bShowShadowAtlasDebugWindow = false;
    int32 SelectedPointLightShadowFace = 0;
    int32 SelectedDirectionalCascade = 0;
    int32 SelectedShadowAtlasPage = 0;
    EShadowDepthPreviewMode ShadowDepthPreviewMode = EShadowDepthPreviewMode::RawDepth;
    EShadowDepthPreviewMode ShadowAtlasDebugPreviewMode = EShadowDepthPreviewMode::RawDepth;
    FShadowAtlasSliceDebugCache ShadowAtlasDebugCache[ShadowAtlas::MaxPages][ShadowAtlas::SliceCount] = {};
};
