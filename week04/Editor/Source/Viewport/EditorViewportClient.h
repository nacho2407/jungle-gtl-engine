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

/**
 * @brief stat fps 명령이 활성화된 경우, viewport에 그려질 FPS 정보
 * 오버레이에 필요한 데이터를 담는 스냅샷 구조체
 */
struct FFPSStatData
{
    float FPS = 0.0f;
    float FrameTimeMS = 0.0f;
};

/**
 * @brief stat memory 명령이 활성화된 경우, viewport에 그려질 메모리
 * 정보 오버레이에 필요한 각 행의 데이터를 담는 구조체
 */
struct FMemoryStatRow
{
    FString Label;
    FString Value;
};

/**
 * @brief stat memory 명령이 활성화된 경우, viewport에 그려질 메모리
 * 정보 오버레이에 필요한 데이터를 담는 스냅샷 구조체
 */
struct FMemoryStatData
{
    TArray<FMemoryStatRow> Rows;
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

    /**
     * @brief 현재 viewport에서 그려지는 각 stat 명령 관련 텍스트 오버레이를 그리는 오케스트레이션 함수
     */
    void DrawStatOverlay(void);

    /**
     * @brief stat fps에 대응하는 FPS 정보 오버레이를 그립니다.
     * 
     * @param InData stat fps 명령이 활성화된 경우, viewport에 그려질 FPS 정보 오버레이에 필요한
     * 데이터를 담는 스냅샷 구조체
     */
    void DrawFPSStatOverlay(const FFPSStatData& InData);

    /**
     * @brief stat memory에 대응하는 메모리 정보 오버레이를 그립니다.
     * 
     * @param InData stat memory 명령이 활성화된 경우, viewport에 그려질 메모리 정보 오버레이에
     * 필요한 각 행의 데이터를 담는 구조체들의 배열을 포함하는 스냅샷 구조체
     */
    void DrawMemoryStatOverlay(const FMemoryStatData& InData);

    void EnableStat(EViewportStatFlags InFlag) { SetStatEnabled(InFlag, true); }

    void DisableStat(EViewportStatFlags InFlag) { SetStatEnabled(InFlag, false); }

    void ToggleStat(EViewportStatFlags InFlag) { SetStatEnabled(InFlag, !IsStatEnabled(InFlag)); }

    bool IsStatEnabled(EViewportStatFlags InFlags) const { return IsFlagSet(StatFlags, InFlags); }

    void ClearAllStats() { StatFlags = EViewportStatFlags::None; }

    EViewportStatFlags GetStatFlags() const { return StatFlags; }

    void SetViewportOrigin(uint32 InX, uint32 InY)
    {
        ViewportOriginX = InX;
        ViewportOriginY = InY;
    }

    uint32 GetViewportOriginX() const { return ViewportOriginX; }
    uint32 GetViewportOriginY() const { return ViewportOriginY; }
    
  private:
    void DrawOutline();

    void SetStatEnabled(EViewportStatFlags InFlag, bool bEnabled) { SetFlag(StatFlags, InFlag, bEnabled); }

    FFPSStatData    CollectFPSStatData() const;
    FMemoryStatData CollectMemoryStatData() const;

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

    /**
     * @brief 현재 viewport에서 활성화된 stat 명령들을 나타내는 플래그 집합
     */
    EViewportStatFlags StatFlags = EViewportStatFlags::None;

    // 다중 뷰포트 확장 대비
    uint32 ViewportOriginX = 0;
    uint32 ViewportOriginY = 0;

    FEditorContext* EditorContext = nullptr;
};
