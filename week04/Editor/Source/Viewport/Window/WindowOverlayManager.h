#pragma once
#include "SWindow.h"
#include "Viewport/EditorViewportClient.h"

struct FEditorContext;
class  FScene;

namespace
{
    float DefaultMoveSpeed = 100.0f;
    float DefaultRotationSpeed = 0.3f;
}

class FWindowOverlayManager
{
  private:
    EViewportLayout               ViewportLayout = EViewportLayout::Single; // Defines how the viewports should be aligned
    TArray<FEditorViewportPanel*> ViewportPanels;
    FEditorContext*               EditorContext = nullptr;
    FScene*                       Scene = nullptr;

    // Global window dimension
    uint32 W = 0;
    uint32 H = 0;

    FEditorViewportClient::FPickCallback PickCallback;

  public:
    void                           ResetViewportDimension();
    TArray<FEditorViewportPanel*>& GetViewportPanels();
    void                           AddNewViewportPanel();
    FEditorViewportPanel*          FindPanelAtPoint(int32 X, int32 Y) const;

    // Deletes and nullifies the heap memory occupied by dynamically allocated panels
    void Release();

    // Toggled when the entire window size is changed
    void SetWindowDimension(uint32 Width, uint32 Height);

    // Sets a lambda function to use for viewport screen interactions
    void SetPickCallback(FEditorViewportClient::FPickCallback Callback);

    // Sets an editor context for each viewport panel to use
    void SetEditorContext(FEditorContext* InEditorContext) { EditorContext = InEditorContext; }

    // Sets the scene to render for ALL viewports. In other words, the viewports share the same rendering scene.
    void SetScene(FScene* InScene);

    // Modifies how camera should be laid out onto the viewport. Falls back to their default dimension upon change
    void SetViewportLayout(EViewportLayout ViewportLayout);

    // Resets camera movement settings to their default value
    void SetNavigationValues(float MoveSpeed, float RotationSpeed);
};