#pragma once
#include "SWindow.h"

class FWindowOverlayManager
{
  private:
    TArray<FEditorViewportPanel*> ViewportPanels;
  public:
    TArray<FRect> FindViewportDimension(EViewportLayout ViewportLayout, int32 W, int32 H);
    TArray<FEditorViewportPanel*>& GetViewportPanels();

    void Release();
};