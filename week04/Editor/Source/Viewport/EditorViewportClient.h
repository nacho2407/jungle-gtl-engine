#pragma once

#include "Core/Misc/BitMaskEnum.h"
#include "Navigation/ViewportNavigationController.h"
#include "Selection/ViewportSelectionController.h"
#include "Gizmo/ViewportGizmoController.h"
#include "Interaction/ViewportInteractionState.h"
#include "RenderSetting/ViewportRenderSetting.h"
#include "Engine/ViewPort/ViewportClient.h"
#include "Input/NavigationInputContext.h"
#include "Input/SelectionInputContext.h"
#include "Input/GizmoInputContext.h"
#include "Renderer/EditorRenderData.h"
#include "Renderer/Types/EditorShowFlags.h"

struct FEditorContext;

enum class EViewportStatFlags : uint8
{
    None = 0,
    FPS = 1 << 0,
    Memory = 1 << 1
};

template <> struct TEnableBitMaskOperators<EViewportStatFlags>
{
    static constexpr bool bEnabled = true;
};

class FEditorViewportClient : public Engine::Viewport::IViewportClient
{
  public:
    FEditorViewportClient() = default;
    ~FEditorViewportClient() override = default;

    void Create() override;
    void Release() override;

    void Initialize(FScene* Scene, uint32 ViewportWidth, uint32 ViewportHeight) override;

    void Tick(float DeltaTime, const Engine::ApplicationCore::FInputState& State) override;
    void Draw() override;

    void HandleInputEvent(const Engine::ApplicationCore::FInputEvent& Event,
                          const Engine::ApplicationCore::FInputState& State) override;

    void BuildRenderData(FEditorRenderData& OutRenderData, EEditorShowFlags InShowFlags);

    void OnResize(uint32 InWidth, uint32 InHeight);
    void SetEditorContext(FEditorContext* InContext);
    void SetScene(FScene* InScene);
    void SyncSelectionFromContext();

    FViewportNavigationController& GetNavigationController() { return NavigationController; }
    const FViewportNavigationController& GetNavigationController() const
    {
        return NavigationController;
    }
    FViewportSelectionController& GetSelectionController() { return SelectionController; }
    FViewportGizmoController& GetGizmoController() { return GizmoController; }
    FViewportInteractionState& GetInteractionState() { return InteractionState; }
    FViewportRenderSetting& GetRenderSetting() { return RenderSetting; }
    const FViewportRenderSetting& GetRenderSetting() const { return RenderSetting; }

    void DrawViewportOverlay();

    FViewportCamera& GetCamera() { return ViewportCamera; }
    using FPickCallback = std::function<FPickResult(int32, int32)>;
    FPickCallback OnPickRequested;
    
    FPickResult PickAt(int32 MouseX, int32 MouseY) const
    {
        if (OnPickRequested)
        {
            return OnPickRequested(MouseX, MouseY);
        }
        return FPickResult{};
    }

    bool IsStatEnabled(EViewportStatFlags InFlags) const { return IsFlagSet(StatFlags, InFlags); }

    void SetStatEnabled(EViewportStatFlags InFlag, bool bEnabled)
    {
        SetFlag(StatFlags, InFlag, bEnabled);
    }

    void ClearAllStats() { StatFlags = EViewportStatFlags::None; }

    EViewportStatFlags GetStatFlags() const { return StatFlags; }
    
private:
    void DrawOutline();

  private:
    FScene* CurScene = nullptr;

    FViewportCamera ViewportCamera;

    FViewportNavigationController NavigationController;
    FViewportSelectionController SelectionController;
    FViewportGizmoController GizmoController;
    FViewportInteractionState InteractionState;
    FViewportRenderSetting RenderSetting;

    FNavigationInputContext ViewportInputContext{&NavigationController};
    FSelectionInputContext SelectionInputContext{&SelectionController};
    FGizmoInputContext      GizmoInputContext{&GizmoController};

    EViewportStatFlags StatFlags = EViewportStatFlags::None;
};
