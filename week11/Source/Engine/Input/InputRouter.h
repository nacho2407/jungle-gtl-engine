#pragma once

#include <functional>
#include <windows.h>

#include "Engine/Input/InputTypes.h"
#include "Slate/SlateUtils.h"

struct FInputSystemSnapshot;

class FInputRouter
{
public:
    using FRectProvider = std::function<bool(FRect&)>;
    using FWorldResolver = std::function<UWorld*()>;
	using FZOrderProvider = std::function<int32()>;

    void SetOwnerWindow(HWND InOwnerWindow) { OwnerWindow = InOwnerWindow; }
    void SetImGuiCaptureState(bool bCaptureMouse, bool bCaptureKeyboard);
    void SetForceViewportMouseBlock(bool bEnable, bool bAllowViewportFocusPress = false);
    void ClearViewportFocus();
    void ForceViewportFocus(FViewport* InViewport);

    void ClearTargets();
    void RegisterTarget(
        FViewport* InViewport,
        FViewportClient* InClient,
        EInteractionDomain InDomain,
        FRectProvider InRectProvider,
        FWorldResolver InWorldResolver,
		FZOrderProvider InZOrderProvider = nullptr);

    FViewportClient* GetFocusedClient() const;

    bool Tick(float DeltaTime, FViewportInputContext& OutContext, FInteractionBinding& OutBinding);

private:
    struct FTargetEntry
    {
        FViewport*         Viewport = nullptr;
        FViewportClient*   Client = nullptr;
        EInteractionDomain Domain = EInteractionDomain::Editor;
        FRectProvider      RectProvider;
        FWorldResolver     WorldResolver;
        FZOrderProvider ZOrderProvider;
    };

    static bool IsPointInRect(const POINT& Point, const FRect& Rect);
    void ResetTrackingState();
    RECT GetTargetRectScreenRect(const FRect& TargetRect) const;
    FTargetEntry* FindEntryByViewport(FViewport* InViewport, FRect& OutRect);
    POINT GetTargetRectScreenCenter(const FRect& TargetRect) const;
    void PopulateDispatchContext(
        const FInputSystemSnapshot& InputSnapshot,
        float DeltaTime,
        const POINT& MouseClientPos,
        FTargetEntry* TargetEntry,
        const FRect& TargetRect,
        FViewportInputContext& OutContext,
        FInteractionBinding& OutBinding);
    void ClearRoutingStateAndMouseControl();
    bool EnsureRoutingEnvironmentReady();
    void DeactivateAllMouseControl();
    void ValidateTrackedViewports();
    void UpdatePointerTrackingState(FTargetEntry* HoveredEntry, bool& bAnyPointerPressed, bool& bAnyPointerDown, bool bHardBlockMouse);
    void FinalizeAndDispatchInput(
        FTargetEntry* TargetEntry,
        const FRect& TargetRect,
        const FInputSystemSnapshot& InputSnapshot,
        const POINT& MouseScreenPos,
        bool bHardBlockMouse,
        FViewportInputContext& InOutContext);
    bool TryFindHoveredTarget(const POINT& MouseClientPos, FTargetEntry*& OutHoveredEntry, FRect& OutHoveredRect);
    FTargetEntry* ResolveDispatchTarget(FTargetEntry* HoveredEntry, const FRect& HoveredRect, bool bAnyPointerDown, FRect& OutTargetRect);
    void UpdateRelativeMouseModeState(
        FTargetEntry* TargetEntry,
        const FRect& TargetRect,
        const FInputSystemSnapshot& InputSnapshot,
        const POINT& MouseScreenPos,
        FViewportInputContext& InOutContext);
    void UpdateAbsoluteClipState(FTargetEntry* TargetEntry, const FViewportInputContext& Context);
    void ActivateRelativeMouseMode(FViewport* InViewport, const POINT& RestoreScreenPos, const RECT& ClipScreenRect);
    void DeactivateRelativeMouseMode();
    void ActivateAbsoluteMouseClip(FViewport* InViewport, const RECT& ClipScreenRect);
    void DeactivateAbsoluteMouseClip();

private:
    TArray<FTargetEntry> Targets;
    HWND OwnerWindow = nullptr;

    FViewport* HoveredViewport = nullptr;
    FViewport* FocusedViewport = nullptr;
    FViewport* CapturedViewport = nullptr;

    bool   bImGuiCaptureMouse = false;
    bool   bImGuiCaptureKeyboard = false;
    bool   bForceViewportMouseBlock = false;
    bool   bAllowViewportFocusPress = false;
    bool   bBlockViewportMouseUntilRelease = false;
    uint64 InputFrameCounter = 0;
    bool   bRelativeMouseModeActive = false;
    FViewport* RelativeMouseModeViewport = nullptr;
    POINT  RelativeMouseRestorePos = { 0, 0 };
    bool   bAbsoluteMouseClipActive = false;
    FViewport* AbsoluteMouseClipViewport = nullptr;
    RECT   AbsoluteMouseClipRect = { 0, 0, 0, 0 };
};

class FInputPolicyRouter
{
public:
    using FRectProvider = FInputRouter::FRectProvider;
    using FWorldResolver = FInputRouter::FWorldResolver;
	using FZOrderProvider = FInputRouter::FZOrderProvider;

    void SetOwnerWindow(HWND InOwnerWindow) { Router.SetOwnerWindow(InOwnerWindow); }
    void SetImGuiCaptureState(bool bCaptureMouse, bool bCaptureKeyboard) { Router.SetImGuiCaptureState(bCaptureMouse, bCaptureKeyboard); }
    void SetForceViewportMouseBlock(bool bEnable, bool bAllowViewportFocusPress = false)
    {
        Router.SetForceViewportMouseBlock(bEnable, bAllowViewportFocusPress);
    }
    void ClearViewportFocus() { Router.ClearViewportFocus(); }
    void ForceViewportFocus(FViewport* InViewport) { Router.ForceViewportFocus(InViewport); }

    void ClearTargets() { Router.ClearTargets(); }
    void RegisterTarget(
        FViewport* InViewport,
        FViewportClient* InClient,
        EInteractionDomain InDomain,
        FRectProvider InRectProvider,
        FWorldResolver InWorldResolver,
        FZOrderProvider InZOrderProvider = nullptr)
    {
        Router.RegisterTarget(InViewport, InClient, InDomain, InRectProvider, InWorldResolver, InZOrderProvider);
    }

    bool Tick(float DeltaTime, FViewportInputContext& OutContext, FInteractionBinding& OutBinding);

    FViewportClient* GetFocusedClient() const { return Router.GetFocusedClient(); }

    const FInputFrameDispatch& GetLastDispatch() const { return LastDispatch; }
    const FInputSideEffectPermissions& GetSideEffectPermissions() const { return LastDispatch.SideEffects; }

private:
    static EInputDomain ConvertInteractionDomain(EInteractionDomain Domain);
    void ResetLastDispatch();
    void MirrorCurrentRouterDispatch(const FViewportInputContext& Context);

private:
    FInputRouter Router;
    FInputFrameDispatch LastDispatch;
};
