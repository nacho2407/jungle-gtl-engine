#include "Renderer/RendererModule.h"

#include <utility>

#include "Renderer/Types/PickId.h"
#include "Renderer/Types/PickResult.h"
#include "SceneView.h"
#include "Core/Runtime/Slate/Window/SWindow.h"
#include <d3d11sdklayers.h>

namespace
{
    bool ShouldRenderSelectionOutline(const FEditorRenderData& InEditorRenderData,
                                      const FSceneRenderData&  InSceneRenderData)
    {
        return InSceneRenderData.SceneView != nullptr && InEditorRenderData.bShowSelectionOutline &&
               InSceneRenderData.ViewMode != EViewModeIndex::VMI_Wireframe;
    }
} // namespace

bool FRendererModule::StartupModule(HWND hWnd)
{
    if (hWnd == nullptr)
    {
        return false;
    }

    if (!RHI.Initialize(hWnd))
    {
        ShutdownModule();
        return false;
    }

    if (!MeshBatchRenderer.Initialize(&RHI))
    {
        ShutdownModule();
        return false;
    }

    if (!SelectionMaskRenderer.Initialize(&RHI))
    {
        ShutdownModule();
        return false;
    }

    if (!PostProcessOutlineRenderer.Initialize(&RHI))
    {
        ShutdownModule();
        return false;
    }

    if (!LineRenderer.Initialize(&RHI))
    {
        ShutdownModule();
        return false;
    }

    if (!TextRenderer.Initialize(&RHI))
    {
        ShutdownModule();
        return false;
    }

    if (!SpriteRenderer.Initialize(&RHI))
    {
        ShutdownModule();
        return false;
    }

    if (!ObjectIdRenderer.Initialize(&RHI))
    {
        ShutdownModule();
        return false;
    }

    if (!WidgetRenderer.Initialize(&RHI)) {
        ShutdownModule();
        return false;
    }

    if (!StaticMeshRenderer.Initialize(&RHI))
    {
        ShutdownModule();
        return false;
    }

#if defined(_DEBUG)
    if (RHI.GetDevice() != nullptr)
    {
        RHI.GetDevice()->QueryInterface(__uuidof(ID3D11Debug),
                                        reinterpret_cast<void**>(DebugDevice.GetAddressOf()));
    }
#endif

    return true;
}

void FRendererModule::ShutdownModule()
{
    ObjectIdRenderer.Shutdown();
    SpriteRenderer.Shutdown();
    TextRenderer.Shutdown();
    LineRenderer.Shutdown();
    PostProcessOutlineRenderer.Shutdown();
    SelectionMaskRenderer.Shutdown();
    MeshBatchRenderer.Shutdown();
    StaticMeshRenderer.Shutdown();
    WidgetRenderer.Shutdown();

    ReportLiveObjects();

    RHI.Shutdown();
}

void FRendererModule::ReportLiveObjects()
{
#if defined(_DEBUG)
    if (DebugDevice != nullptr)
    {
        DebugDevice->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);
        DebugDevice.Reset();
    }
#endif
}

void FRendererModule::BeginFrame()
{
    RHI.BeginFrame();

    static const FLOAT ClearColor[4] = {0.17f, 0.17f, 0.17f, 1.0f};

    RHI.SetDefaultRenderTargets();
    RHI.Clear(ClearColor, 1.0f, 0);
}

void FRendererModule::EndFrame() { RHI.EndFrame(); }

void FRendererModule::OnWindowResized(int32 InWidth, int32 InHeight)
{
    if (InWidth <= 0 || InHeight <= 0)
    {
        return;
    }

    RHI.Resize(InWidth, InHeight);
    ObjectIdRenderer.Resize(InWidth, InHeight);
    SelectionMaskRenderer.Resize(InWidth, InHeight);
}

void FRendererModule::SetSceneFrameData(FSceneFrameRenderData&& InFrameData)
{
    CachedSceneData.StaticMeshes   = std::move(InFrameData.StaticMeshes);
    CachedSceneData.Primitives     = std::move(InFrameData.Primitives);
    CachedSceneData.Sprites        = std::move(InFrameData.Sprites);
    CachedSceneData.Texts          = std::move(InFrameData.Texts);
    CachedSceneData.bUseInstancing = InFrameData.bUseInstancing;
    CachedSceneData.SceneView      = nullptr; // stamped per-panel in Render()
}

/**
 * @brief Render Order: World Pass -> Overlay Pass
 */
void FRendererModule::Render(const FEditorRenderData& InEditorRenderData, EViewModeIndex ViewMode)
{
    CachedSceneData.SceneView = InEditorRenderData.SceneView;
    CachedSceneData.ViewMode  = ViewMode;
    RenderWorldPass(InEditorRenderData, CachedSceneData);
    RenderOverlayPass(InEditorRenderData, CachedSceneData);
}

/**
 * World: Primitives -> Grid -> Axes
 */
void FRendererModule::RenderWorldPass(const FEditorRenderData& InEditorRenderData,
                                      const FSceneRenderData&  InSceneRenderData)
{
    const FViewportRect& ViewRect = InSceneRenderData.SceneView->GetViewRect();
    D3D11_VIEWPORT       VP = {(float)ViewRect.X,
                               (float)ViewRect.Y,
                               (float)ViewRect.Width,
                               (float)ViewRect.Height,
                               0.f,
                               1.f
    };
    RHI.SetViewport(VP);

   /* if (HasScenePrimitives(InSceneRenderData))
    {
        FMeshPassParams ScenePassParams = {};
        ScenePassParams.SceneView = InSceneRenderData.SceneView;
        ScenePassParams.ViewMode = InSceneRenderData.ViewMode;
        ScenePassParams.bUseInstancing = InSceneRenderData.bUseInstancing;
        ScenePassParams.bDisableDepth = false;

        MeshBatchRenderer.BeginFrame(ScenePassParams);

        if (ShouldTintSelectedWireframe(InEditorRenderData, InSceneRenderData))
        {
            const TArray<FPrimitiveRenderItem> WireframeItems =
                BuildWireframePrimitiveSubmission(InSceneRenderData);
            MeshBatchRenderer.AddPrimitives(WireframeItems);
        }
        else
        {
            SceneMeshSubmitter.Submit(MeshBatchRenderer, InSceneRenderData);
        }

        MeshBatchRenderer.EndFrame();
    }*/

    // -------------------------------------------------------------------------
    // Static Mesh 렌더링
    // -------------------------------------------------------------------------
    if (InSceneRenderData.SceneView != nullptr && !InSceneRenderData.StaticMeshes.empty())
    {
        FMeshPassParams StaticMeshPassParams = {};
        StaticMeshPassParams.SceneView = InSceneRenderData.SceneView;
        StaticMeshPassParams.ViewMode = InSceneRenderData.ViewMode;
        // 스태틱 메시는 현재 단일 드로우 콜 구조이므로 인스턴싱 플래그는 false 또는 무시됩니다.
        StaticMeshPassParams.bUseInstancing = false;
        StaticMeshPassParams.bDisableDepth = false;
        StaticMeshPassParams.Time = Time; // 시간 전달

        #if IS_OBJ_VIEWER
        StaticMeshPassParams.CullMode = InEditorRenderData.CullMode;
        #endif

        StaticMeshRenderer.BeginFrame(StaticMeshPassParams);

        // Submitter를 통해 RenderData의 StaticMeshes를 Renderer로 전달
        StaticMeshSubmitter.Submit(StaticMeshRenderer, InSceneRenderData);

        StaticMeshRenderer.EndFrame();
    }
    // -------------------------------------------------------------------------

    if (InEditorRenderData.SceneView != nullptr)
    {
        LineRenderer.BeginFrame(InEditorRenderData.SceneView);

        if (InEditorRenderData.bShowGrid)
        {
            WorldGridSubmitter.Submit(LineRenderer, InEditorRenderData);
        }

        if (InEditorRenderData.bShowWorldAxes)
        {
            WorldAxesSubmitter.Submit(LineRenderer, InEditorRenderData);
        }

        AABBSubmitter.Submit(LineRenderer, InSceneRenderData);
        LineRenderer.EndFrame();
    }

    if (InSceneRenderData.SceneView != nullptr && !InSceneRenderData.Sprites.empty())
    {
        SpriteRenderer.BeginFrame(InSceneRenderData.SceneView);
        SpriteSubmitter.Submit(SpriteRenderer, InSceneRenderData);
        SpriteRenderer.EndFrame(InSceneRenderData.SceneView);
    }

    if (InSceneRenderData.SceneView != nullptr && !InSceneRenderData.Texts.empty())
    {
        if (InSceneRenderData.ViewMode == EViewModeIndex::VMI_Wireframe)
        {
            LineRenderer.BeginFrame(InSceneRenderData.SceneView);
            TextSubmitter.Submit(LineRenderer, InSceneRenderData);
            LineRenderer.EndFrame();
        }
        else
        {
            TextRenderer.BeginFrame(InSceneRenderData.SceneView);
            TextSubmitter.Submit(TextRenderer, InSceneRenderData);
            TextRenderer.EndFrame(InSceneRenderData.SceneView);
        }
    }

    if (ShouldRenderSelectionOutline(InEditorRenderData, InSceneRenderData))
    {
        SelectionMaskRenderer.BeginFrame(InSceneRenderData.SceneView,
                                         InEditorRenderData.bEnableOutlineOcclusion);
        SelectionMaskRenderer.AddStaticMeshes(InSceneRenderData.StaticMeshes);
        SelectionMaskRenderer.AddSprites(InSceneRenderData.Sprites);
        SelectionMaskRenderer.AddTexts(InSceneRenderData.Texts);
        SelectionMaskRenderer.EndFrame();

        PostProcessOutlineRenderer.BeginFrame(InSceneRenderData.SceneView,
                                              SelectionMaskRenderer.GetMaskSRV());
        PostProcessOutlineRenderer.EndFrame();
    }
}

/**
 * Overlay: Gizmo -> Text
 */
void FRendererModule::RenderOverlayPass(const FEditorRenderData& InEditorRenderData,
                                        const FSceneRenderData&  InSceneRenderData)
{
    const bool bHasEditorGizmo =
        InEditorRenderData.SceneView != nullptr && InEditorRenderData.bShowGizmo &&
        InEditorRenderData.Gizmo.GizmoType != EGizmoType::None;

    if (bHasEditorGizmo)
    {
        RHI.ClearDepthStencil(RHI.GetDepthStencilView(), 1.0f, 0);

        // ================= Gizmo =================
        FMeshPassParams GizmoPassParams = {};
        GizmoPassParams.SceneView = InEditorRenderData.SceneView;
        GizmoPassParams.ViewMode = EViewModeIndex::VMI_Unlit;
        GizmoPassParams.bUseInstancing = true;
        GizmoPassParams.bDisableDepth = false;

        MeshBatchRenderer.BeginFrame(GizmoPassParams);
        OverlayMeshSubmitter.Submit(MeshBatchRenderer, InEditorRenderData);
        MeshBatchRenderer.EndFrame();

        // ================= Gizmo Center =================
        FMeshPassParams GizmoCenterPassParams = GizmoPassParams;
        GizmoCenterPassParams.bDisableDepth = true;

        MeshBatchRenderer.BeginFrame(GizmoCenterPassParams);
        OverlayMeshSubmitter.SubmitCenterHandle(MeshBatchRenderer, InEditorRenderData);
        MeshBatchRenderer.EndFrame();
    }
}

void FRendererModule::RenderViewportOverlayPass(const FWidgetRenderData& InWidgetRenderData) 
{
    if (InWidgetRenderData.Widgets.empty())
        return;

    WidgetRenderer.BeginFrame(InWidgetRenderData.ScreenWidth, InWidgetRenderData.ScreenHeight);

    ID3D11DeviceContext* Context = RHI.GetDeviceContext();

    for (SWidget* Widget : InWidgetRenderData.Widgets)
    {
        if (!Widget)
            continue;

        // Only render SWindow widgets for now, as they are the only ones used in viewport overlays.
        // Implement a separate submitter if more variety is need in the future
        SWindow* Window = dynamic_cast<SWindow*>(Widget);
        if (!Window)
            continue;

        WidgetRenderer.DrawWidget(Context, Window->PosX, Window->PosY, Window->Width,
                                  Window->Height, FColor(0.25f, 0.25f, 0.25f, 1.f));
    }
}

bool FRendererModule::PickRaw(const FEditorRenderData& InEditorRenderData, int32 MouseX,
                              int32 MouseY, uint32& OutPickId)
{
    OutPickId = PickId::None;

    const FSceneView* SceneView = InEditorRenderData.SceneView;
    if (SceneView == nullptr)
    {
        return false;
    }

    ObjectIdRenderer.BeginFrame(SceneView, MouseX, MouseY);

    if (InEditorRenderData.bShowGizmo && InEditorRenderData.Gizmo.GizmoType != EGizmoType::None)
    {
        OverlayMeshSubmitter.Submit(ObjectIdRenderer, InEditorRenderData);
    }

    return ObjectIdRenderer.RenderAndReadBack(OutPickId);
}

bool FRendererModule::Pick(const FEditorRenderData& InEditorRenderData, int32 MouseX, int32 MouseY,
                           FPickResult& OutResult)
{
    OutResult = {};

    uint32 PickedId = PickId::None;
    if (!PickRaw(InEditorRenderData, MouseX, MouseY, PickedId))
    {
        return false;
    }

    OutResult = PickResult::FromPickId(PickedId);
    return true;
}

void FRendererModule::SetVSyncEnabled(bool bEnabled) { RHI.SetVSyncEnabled(bEnabled); }

bool FRendererModule::IsVSyncEnabled() const { return RHI.IsVSyncEnabled(); }
