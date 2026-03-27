#pragma once
#include "Core/CoreMinimal.h"
#include "EditorViewportPanel.h"

enum class EViewportLayout
{
    Single,
    TwoColumn,
    TwoRow,
    ColumnTwoRow,
    TwoRowColumn,
    FourWay
};

struct FRect
{
    FRect(int32 X_, int32 Y_, int32 W_, int32 H_) : X(X_), Y(Y_), W(W_), H(H_) {};
    int32 X;
    int32 Y;
    int32 W;
    int32 H;
};

class SWindow
{
    private:
    FRect WindowRect;

    public:
    SWindow() = default;
    virtual ~SWindow() = default;

    bool IsHover(const FVector& Point) const { return true; }
};