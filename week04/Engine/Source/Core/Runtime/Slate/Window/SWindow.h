#pragma once
#include "Core/Runtime/Slate/SWidget.h"

enum class EViewportLayout
{
    Single,
    TwoColumn,
    TwoRow,
    ColumnTwoRow,
    TwoRowColumn,
    FourWay
};


class ENGINE_API SWindow : public SWidget
{
protected:
    SWindow() = default;

public:
    float PosX   = 0.f;
    float PosY   = 0.f;
    float Width  = 0.f;
    float Height = 0.f;

    bool HitTest(FVector2 P) const override
    {
        return (P.X >= PosX && P.X < PosX + Width &&
                P.Y >= PosY && P.Y < PosY + Height);
    }

    virtual ~SWindow() = default;
};
