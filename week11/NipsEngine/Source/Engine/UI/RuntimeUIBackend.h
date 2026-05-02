#pragma once

#include "UI/RuntimeUITypes.h"

class FRuntimeUIWidget;

class IRuntimeUIBackend
{
public:
    virtual ~IRuntimeUIBackend() = default;

    virtual void BeginFrame(const FRuntimeUIRenderContext& Context) = 0;
    virtual void DrawPanel(const FRuntimeUIWidget& Widget) = 0;
    virtual void DrawTextWidget(const FRuntimeUIWidget& Widget) = 0;
    virtual void DrawButton(const FRuntimeUIWidget& Widget) = 0;
    virtual void DrawImage(const FRuntimeUIWidget& Widget) = 0;
    virtual void DrawProgressBar(const FRuntimeUIWidget& Widget) = 0;
    virtual void EndFrame() = 0;
};
