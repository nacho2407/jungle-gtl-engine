#pragma once

#include "Panel.h"

class AActor;

namespace Engine::Component
{
    class USceneComponent;
}

class FPropertiesPanel : public IPanel
{
public:
    const wchar_t* GetPanelID() const override;
    const wchar_t* GetDisplayName() const override;
    bool ShouldOpenByDefault() const override { return true; }
    int32 GetWindowMenuOrder() const override { return 10; }

    void Draw() override;

private:
    Engine::Component::USceneComponent* ResolveTargetComponent(AActor*& OutSelectedActor) const;
    void DrawNoSelectionState() const;
    void DrawUnsupportedSelectionState() const;
    void DrawSelectionSummary(AActor* SelectedActor,
                              Engine::Component::USceneComponent* TargetComponent) const;
    void DrawTransformEditor(Engine::Component::USceneComponent* TargetComponent) const;
};
