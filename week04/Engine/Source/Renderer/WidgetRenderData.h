#pragma once

#include "Core/CoreMinimal.h"

class SWidget;

struct FWidgetRenderData
{
    TArray<SWidget*> Widgets;
    uint32           ScreenWidth = 0;
    uint32           ScreenHeight = 0;
};
