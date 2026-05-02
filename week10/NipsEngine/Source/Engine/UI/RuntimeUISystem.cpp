#include "UI/RuntimeUISystem.h"

#include "UI/RuntimeUIBackend.h"

#include <algorithm>
#include <cmath>

namespace
{
    float ApplyAnimationEasing(float Alpha, ERuntimeUIAnimationEasing Easing)
    {
        Alpha = RuntimeUIClamp01(Alpha);

        switch (Easing)
        {
        case ERuntimeUIAnimationEasing::EaseIn:
            return Alpha * Alpha;
        case ERuntimeUIAnimationEasing::EaseOut:
        {
            const float Inv = 1.0f - Alpha;
            return 1.0f - Inv * Inv;
        }
        case ERuntimeUIAnimationEasing::EaseInOut:
            if (Alpha < 0.5f)
            {
                return 2.0f * Alpha * Alpha;
            }
            else
            {
                const float Inv = -2.0f * Alpha + 2.0f;
                return 1.0f - (Inv * Inv) * 0.5f;
            }
        case ERuntimeUIAnimationEasing::SmoothStep:
            return Alpha * Alpha * (3.0f - 2.0f * Alpha);
        case ERuntimeUIAnimationEasing::Linear:
        default:
            return Alpha;
        }
    }
}

FRuntimeUISystem::FRuntimeUISystem()
{
    CreateCanvas(DefaultCanvasId);
}

bool FRuntimeUISystem::CreateCanvas(const FString& CanvasId)
{
    if (CanvasId.empty() || Canvases.find(CanvasId) != Canvases.end())
    {
        return false;
    }

    Canvases.emplace(CanvasId, FRuntimeUICanvas(CanvasId));
    return true;
}

bool FRuntimeUISystem::CreateScreen(const FString& ScreenId, const FString& CanvasId)
{
    if (ScreenId.empty() || Screens.find(ScreenId) != Screens.end())
    {
        return false;
    }

    if (Canvases.find(CanvasId) == Canvases.end())
    {
        return false;
    }

    Screens.emplace(ScreenId, FRuntimeUIScreen(ScreenId, CanvasId));
    FRuntimeUICanvas& Canvas = Canvases[CanvasId];
    if (Canvas.GetActiveScreenId().empty())
    {
        Canvas.SetActiveScreenId(ScreenId);
    }
    return true;
}

bool FRuntimeUISystem::SetActiveScreen(const FString& CanvasId, const FString& ScreenId)
{
    FRuntimeUICanvas* Canvas = FindCanvas(CanvasId);
    const FRuntimeUIScreen* Screen = FindScreen(ScreenId);
    if (!Canvas || !Screen || Screen->GetCanvasId() != CanvasId)
    {
        return false;
    }

    Canvas->SetActiveScreenId(ScreenId);
    return true;
}

bool FRuntimeUISystem::CreateWidget(const FString& ScreenId, const FRuntimeUIWidgetDesc& Desc)
{
    FRuntimeUIScreen* Screen = FindScreen(ScreenId);
    if (!Screen || Desc.Id.empty() || Widgets.find(Desc.Id) != Widgets.end())
    {
        return false;
    }

    if (!Desc.ParentId.empty() && Widgets.find(Desc.ParentId) == Widgets.end())
    {
        return false;
    }

    Widgets.emplace(Desc.Id, FRuntimeUIWidget(Desc));
    if (Desc.ParentId.empty())
    {
        Screen->AddRootWidget(Desc.Id);
    }
    else
    {
        Widgets[Desc.ParentId].AddChild(Desc.Id);
    }

    return true;
}

bool FRuntimeUISystem::RemoveWidget(const FString& WidgetId)
{
    if (Widgets.find(WidgetId) == Widgets.end())
    {
        return false;
    }

    Animations.erase(WidgetId);
    DetachWidgetFromParentOrScreen(WidgetId);
    RemoveWidgetRecursive(WidgetId);
    return true;
}

FRuntimeUICanvas* FRuntimeUISystem::FindCanvas(const FString& CanvasId)
{
    auto It = Canvases.find(CanvasId);
    return It != Canvases.end() ? &It->second : nullptr;
}

const FRuntimeUICanvas* FRuntimeUISystem::FindCanvas(const FString& CanvasId) const
{
    auto It = Canvases.find(CanvasId);
    return It != Canvases.end() ? &It->second : nullptr;
}

FRuntimeUIScreen* FRuntimeUISystem::FindScreen(const FString& ScreenId)
{
    auto It = Screens.find(ScreenId);
    return It != Screens.end() ? &It->second : nullptr;
}

const FRuntimeUIScreen* FRuntimeUISystem::FindScreen(const FString& ScreenId) const
{
    auto It = Screens.find(ScreenId);
    return It != Screens.end() ? &It->second : nullptr;
}

FRuntimeUIWidget* FRuntimeUISystem::FindWidget(const FString& WidgetId)
{
    auto It = Widgets.find(WidgetId);
    return It != Widgets.end() ? &It->second : nullptr;
}

const FRuntimeUIWidget* FRuntimeUISystem::FindWidget(const FString& WidgetId) const
{
    auto It = Widgets.find(WidgetId);
    return It != Widgets.end() ? &It->second : nullptr;
}

bool FRuntimeUISystem::SetText(const FString& WidgetId, const FString& Text)
{
    FRuntimeUIWidget* Widget = FindWidget(WidgetId);
    if (!Widget)
    {
        return false;
    }
    Widget->SetText(Text);
    return true;
}

bool FRuntimeUISystem::SetImage(const FString& WidgetId, const FString& ImagePath)
{
    FRuntimeUIWidget* Widget = FindWidget(WidgetId);
    if (!Widget)
    {
        return false;
    }
    Widget->SetImagePath(ImagePath);
    return true;
}

bool FRuntimeUISystem::SetProgress(const FString& WidgetId, float Value)
{
    FRuntimeUIWidget* Widget = FindWidget(WidgetId);
    if (!Widget)
    {
        return false;
    }
    Widget->SetProgress(Value);
    return true;
}

bool FRuntimeUISystem::SetVisible(const FString& WidgetId, bool bVisible)
{
    FRuntimeUIWidget* Widget = FindWidget(WidgetId);
    if (!Widget)
    {
        return false;
    }
    Widget->SetVisible(bVisible);
    return true;
}

bool FRuntimeUISystem::SetEnabled(const FString& WidgetId, bool bEnabled)
{
    FRuntimeUIWidget* Widget = FindWidget(WidgetId);
    if (!Widget)
    {
        return false;
    }
    Widget->SetEnabled(bEnabled);
    return true;
}

bool FRuntimeUISystem::SetActionEvent(const FString& WidgetId, const FString& EventName)
{
    FRuntimeUIWidget* Widget = FindWidget(WidgetId);
    if (!Widget)
    {
        return false;
    }
    Widget->SetActionEvent(EventName);
    return true;
}

bool FRuntimeUISystem::SetZOrder(const FString& WidgetId, int32 ZOrder)
{
    FRuntimeUIWidget* Widget = FindWidget(WidgetId);
    if (!Widget)
    {
        return false;
    }
    Widget->SetZOrder(ZOrder);
    return true;
}

bool FRuntimeUISystem::SetTint(const FString& WidgetId, const FRuntimeUIColor& Color)
{
    FRuntimeUIWidget* Widget = FindWidget(WidgetId);
    if (!Widget)
    {
        return false;
    }
    Widget->SetTint(Color);
    return true;
}

bool FRuntimeUISystem::SetBackgroundColor(const FString& WidgetId, const FRuntimeUIColor& Color)
{
    FRuntimeUIWidget* Widget = FindWidget(WidgetId);
    if (!Widget)
    {
        return false;
    }
    Widget->SetBackgroundColor(Color);
    return true;
}

bool FRuntimeUISystem::SetTextColor(const FString& WidgetId, const FRuntimeUIColor& Color)
{
    FRuntimeUIWidget* Widget = FindWidget(WidgetId);
    if (!Widget)
    {
        return false;
    }
    Widget->SetTextColor(Color);
    return true;
}

bool FRuntimeUISystem::SetAlpha(const FString& WidgetId, float Alpha)
{
    FRuntimeUIWidget* Widget = FindWidget(WidgetId);
    if (!Widget)
    {
        return false;
    }
    Widget->SetAlpha(Alpha);
    return true;
}

bool FRuntimeUISystem::SetRounding(const FString& WidgetId, float Rounding)
{
    FRuntimeUIWidget* Widget = FindWidget(WidgetId);
    if (!Widget)
    {
        return false;
    }
    Widget->SetRounding(std::max(0.0f, Rounding));
    return true;
}

bool FRuntimeUISystem::SetFontScale(const FString& WidgetId, float FontScale)
{
    FRuntimeUIWidget* Widget = FindWidget(WidgetId);
    if (!Widget)
    {
        return false;
    }
    Widget->SetFontScale(std::max(0.1f, FontScale));
    return true;
}

bool FRuntimeUISystem::AnimateTransform(
    const FString& WidgetId,
    const FRuntimeUIVector2& ToPosition,
    const FRuntimeUIVector2& ToSize,
    float Duration,
    bool bLoop,
    bool bPingPong,
    ERuntimeUIAnimationEasing Easing)
{
    FRuntimeUIWidget* Widget = FindWidget(WidgetId);
    if (!Widget || Duration <= 0.0f)
    {
        return false;
    }

    FRuntimeUIWidgetAnimation Animation;
    Animation.FromPosition = Widget->GetLocalPosition();
    Animation.ToPosition = ToPosition;
    Animation.FromSize = Widget->GetSize();
    Animation.ToSize = ToSize;
    Animation.Duration = Duration;
    Animation.bLoop = bLoop;
    Animation.bPingPong = bPingPong;
    Animation.Easing = Easing;
    Animations[WidgetId] = Animation;
    return true;
}

bool FRuntimeUISystem::AnimatePatrol(
    const FString& WidgetId,
    const FRuntimeUIVector2& FromPosition,
    const FRuntimeUIVector2& ToPosition,
    float Duration,
    bool bLoop,
    bool bPingPong,
    ERuntimeUIAnimationEasing Easing)
{
    FRuntimeUIWidget* Widget = FindWidget(WidgetId);
    if (!Widget || Duration <= 0.0f)
    {
        return false;
    }

    Widget->SetLocalPosition(FromPosition);

    FRuntimeUIWidgetAnimation Animation;
    Animation.FromPosition = FromPosition;
    Animation.ToPosition = ToPosition;
    Animation.FromSize = Widget->GetSize();
    Animation.ToSize = Widget->GetSize();
    Animation.Duration = Duration;
    Animation.bLoop = bLoop;
    Animation.bPingPong = bPingPong;
    Animation.Easing = Easing;
    Animations[WidgetId] = Animation;
    return true;
}

bool FRuntimeUISystem::StopAnimation(const FString& WidgetId)
{
    return Animations.erase(WidgetId) > 0;
}

void FRuntimeUISystem::UpdateLayout(const FRuntimeUIRenderContext& Context)
{
    TickAnimations(Context.DeltaTime);

    for (auto& CanvasPair : Canvases)
    {
        FRuntimeUICanvas& Canvas = CanvasPair.second;
        Canvas.UpdateFromRenderContext(Context);

        const FRuntimeUIScreen* Screen = FindScreen(Canvas.GetActiveScreenId());
        if (!Screen || Screen->GetCanvasId() != Canvas.GetId())
        {
            continue;
        }

        const bool bCanvasVisible = Canvas.IsVisible() && Screen->IsVisible();
        for (const FString& RootWidgetId : Screen->GetRootWidgetIds())
        {
            FRuntimeUIWidget* RootWidget = FindWidget(RootWidgetId);
            if (RootWidget)
            {
                ComputeWidgetTree(
                    *RootWidget,
                    Canvas.GetComputedRect(),
                    bCanvasVisible,
                    Canvas.IsEnabled(),
                    1.0f,
                    Canvas.GetComputedScale());
            }
        }
    }
}

void FRuntimeUISystem::Render(IRuntimeUIBackend& Backend, const FRuntimeUIRenderContext& Context)
{
    UpdateLayout(Context);
    Backend.BeginFrame(Context);

    for (const auto& CanvasPair : Canvases)
    {
        const FRuntimeUICanvas& Canvas = CanvasPair.second;
        const FRuntimeUIScreen* Screen = FindScreen(Canvas.GetActiveScreenId());
        if (!Screen || Screen->GetCanvasId() != Canvas.GetId() || !Canvas.IsVisible() || !Screen->IsVisible())
        {
            continue;
        }

        TArray<FString> RootWidgetIds = Screen->GetRootWidgetIds();
        std::sort(
            RootWidgetIds.begin(),
            RootWidgetIds.end(),
            [this](const FString& A, const FString& B)
            {
                const FRuntimeUIWidget* WidgetA = FindWidget(A);
                const FRuntimeUIWidget* WidgetB = FindWidget(B);
                const int32 ZOrderA = WidgetA ? WidgetA->GetZOrder() : 0;
                const int32 ZOrderB = WidgetB ? WidgetB->GetZOrder() : 0;
                return ZOrderA < ZOrderB;
            });

        for (const FString& RootWidgetId : RootWidgetIds)
        {
            const FRuntimeUIWidget* RootWidget = FindWidget(RootWidgetId);
            if (RootWidget)
            {
                RenderWidgetTree(Backend, *RootWidget);
            }
        }
    }

    Backend.EndFrame();
}

bool FRuntimeUISystem::HandleInput(const FRuntimeUIInputEvent& Event)
{
    FRuntimeUIWidget* HitWidget = HitTest(Event.ScreenPosition);

    switch (Event.Type)
    {
    case ERuntimeUIInputEventType::MouseMove:
        SetHoveredWidget(HitWidget ? HitWidget->GetId() : FString());
        return HitWidget != nullptr;

    case ERuntimeUIInputEventType::MouseButtonDown:
        if (Event.MouseButton == ERuntimeUIMouseButton::Left && HitWidget)
        {
            SetHoveredWidget(HitWidget->GetId());
            SetPressedWidget(HitWidget->GetId());
            return true;
        }
        return HitWidget != nullptr;

    case ERuntimeUIInputEventType::MouseButtonUp:
        if (Event.MouseButton == ERuntimeUIMouseButton::Left)
        {
            const FString PreviousPressedWidgetId = PressedWidgetId;
            SetPressedWidget(FString());
            SetHoveredWidget(HitWidget ? HitWidget->GetId() : FString());

            if (!PreviousPressedWidgetId.empty() && HitWidget && HitWidget->GetId() == PreviousPressedWidgetId)
            {
                PushActionEvent(HitWidget->GetActionEvent());
                return true;
            }
        }
        return HitWidget != nullptr || !PressedWidgetId.empty();

    case ERuntimeUIInputEventType::MouseWheel:
        return HitWidget != nullptr;

    case ERuntimeUIInputEventType::KeyDown:
    case ERuntimeUIInputEventType::KeyUp:
    case ERuntimeUIInputEventType::TextInput:
        return !PressedWidgetId.empty() || !HoveredWidgetId.empty();

    default:
        return false;
    }
}

void FRuntimeUISystem::PushActionEvent(const FString& EventName)
{
    if (!EventName.empty())
    {
        PendingActionEvents.push_back(EventName);
    }
}

TArray<FString> FRuntimeUISystem::ConsumeActionEvents()
{
    TArray<FString> Events = PendingActionEvents;
    PendingActionEvents.clear();
    return Events;
}

void FRuntimeUISystem::TickAnimations(float DeltaTime)
{
    if (Animations.empty() || DeltaTime <= 0.0f)
    {
        return;
    }

    TArray<FString> FinishedAnimations;
    for (auto& AnimationPair : Animations)
    {
        FRuntimeUIWidget* Widget = FindWidget(AnimationPair.first);
        if (!Widget)
        {
            FinishedAnimations.push_back(AnimationPair.first);
            continue;
        }

        FRuntimeUIWidgetAnimation& Animation = AnimationPair.second;
        Animation.Elapsed += DeltaTime;

        float Alpha = Animation.Duration > 0.0f ? Animation.Elapsed / Animation.Duration : 1.0f;
        if (Alpha >= 1.0f)
        {
            if (Animation.bLoop)
            {
                Animation.Elapsed = std::fmod(Animation.Elapsed, Animation.Duration);
                Alpha = Animation.Elapsed / Animation.Duration;
                if (Animation.bPingPong)
                {
                    std::swap(Animation.FromPosition, Animation.ToPosition);
                    std::swap(Animation.FromSize, Animation.ToSize);
                }
            }
            else
            {
                Alpha = 1.0f;
                FinishedAnimations.push_back(AnimationPair.first);
            }
        }

        const float EasedAlpha = ApplyAnimationEasing(Alpha, Animation.Easing);
        const FRuntimeUIVector2 NewPosition(
            Animation.FromPosition.X + (Animation.ToPosition.X - Animation.FromPosition.X) * EasedAlpha,
            Animation.FromPosition.Y + (Animation.ToPosition.Y - Animation.FromPosition.Y) * EasedAlpha);
        const FRuntimeUIVector2 NewSize(
            Animation.FromSize.X + (Animation.ToSize.X - Animation.FromSize.X) * EasedAlpha,
            Animation.FromSize.Y + (Animation.ToSize.Y - Animation.FromSize.Y) * EasedAlpha);

        Widget->SetLocalPosition(NewPosition);
        Widget->SetSize(NewSize);
    }

    for (const FString& WidgetId : FinishedAnimations)
    {
        Animations.erase(WidgetId);
    }
}

void FRuntimeUISystem::ComputeWidgetTree(
    FRuntimeUIWidget& Widget,
    const FRuntimeUIRect& ParentRect,
    bool bParentVisible,
    bool bParentEnabled,
    float ParentAlpha,
    float LayoutScale)
{
    Widget.ComputeLayout(ParentRect, bParentVisible, bParentEnabled, ParentAlpha, LayoutScale);

    TArray<FString> Children = Widget.GetChildren();
    std::sort(
        Children.begin(),
        Children.end(),
        [this](const FString& A, const FString& B)
        {
            const FRuntimeUIWidget* WidgetA = FindWidget(A);
            const FRuntimeUIWidget* WidgetB = FindWidget(B);
            const int32 ZOrderA = WidgetA ? WidgetA->GetZOrder() : 0;
            const int32 ZOrderB = WidgetB ? WidgetB->GetZOrder() : 0;
            return ZOrderA < ZOrderB;
        });

    for (const FString& ChildId : Children)
    {
        FRuntimeUIWidget* Child = FindWidget(ChildId);
        if (Child)
        {
            ComputeWidgetTree(
                *Child,
                Widget.GetComputedRect(),
                Widget.IsComputedVisible(),
                Widget.IsComputedEnabled(),
                Widget.GetComputedAlpha(),
                LayoutScale);
        }
    }
}

void FRuntimeUISystem::RenderWidgetTree(IRuntimeUIBackend& Backend, const FRuntimeUIWidget& Widget) const
{
    if (!Widget.IsComputedVisible())
    {
        return;
    }

    DrawWidget(Backend, Widget);

    TArray<FString> Children = Widget.GetChildren();
    std::sort(
        Children.begin(),
        Children.end(),
        [this](const FString& A, const FString& B)
        {
            const FRuntimeUIWidget* WidgetA = FindWidget(A);
            const FRuntimeUIWidget* WidgetB = FindWidget(B);
            const int32 ZOrderA = WidgetA ? WidgetA->GetZOrder() : 0;
            const int32 ZOrderB = WidgetB ? WidgetB->GetZOrder() : 0;
            return ZOrderA < ZOrderB;
        });

    for (const FString& ChildId : Children)
    {
        const FRuntimeUIWidget* Child = FindWidget(ChildId);
        if (Child)
        {
            RenderWidgetTree(Backend, *Child);
        }
    }
}

void FRuntimeUISystem::DrawWidget(IRuntimeUIBackend& Backend, const FRuntimeUIWidget& Widget) const
{
    switch (Widget.GetType())
    {
    case ERuntimeUIWidgetType::Panel:
        Backend.DrawPanel(Widget);
        break;
    case ERuntimeUIWidgetType::Text:
        Backend.DrawTextWidget(Widget);
        break;
    case ERuntimeUIWidgetType::Button:
        Backend.DrawButton(Widget);
        break;
    case ERuntimeUIWidgetType::Image:
        Backend.DrawImage(Widget);
        break;
    case ERuntimeUIWidgetType::ProgressBar:
        Backend.DrawProgressBar(Widget);
        break;
    default:
        break;
    }
}

FRuntimeUIWidget* FRuntimeUISystem::HitTest(const FRuntimeUIVector2& ScreenPosition)
{
    for (auto CanvasIt = Canvases.begin(); CanvasIt != Canvases.end(); ++CanvasIt)
    {
        FRuntimeUICanvas& Canvas = CanvasIt->second;
        FRuntimeUIScreen* Screen = FindScreen(Canvas.GetActiveScreenId());
        if (!Screen || Screen->GetCanvasId() != Canvas.GetId() || !Canvas.IsVisible() || !Screen->IsVisible())
        {
            continue;
        }

        TArray<FString> RootWidgetIds = Screen->GetRootWidgetIds();
        std::sort(
            RootWidgetIds.begin(),
            RootWidgetIds.end(),
            [this](const FString& A, const FString& B)
            {
                const FRuntimeUIWidget* WidgetA = FindWidget(A);
                const FRuntimeUIWidget* WidgetB = FindWidget(B);
                const int32 ZOrderA = WidgetA ? WidgetA->GetZOrder() : 0;
                const int32 ZOrderB = WidgetB ? WidgetB->GetZOrder() : 0;
                return ZOrderA > ZOrderB;
            });

        for (const FString& RootWidgetId : RootWidgetIds)
        {
            FRuntimeUIWidget* RootWidget = FindWidget(RootWidgetId);
            if (!RootWidget)
            {
                continue;
            }

            if (FRuntimeUIWidget* HitWidget = HitTestWidgetTree(*RootWidget, ScreenPosition))
            {
                return HitWidget;
            }
        }
    }

    return nullptr;
}

FRuntimeUIWidget* FRuntimeUISystem::HitTestWidgetTree(FRuntimeUIWidget& Widget, const FRuntimeUIVector2& ScreenPosition)
{
    if (!Widget.IsComputedVisible())
    {
        return nullptr;
    }

    TArray<FString> Children = Widget.GetChildren();
    std::sort(
        Children.begin(),
        Children.end(),
        [this](const FString& A, const FString& B)
        {
            const FRuntimeUIWidget* WidgetA = FindWidget(A);
            const FRuntimeUIWidget* WidgetB = FindWidget(B);
            const int32 ZOrderA = WidgetA ? WidgetA->GetZOrder() : 0;
            const int32 ZOrderB = WidgetB ? WidgetB->GetZOrder() : 0;
            return ZOrderA > ZOrderB;
        });

    for (const FString& ChildId : Children)
    {
        FRuntimeUIWidget* Child = FindWidget(ChildId);
        if (!Child)
        {
            continue;
        }

        if (FRuntimeUIWidget* HitChild = HitTestWidgetTree(*Child, ScreenPosition))
        {
            return HitChild;
        }
    }

    if (IsWidgetInputTarget(Widget) && Widget.GetComputedRect().Contains(ScreenPosition))
    {
        return &Widget;
    }

    return nullptr;
}

void FRuntimeUISystem::SetHoveredWidget(const FString& WidgetId)
{
    if (HoveredWidgetId == WidgetId)
    {
        return;
    }

    if (FRuntimeUIWidget* Previous = FindWidget(HoveredWidgetId))
    {
        Previous->SetHovered(false);
    }

    HoveredWidgetId = WidgetId;

    if (FRuntimeUIWidget* Current = FindWidget(HoveredWidgetId))
    {
        Current->SetHovered(true);
    }
}

void FRuntimeUISystem::SetPressedWidget(const FString& WidgetId)
{
    if (PressedWidgetId == WidgetId)
    {
        return;
    }

    if (FRuntimeUIWidget* Previous = FindWidget(PressedWidgetId))
    {
        Previous->SetPressed(false);
    }

    PressedWidgetId = WidgetId;

    if (FRuntimeUIWidget* Current = FindWidget(PressedWidgetId))
    {
        Current->SetPressed(true);
    }
}

bool FRuntimeUISystem::IsWidgetInputTarget(const FRuntimeUIWidget& Widget) const
{
    return Widget.IsComputedVisible() &&
        Widget.IsComputedEnabled() &&
        Widget.IsInteractable() &&
        (Widget.GetType() == ERuntimeUIWidgetType::Button || !Widget.GetActionEvent().empty());
}

void FRuntimeUISystem::DetachWidgetFromParentOrScreen(const FString& WidgetId)
{
    FRuntimeUIWidget* Widget = FindWidget(WidgetId);
    if (!Widget)
    {
        return;
    }

    if (!Widget->GetParentId().empty())
    {
        FRuntimeUIWidget* Parent = FindWidget(Widget->GetParentId());
        if (Parent)
        {
            Parent->RemoveChild(WidgetId);
        }
        return;
    }

    for (auto& ScreenPair : Screens)
    {
        ScreenPair.second.RemoveRootWidget(WidgetId);
    }
}

void FRuntimeUISystem::RemoveWidgetRecursive(const FString& WidgetId)
{
    FRuntimeUIWidget* Widget = FindWidget(WidgetId);
    if (!Widget)
    {
        return;
    }

    const TArray<FString> Children = Widget->GetChildren();
    for (const FString& ChildId : Children)
    {
        RemoveWidgetRecursive(ChildId);
    }

    Widgets.erase(WidgetId);
}
