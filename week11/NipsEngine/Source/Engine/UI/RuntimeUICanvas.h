#pragma once

#include "Core/Containers/String.h"
#include "UI/RuntimeUITypes.h"

class FRuntimeUICanvas
{
public:
    FRuntimeUICanvas() = default;
    explicit FRuntimeUICanvas(const FString& InId)
        : Id(InId)
    {
    }

    const FString& GetId() const { return Id; }

    void SetActiveScreenId(const FString& InScreenId) { ActiveScreenId = InScreenId; }
    const FString& GetActiveScreenId() const { return ActiveScreenId; }

    void SetVisible(bool bInVisible) { bVisible = bInVisible; }
    bool IsVisible() const { return bVisible; }

    void SetEnabled(bool bInEnabled) { bEnabled = bInEnabled; }
    bool IsEnabled() const { return bEnabled; }

    void SetScaleMode(ERuntimeUICanvasScaleMode InScaleMode) { ScaleMode = InScaleMode; }
    ERuntimeUICanvasScaleMode GetScaleMode() const { return ScaleMode; }

    void SetReferenceResolution(const FRuntimeUIVector2& InReferenceResolution) { ReferenceResolution = InReferenceResolution; }
    const FRuntimeUIVector2& GetReferenceResolution() const { return ReferenceResolution; }

    void UpdateFromRenderContext(const FRuntimeUIRenderContext& Context);

    const FRuntimeUIRect& GetComputedRect() const { return ComputedRect; }
    float GetComputedScale() const { return ComputedScale; }

private:
    FString Id;
    FString ActiveScreenId;
    bool bVisible = true;
    bool bEnabled = true;
    ERuntimeUICanvasScaleMode ScaleMode = ERuntimeUICanvasScaleMode::ScaleWithScreenSize;
    FRuntimeUIVector2 ReferenceResolution = FRuntimeUIVector2(1920.0f, 1080.0f);

    FRuntimeUIRect ComputedRect;
    float ComputedScale = 1.0f;
};
