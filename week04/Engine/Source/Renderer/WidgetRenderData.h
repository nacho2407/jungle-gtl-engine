#pragma once
#include "Core/Containers/Array.h"

class SWidget;

struct FViewportOverlayTextItem
{
    FString Text;
    float   X = 0.0f;
    float   Y = 0.0f;
    FColor  Color = FColor::White();
    float   Scale = 1.0f;
};

struct FWidgetRenderData
{
    TArray<SWidget*>                 Widgets;
    TArray<FViewportOverlayTextItem> TextItems;
    uint32                           ScreenWidth = 0;
    uint32                           ScreenHeight = 0;
};
