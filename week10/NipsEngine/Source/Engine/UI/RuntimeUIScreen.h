#pragma once

#include "Core/Containers/Array.h"
#include "Core/Containers/String.h"

class FRuntimeUIScreen
{
public:
    FRuntimeUIScreen() = default;
    FRuntimeUIScreen(const FString& InId, const FString& InCanvasId)
        : Id(InId)
        , CanvasId(InCanvasId)
    {
    }

    const FString& GetId() const { return Id; }
    const FString& GetCanvasId() const { return CanvasId; }
    const TArray<FString>& GetRootWidgetIds() const { return RootWidgetIds; }

    void AddRootWidget(const FString& WidgetId);
    void RemoveRootWidget(const FString& WidgetId);

    void SetVisible(bool bInVisible) { bVisible = bInVisible; }
    bool IsVisible() const { return bVisible; }

private:
    FString Id;
    FString CanvasId;
    TArray<FString> RootWidgetIds;
    bool bVisible = true;
};
