#pragma once

#include "Core/Containers/Array.h"
#include "Core/Containers/String.h"
#include "UI/RuntimeUISystem.h"

class FRuntimeUIFacade
{
public:
    explicit FRuntimeUIFacade(FRuntimeUISystem& InUI);

    bool EnsureCanvas(const FString& CanvasId = FRuntimeUISystem::DefaultCanvasId);
    bool EnsureScreen(const FString& ScreenId, const FString& CanvasId = FRuntimeUISystem::DefaultCanvasId);
    bool ShowScreen(const FString& ScreenId, const FString& CanvasId = FRuntimeUISystem::DefaultCanvasId);

    bool CreatePanel(
        const FString& ScreenId,
        const FString& WidgetId,
        const FRuntimeUIVector2& Position,
        const FRuntimeUIVector2& Size,
        const FString& ParentId = FString());

    bool CreateText(
        const FString& ScreenId,
        const FString& WidgetId,
        const FString& Text,
        const FRuntimeUIVector2& Position,
        const FRuntimeUIVector2& Size,
        const FString& ParentId = FString());

    bool CreateButton(
        const FString& ScreenId,
        const FString& WidgetId,
        const FString& Text,
        const FString& ActionEvent,
        const FRuntimeUIVector2& Position,
        const FRuntimeUIVector2& Size,
        const FString& ParentId = FString());

    bool CreateImage(
        const FString& ScreenId,
        const FString& WidgetId,
        const FString& ImagePath,
        const FRuntimeUIVector2& Position,
        const FRuntimeUIVector2& Size,
        const FString& ParentId = FString());

    bool CreateProgressBar(
        const FString& ScreenId,
        const FString& WidgetId,
        float Progress,
        const FRuntimeUIVector2& Position,
        const FRuntimeUIVector2& Size,
        const FString& ParentId = FString());

    bool RemoveWidget(const FString& WidgetId);
    bool SetText(const FString& WidgetId, const FString& Text);
    bool SetImage(const FString& WidgetId, const FString& ImagePath);
    bool SetProgress(const FString& WidgetId, float Value);
    bool SetVisible(const FString& WidgetId, bool bVisible);
    bool SetEnabled(const FString& WidgetId, bool bEnabled);
    bool SetActionEvent(const FString& WidgetId, const FString& EventName);
    bool SetZOrder(const FString& WidgetId, int32 ZOrder);
    bool SetTint(const FString& WidgetId, const FRuntimeUIColor& Color);
    bool SetBackgroundColor(const FString& WidgetId, const FRuntimeUIColor& Color);
    bool SetTextColor(const FString& WidgetId, const FRuntimeUIColor& Color);
    bool SetAlpha(const FString& WidgetId, float Alpha);
    bool SetRounding(const FString& WidgetId, float Rounding);
    bool SetFontScale(const FString& WidgetId, float FontScale);

    bool SetWidgetTransform(
        const FString& WidgetId,
        const FRuntimeUIVector2& Position,
        const FRuntimeUIVector2& Size);

    bool SetWidgetAnchors(
        const FString& WidgetId,
        const FRuntimeUIVector2& AnchorMin,
        const FRuntimeUIVector2& AnchorMax,
        const FRuntimeUIVector2& Pivot);

    bool SetImageOptions(
        const FString& WidgetId,
        const FRuntimeUIUVRect& UV,
        ERuntimeUIImageDrawMode DrawMode,
        const FRuntimeUIMargin& NineSliceBorder = FRuntimeUIMargin());

    bool SetButtonImages(
        const FString& WidgetId,
        const FString& Normal,
        const FString& Hover,
        const FString& Pressed,
        const FString& Disabled);

    bool SetProgressImages(
        const FString& WidgetId,
        const FString& Background,
        const FString& Fill,
        const FString& Frame,
        ERuntimeUIProgressFillDirection FillDirection = ERuntimeUIProgressFillDirection::LeftToRight);

    bool SetSpriteFrame(
        const FString& WidgetId,
        int32 Columns,
        int32 Rows,
        int32 FrameIndex);

    bool AnimateWidgetTransform(
        const FString& WidgetId,
        const FRuntimeUIVector2& ToPosition,
        const FRuntimeUIVector2& ToSize,
        float Duration,
        bool bLoop = false,
        bool bPingPong = false,
        ERuntimeUIAnimationEasing Easing = ERuntimeUIAnimationEasing::Linear);

    bool AnimateWidgetPatrol(
        const FString& WidgetId,
        const FRuntimeUIVector2& FromPosition,
        const FRuntimeUIVector2& ToPosition,
        float Duration,
        bool bLoop = true,
        bool bPingPong = true,
        ERuntimeUIAnimationEasing Easing = ERuntimeUIAnimationEasing::Linear);

    bool StopWidgetAnimation(const FString& WidgetId);

    TArray<FString> PollActionEvents();

private:
    FRuntimeUIWidgetDesc MakeWidgetDesc(
        ERuntimeUIWidgetType Type,
        const FString& WidgetId,
        const FRuntimeUIVector2& Position,
        const FRuntimeUIVector2& Size,
        const FString& ParentId) const;

    FRuntimeUISystem& UI;
};
