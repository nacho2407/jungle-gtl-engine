#include "UI/RuntimeUIFacade.h"

#include <algorithm>

FRuntimeUIFacade::FRuntimeUIFacade(FRuntimeUISystem& InUI)
    : UI(InUI)
{
}

bool FRuntimeUIFacade::EnsureCanvas(const FString& CanvasId)
{
    return UI.FindCanvas(CanvasId) != nullptr || UI.CreateCanvas(CanvasId);
}

bool FRuntimeUIFacade::EnsureScreen(const FString& ScreenId, const FString& CanvasId)
{
    return UI.FindScreen(ScreenId) != nullptr || (EnsureCanvas(CanvasId) && UI.CreateScreen(ScreenId, CanvasId));
}

bool FRuntimeUIFacade::ShowScreen(const FString& ScreenId, const FString& CanvasId)
{
    return EnsureScreen(ScreenId, CanvasId) && UI.SetActiveScreen(CanvasId, ScreenId);
}

bool FRuntimeUIFacade::CreatePanel(
    const FString& ScreenId,
    const FString& WidgetId,
    const FRuntimeUIVector2& Position,
    const FRuntimeUIVector2& Size,
    const FString& ParentId)
{
    FRuntimeUIWidgetDesc Desc = MakeWidgetDesc(ERuntimeUIWidgetType::Panel, WidgetId, Position, Size, ParentId);
    return UI.CreateWidget(ScreenId, Desc);
}

bool FRuntimeUIFacade::CreateText(
    const FString& ScreenId,
    const FString& WidgetId,
    const FString& Text,
    const FRuntimeUIVector2& Position,
    const FRuntimeUIVector2& Size,
    const FString& ParentId)
{
    FRuntimeUIWidgetDesc Desc = MakeWidgetDesc(ERuntimeUIWidgetType::Text, WidgetId, Position, Size, ParentId);
    Desc.Text = Text;
    return UI.CreateWidget(ScreenId, Desc);
}

bool FRuntimeUIFacade::CreateButton(
    const FString& ScreenId,
    const FString& WidgetId,
    const FString& Text,
    const FString& ActionEvent,
    const FRuntimeUIVector2& Position,
    const FRuntimeUIVector2& Size,
    const FString& ParentId)
{
    FRuntimeUIWidgetDesc Desc = MakeWidgetDesc(ERuntimeUIWidgetType::Button, WidgetId, Position, Size, ParentId);
    Desc.Text = Text;
    Desc.ActionEvent = ActionEvent;
    return UI.CreateWidget(ScreenId, Desc);
}

bool FRuntimeUIFacade::CreateImage(
    const FString& ScreenId,
    const FString& WidgetId,
    const FString& ImagePath,
    const FRuntimeUIVector2& Position,
    const FRuntimeUIVector2& Size,
    const FString& ParentId)
{
    FRuntimeUIWidgetDesc Desc = MakeWidgetDesc(ERuntimeUIWidgetType::Image, WidgetId, Position, Size, ParentId);
    Desc.ImagePath = ImagePath;
    return UI.CreateWidget(ScreenId, Desc);
}

bool FRuntimeUIFacade::CreateProgressBar(
    const FString& ScreenId,
    const FString& WidgetId,
    float Progress,
    const FRuntimeUIVector2& Position,
    const FRuntimeUIVector2& Size,
    const FString& ParentId)
{
    FRuntimeUIWidgetDesc Desc = MakeWidgetDesc(ERuntimeUIWidgetType::ProgressBar, WidgetId, Position, Size, ParentId);
    Desc.Progress = RuntimeUIClamp01(Progress);
    return UI.CreateWidget(ScreenId, Desc);
}

bool FRuntimeUIFacade::RemoveWidget(const FString& WidgetId)
{
    return UI.RemoveWidget(WidgetId);
}

bool FRuntimeUIFacade::SetText(const FString& WidgetId, const FString& Text)
{
    return UI.SetText(WidgetId, Text);
}

bool FRuntimeUIFacade::SetImage(const FString& WidgetId, const FString& ImagePath)
{
    return UI.SetImage(WidgetId, ImagePath);
}

bool FRuntimeUIFacade::SetProgress(const FString& WidgetId, float Value)
{
    return UI.SetProgress(WidgetId, Value);
}

bool FRuntimeUIFacade::SetVisible(const FString& WidgetId, bool bVisible)
{
    return UI.SetVisible(WidgetId, bVisible);
}

bool FRuntimeUIFacade::SetEnabled(const FString& WidgetId, bool bEnabled)
{
    return UI.SetEnabled(WidgetId, bEnabled);
}

bool FRuntimeUIFacade::SetActionEvent(const FString& WidgetId, const FString& EventName)
{
    return UI.SetActionEvent(WidgetId, EventName);
}

bool FRuntimeUIFacade::SetZOrder(const FString& WidgetId, int32 ZOrder)
{
    return UI.SetZOrder(WidgetId, ZOrder);
}

bool FRuntimeUIFacade::SetTint(const FString& WidgetId, const FRuntimeUIColor& Color)
{
    return UI.SetTint(WidgetId, Color);
}

bool FRuntimeUIFacade::SetBackgroundColor(const FString& WidgetId, const FRuntimeUIColor& Color)
{
    return UI.SetBackgroundColor(WidgetId, Color);
}

bool FRuntimeUIFacade::SetTextColor(const FString& WidgetId, const FRuntimeUIColor& Color)
{
    return UI.SetTextColor(WidgetId, Color);
}

bool FRuntimeUIFacade::SetAlpha(const FString& WidgetId, float Alpha)
{
    return UI.SetAlpha(WidgetId, Alpha);
}

bool FRuntimeUIFacade::SetRounding(const FString& WidgetId, float Rounding)
{
    return UI.SetRounding(WidgetId, Rounding);
}

bool FRuntimeUIFacade::SetFontScale(const FString& WidgetId, float FontScale)
{
    return UI.SetFontScale(WidgetId, FontScale);
}

bool FRuntimeUIFacade::SetWidgetTransform(
    const FString& WidgetId,
    const FRuntimeUIVector2& Position,
    const FRuntimeUIVector2& Size)
{
    FRuntimeUIWidget* Widget = UI.FindWidget(WidgetId);
    if (!Widget)
    {
        return false;
    }

    Widget->SetLocalPosition(Position);
    Widget->SetSize(Size);
    return true;
}

bool FRuntimeUIFacade::SetWidgetAnchors(
    const FString& WidgetId,
    const FRuntimeUIVector2& AnchorMin,
    const FRuntimeUIVector2& AnchorMax,
    const FRuntimeUIVector2& Pivot)
{
    FRuntimeUIWidget* Widget = UI.FindWidget(WidgetId);
    if (!Widget)
    {
        return false;
    }

    Widget->SetAnchors(AnchorMin, AnchorMax);
    Widget->SetPivot(Pivot);
    return true;
}

bool FRuntimeUIFacade::SetImageOptions(
    const FString& WidgetId,
    const FRuntimeUIUVRect& UV,
    ERuntimeUIImageDrawMode DrawMode,
    const FRuntimeUIMargin& NineSliceBorder)
{
    FRuntimeUIWidget* Widget = UI.FindWidget(WidgetId);
    if (!Widget)
    {
        return false;
    }

    Widget->SetImageUV(UV);
    Widget->SetImageDrawMode(DrawMode);
    Widget->SetNineSliceBorder(NineSliceBorder);
    return true;
}

bool FRuntimeUIFacade::SetButtonImages(
    const FString& WidgetId,
    const FString& Normal,
    const FString& Hover,
    const FString& Pressed,
    const FString& Disabled)
{
    FRuntimeUIWidget* Widget = UI.FindWidget(WidgetId);
    if (!Widget)
    {
        return false;
    }

    Widget->SetButtonImages(Normal, Hover, Pressed, Disabled);
    return true;
}

bool FRuntimeUIFacade::SetProgressImages(
    const FString& WidgetId,
    const FString& Background,
    const FString& Fill,
    const FString& Frame,
    ERuntimeUIProgressFillDirection FillDirection)
{
    FRuntimeUIWidget* Widget = UI.FindWidget(WidgetId);
    if (!Widget)
    {
        return false;
    }

    Widget->SetProgressImages(Background, Fill, Frame);
    Widget->SetProgressFillDirection(FillDirection);
    return true;
}

bool FRuntimeUIFacade::SetSpriteFrame(
    const FString& WidgetId,
    int32 Columns,
    int32 Rows,
    int32 FrameIndex)
{
    FRuntimeUIWidget* Widget = UI.FindWidget(WidgetId);
    if (!Widget || Columns <= 0 || Rows <= 0)
    {
        return false;
    }

    const int32 TotalFrames = Columns * Rows;
    const int32 ClampedFrame = std::clamp(FrameIndex, 0, TotalFrames - 1);
    const int32 Col = ClampedFrame % Columns;
    const int32 Row = ClampedFrame / Columns;
    const float CellW = 1.0f / static_cast<float>(Columns);
    const float CellH = 1.0f / static_cast<float>(Rows);

    FRuntimeUIUVRect UV;
    UV.Min = FRuntimeUIVector2(CellW * static_cast<float>(Col), CellH * static_cast<float>(Row));
    UV.Max = FRuntimeUIVector2(UV.Min.X + CellW, UV.Min.Y + CellH);
    Widget->SetImageUV(UV);
    return true;
}

bool FRuntimeUIFacade::AnimateWidgetTransform(
    const FString& WidgetId,
    const FRuntimeUIVector2& ToPosition,
    const FRuntimeUIVector2& ToSize,
    float Duration,
    bool bLoop,
    bool bPingPong,
    ERuntimeUIAnimationEasing Easing)
{
    return UI.AnimateTransform(WidgetId, ToPosition, ToSize, Duration, bLoop, bPingPong, Easing);
}

bool FRuntimeUIFacade::AnimateWidgetPatrol(
    const FString& WidgetId,
    const FRuntimeUIVector2& FromPosition,
    const FRuntimeUIVector2& ToPosition,
    float Duration,
    bool bLoop,
    bool bPingPong,
    ERuntimeUIAnimationEasing Easing)
{
    return UI.AnimatePatrol(WidgetId, FromPosition, ToPosition, Duration, bLoop, bPingPong, Easing);
}

bool FRuntimeUIFacade::StopWidgetAnimation(const FString& WidgetId)
{
    return UI.StopAnimation(WidgetId);
}

TArray<FString> FRuntimeUIFacade::PollActionEvents()
{
    return UI.ConsumeActionEvents();
}

FRuntimeUIWidgetDesc FRuntimeUIFacade::MakeWidgetDesc(
    ERuntimeUIWidgetType Type,
    const FString& WidgetId,
    const FRuntimeUIVector2& Position,
    const FRuntimeUIVector2& Size,
    const FString& ParentId) const
{
    FRuntimeUIWidgetDesc Desc;
    Desc.Id = WidgetId;
    Desc.ParentId = ParentId;
    Desc.Type = Type;
    Desc.Position = Position;
    Desc.Size = Size;
    return Desc;
}
