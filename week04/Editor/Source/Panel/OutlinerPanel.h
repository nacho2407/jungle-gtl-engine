#pragma once

#include "Panel.h"

class AActor;

class FOutlinerPanel : public IPanel
{
public:
    enum class ESpawnActorType : int32
    {
        Cube = 0,
        Sphere,
        Count
    };

public:
    const wchar_t* GetPanelID() const override;
    const wchar_t* GetDisplayName() const override;
    bool ShouldOpenByDefault() const override { return true; }
    int32 GetWindowMenuOrder() const override { return 0; }

    void Draw() override;

private:
    void DrawToolbar();
    void DrawEmptyState() const;
    void DrawActorRow(AActor* Actor) const;
    void SpawnActors() const;

private:
    ESpawnActorType SpawnActorType = ESpawnActorType::Cube;
    int32 SpawnCount = 1;
};
