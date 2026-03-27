#pragma once
#include "Core/CoreMinimal.h"
#include "ViewportLayoutManager.h"

class SWindow
{
    private:
    FRect Rect;

    public:
    SWindow() = default;
    virtual ~SWindow() = default;

    bool IsHover(const FVector& Point) const;
};