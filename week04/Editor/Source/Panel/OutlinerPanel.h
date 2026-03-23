#pragma once

#include "Panel.h"

class AActor;

class FOutlinerPanel : public IPanel
{
public:
    const wchar_t* GetPanelID() const override;
    const wchar_t* GetDisplayName() const override;
    bool ShouldOpenByDefault() const override { return true; }
    int32 GetWindowMenuOrder() const override { return 0; }

    void Draw() override;

private:
    void DrawToolbar() const;
    void DrawEmptyState() const;
    void DrawActorRow(AActor* Actor) const;
    void SpawnCubeActor() const;
};
