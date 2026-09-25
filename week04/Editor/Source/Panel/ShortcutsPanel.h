#pragma once

#include "Panel.h"

class FShortcutsPanel : public IPanel
{
public:
    const wchar_t* GetPanelID() const override;
    const wchar_t* GetDisplayName() const override;
    bool           ShouldShowInWindowMenu() const override { return false; }
    int32          GetWindowMenuOrder() const override { return 40; }

    void Draw() override;
};
