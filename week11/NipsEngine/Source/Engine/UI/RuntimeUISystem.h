#pragma once

#include "Core/Containers/Array.h"
#include "Core/Containers/Map.h"
#include "Core/Containers/String.h"
#include "UI/RuntimeUICanvas.h"
#include "UI/RuntimeUIInput.h"
#include "UI/RuntimeUIScreen.h"
#include "UI/RuntimeUIWidget.h"

class IRuntimeUIBackend;

struct FRuntimeUIWidgetAnimation
{
    FRuntimeUIVector2 FromPosition;
    FRuntimeUIVector2 ToPosition;
    FRuntimeUIVector2 FromSize;
    FRuntimeUIVector2 ToSize;
    float Duration = 0.0f;
    float Elapsed = 0.0f;
    bool bLoop = false;
    bool bPingPong = false;
    ERuntimeUIAnimationEasing Easing = ERuntimeUIAnimationEasing::Linear;
};

class FRuntimeUISystem
{
public:
    static constexpr const char* DefaultCanvasId = "Main";

    FRuntimeUISystem();

    bool CreateCanvas(const FString& CanvasId);
    bool CreateScreen(const FString& ScreenId, const FString& CanvasId = DefaultCanvasId);
    bool SetActiveScreen(const FString& CanvasId, const FString& ScreenId);

    bool CreateWidget(const FString& ScreenId, const FRuntimeUIWidgetDesc& Desc);
    bool RemoveWidget(const FString& WidgetId);

    FRuntimeUICanvas* FindCanvas(const FString& CanvasId);
    const FRuntimeUICanvas* FindCanvas(const FString& CanvasId) const;
    FRuntimeUIScreen* FindScreen(const FString& ScreenId);
    const FRuntimeUIScreen* FindScreen(const FString& ScreenId) const;
    FRuntimeUIWidget* FindWidget(const FString& WidgetId);
    const FRuntimeUIWidget* FindWidget(const FString& WidgetId) const;

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
    bool AnimateTransform(
        const FString& WidgetId,
        const FRuntimeUIVector2& ToPosition,
        const FRuntimeUIVector2& ToSize,
        float Duration,
        bool bLoop = false,
        bool bPingPong = false,
        ERuntimeUIAnimationEasing Easing = ERuntimeUIAnimationEasing::Linear);
    bool AnimatePatrol(
        const FString& WidgetId,
        const FRuntimeUIVector2& FromPosition,
        const FRuntimeUIVector2& ToPosition,
        float Duration,
        bool bLoop = true,
        bool bPingPong = true,
        ERuntimeUIAnimationEasing Easing = ERuntimeUIAnimationEasing::Linear);
    bool StopAnimation(const FString& WidgetId);

    void UpdateLayout(const FRuntimeUIRenderContext& Context);
    void Render(IRuntimeUIBackend& Backend, const FRuntimeUIRenderContext& Context);
    bool HandleInput(const FRuntimeUIInputEvent& Event);

    void PushActionEvent(const FString& EventName);
    TArray<FString> ConsumeActionEvents();

    const FString& GetHoveredWidgetId() const { return HoveredWidgetId; }
    const FString& GetPressedWidgetId() const { return PressedWidgetId; }

    const TMap<FString, FRuntimeUICanvas>& GetCanvases() const { return Canvases; }
    const TMap<FString, FRuntimeUIScreen>& GetScreens() const { return Screens; }
    const TMap<FString, FRuntimeUIWidget>& GetWidgets() const { return Widgets; }

private:
    void TickAnimations(float DeltaTime);
    void ComputeWidgetTree(FRuntimeUIWidget& Widget, const FRuntimeUIRect& ParentRect, bool bParentVisible, bool bParentEnabled, float ParentAlpha);
    void RenderWidgetTree(IRuntimeUIBackend& Backend, const FRuntimeUIWidget& Widget) const;
    void DrawWidget(IRuntimeUIBackend& Backend, const FRuntimeUIWidget& Widget) const;
    FRuntimeUIWidget* HitTest(const FRuntimeUIVector2& ScreenPosition);
    FRuntimeUIWidget* HitTestWidgetTree(FRuntimeUIWidget& Widget, const FRuntimeUIVector2& ScreenPosition);
    void SetHoveredWidget(const FString& WidgetId);
    void SetPressedWidget(const FString& WidgetId);
    bool IsWidgetInputTarget(const FRuntimeUIWidget& Widget) const;
    void DetachWidgetFromParentOrScreen(const FString& WidgetId);
    void RemoveWidgetRecursive(const FString& WidgetId);

    TMap<FString, FRuntimeUICanvas> Canvases;
    TMap<FString, FRuntimeUIScreen> Screens;
    TMap<FString, FRuntimeUIWidget> Widgets;
    TMap<FString, FRuntimeUIWidgetAnimation> Animations;
    TArray<FString> PendingActionEvents;
    FString HoveredWidgetId;
    FString PressedWidgetId;
};
