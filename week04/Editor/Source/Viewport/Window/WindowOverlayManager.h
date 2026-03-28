#pragma once
#include "SWindow.h"
#include "Viewport/EditorViewportClient.h"

struct FEditorContext;
class  FScene;

namespace
{
    
}

class FWindowOverlayManager
{
  private:
    EViewportLayout               ViewportLayout = EViewportLayout::Single;
    TArray<FEditorViewportPanel*> ViewportPanels;
    FEditorContext*               EditorContext = nullptr;
    FScene*                       Scene = nullptr;
    //bool                          bAreViewportsDirty = false;

    uint32 W = 0;
    uint32 H = 0;

    FEditorViewportClient::FPickCallback PickCallback;

  public:
    void                           ResetViewportDimension();
    TArray<FEditorViewportPanel*>& GetViewportPanels();
    void                           AddNewViewportPanel();
    FEditorViewportPanel*          FindPanelAtPoint(int32 X, int32 Y) const;

    void Release();

    void SetWindowDimension(uint32 Width, uint32 Height);
    void SetPickCallback(FEditorViewportClient::FPickCallback Callback);
    void SetEditorContext(FEditorContext* InEditorContext) { EditorContext = InEditorContext; }
    void SetScene(FScene* InScene);
    void SetViewportLayout(EViewportLayout ViewportLayout);
};