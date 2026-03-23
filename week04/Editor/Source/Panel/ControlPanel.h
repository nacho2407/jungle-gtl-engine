#pragma once

#include "Panel.h"

class FViewportCamera;

class FControlPanel : public IPanel
{
public:
    const wchar_t* GetPanelID() const override;
    const wchar_t* GetDisplayName() const override;
    bool ShouldOpenByDefault() const override { return true; }
    int32 GetWindowMenuOrder() const override { return 20; }

    void Draw() override;

private:
    FViewportCamera* ResolveViewportCamera() const;
    void DrawUnavailableState() const;
    void DrawTransformSection(FViewportCamera& Camera) const;
    void DrawProjectionSection(FViewportCamera& Camera) const;
};
