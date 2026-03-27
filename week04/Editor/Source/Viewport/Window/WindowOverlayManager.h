#pragma once
#include "SWindow.h"

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
  public:
    void                           ResetViewportDimension();
    TArray<FEditorViewportPanel*>& GetViewportPanels();
    void                           AddNewViewportPanel();

    void Release();

    void SetWindowDimension(uint32 Width, uint32 Height);
    void SetEditorContext(FEditorContext* InEditorContext) { EditorContext = InEditorContext; }
    void SetScene(FScene* InScene) { Scene = InScene; }
    void SetViewportLayout(EViewportLayout ViewportLayout);
};