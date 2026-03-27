#include "ViewportLayoutManager.h"

TArray<FRect> FViewportLayoutManager::FindViewportDimension(EViewportLayout ViewportLayout, int32 W,
                                                            int32 H)
{
    FRect PlaceHolder(0, 0, 0, 0);
    TArray<FRect> ViewportDim(4, PlaceHolder);
    if (ViewportLayout == EViewportLayout::Single)
    {
        ViewportDim[0] = FRect(0, 0, W, H);
    }
    else if (ViewportLayout == EViewportLayout::TwoColumn)
    {
        ViewportDim[0] = FRect(0, 0, W / 2, H);
        ViewportDim[1] = FRect(W / 2, 0, W / 2, H);
    }
    else if (ViewportLayout == EViewportLayout::TwoRow)
    {
        ViewportDim[0] = FRect(0, 0, W, H / 2);
        ViewportDim[1] = FRect(0, H / 2, 0, H / 2);
    }
    else if (ViewportLayout == EViewportLayout::ColumnTwoRow)
    {
        ViewportDim[0] = FRect(0, 0, W / 2, 0);
        ViewportDim[1] = FRect(W / 2, 0, W / 2, H / 2);
        ViewportDim[2] = FRect(W / 2, H / 2, W / 2, H / 2);
    }
    else if (ViewportLayout == EViewportLayout::TwoRowColumn)
    {
        ViewportDim[0] = FRect(0, 0, W / 2, H / 2);
        ViewportDim[1] = FRect(0, H / 2, W / 2, H / 2);
        ViewportDim[2] = FRect(W / 2, 0, W / 2, H);
    }
    else if (ViewportLayout == EViewportLayout::FourWay)
    {
        ViewportDim[0] = FRect(0, 0, W / 2, H / 2);
        ViewportDim[1] = FRect(0, H / 2, W / 2, H / 2);
        ViewportDim[2] = FRect(W / 2, 0, W / 2, H / 2);
        ViewportDim[3] = FRect(W / 2, H / 2, W / 2, H / 2);
    }

    return ViewportDim;
}