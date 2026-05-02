#include "UI/RuntimeUIWidget.h"

#include <algorithm>

FRuntimeUIWidget::FRuntimeUIWidget(const FRuntimeUIWidgetDesc& InDesc)
    : Id(InDesc.Id)
    , ParentId(InDesc.ParentId)
    , Type(InDesc.Type)
    , AnchorMin(InDesc.AnchorMin)
    , AnchorMax(InDesc.AnchorMax)
    , Pivot(InDesc.Pivot)
    , Position(InDesc.Position)
    , Size(InDesc.Size)
    , Text(InDesc.Text)
    , ImagePath(InDesc.ImagePath)
    , ImageUV(InDesc.ImageUV)
    , ImageDrawMode(InDesc.ImageDrawMode)
    , NineSliceBorder(InDesc.NineSliceBorder)
    , ButtonNormalImagePath(InDesc.ButtonNormalImagePath)
    , ButtonHoverImagePath(InDesc.ButtonHoverImagePath)
    , ButtonPressedImagePath(InDesc.ButtonPressedImagePath)
    , ButtonDisabledImagePath(InDesc.ButtonDisabledImagePath)
    , ProgressBackgroundImagePath(InDesc.ProgressBackgroundImagePath)
    , ProgressFillImagePath(InDesc.ProgressFillImagePath)
    , ProgressFrameImagePath(InDesc.ProgressFrameImagePath)
    , ProgressFillDirection(InDesc.ProgressFillDirection)
    , ActionEvent(InDesc.ActionEvent)
    , Progress(RuntimeUIClamp01(InDesc.Progress))
    , ZOrder(InDesc.ZOrder)
    , bVisible(InDesc.bVisible)
    , bEnabled(InDesc.bEnabled)
    , bInteractable(InDesc.bInteractable)
    , Style(InDesc.Style)
{
}

void FRuntimeUIWidget::AddChild(const FString& ChildId)
{
    if (std::find(Children.begin(), Children.end(), ChildId) == Children.end())
    {
        Children.push_back(ChildId);
    }
}

void FRuntimeUIWidget::RemoveChild(const FString& ChildId)
{
    Children.erase(std::remove(Children.begin(), Children.end(), ChildId), Children.end());
}

void FRuntimeUIWidget::SetAnchors(const FRuntimeUIVector2& InAnchorMin, const FRuntimeUIVector2& InAnchorMax)
{
    AnchorMin = InAnchorMin;
    AnchorMax = InAnchorMax;
}

void FRuntimeUIWidget::SetButtonImages(
    const FString& Normal,
    const FString& Hover,
    const FString& Pressed,
    const FString& Disabled)
{
    ButtonNormalImagePath = Normal;
    ButtonHoverImagePath = Hover;
    ButtonPressedImagePath = Pressed;
    ButtonDisabledImagePath = Disabled;
}

void FRuntimeUIWidget::SetProgressImages(const FString& Background, const FString& Fill, const FString& Frame)
{
    ProgressBackgroundImagePath = Background;
    ProgressFillImagePath = Fill;
    ProgressFrameImagePath = Frame;
}

const FString& FRuntimeUIWidget::GetCurrentButtonImagePath() const
{
    if (!IsComputedEnabled() && !ButtonDisabledImagePath.empty())
    {
        return ButtonDisabledImagePath;
    }
    if (IsPressed() && !ButtonPressedImagePath.empty())
    {
        return ButtonPressedImagePath;
    }
    if (IsHovered() && !ButtonHoverImagePath.empty())
    {
        return ButtonHoverImagePath;
    }
    return ButtonNormalImagePath;
}

void FRuntimeUIWidget::ComputeLayout(
    const FRuntimeUIRect& ParentRect,
    bool bParentVisible,
    bool bParentEnabled,
    float ParentAlpha,
    float LayoutScale)
{
    const FRuntimeUIVector2 ParentSize = ParentRect.Size;
    const FRuntimeUIVector2 AnchorPoint(
        ParentRect.Min.X + ParentSize.X * AnchorMin.X,
        ParentRect.Min.Y + ParentSize.Y * AnchorMin.Y);

    const FRuntimeUIVector2 ScaledPosition = Position * LayoutScale;
    const FRuntimeUIVector2 ScaledSize = Size * LayoutScale;
    FRuntimeUIVector2 ComputedSize = ScaledSize;
    if (AnchorMin.X != AnchorMax.X)
    {
        ComputedSize.X = ParentSize.X * (AnchorMax.X - AnchorMin.X) + ScaledSize.X;
    }
    if (AnchorMin.Y != AnchorMax.Y)
    {
        ComputedSize.Y = ParentSize.Y * (AnchorMax.Y - AnchorMin.Y) + ScaledSize.Y;
    }

    const FRuntimeUIVector2 PivotOffset(ComputedSize.X * Pivot.X, ComputedSize.Y * Pivot.Y);
    ComputedRect = FRuntimeUIRect(AnchorPoint + ScaledPosition - PivotOffset, ComputedSize);
    ComputedAlpha = ParentAlpha * RuntimeUIClamp01(Style.Alpha);
    bComputedVisible = bParentVisible && bVisible && ComputedAlpha > 0.0f;
    bComputedEnabled = bParentEnabled && bEnabled;
}
