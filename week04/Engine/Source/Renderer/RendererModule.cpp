#include "Renderer/RendererModule.h"

#include "Renderer/Types/PickId.h"
#include "Renderer/Types/PickResult.h"

namespace
{
    bool HasScenePrimitives(const FSceneRenderData& InSceneRenderData)
    {
        return InSceneRenderData.SceneView != nullptr && !InSceneRenderData.Primitives.empty();
    }

    bool ShouldRenderSelectionOutline(const FEditorRenderData& InEditorRenderData,
                                      const FSceneRenderData&  InSceneRenderData)
    {
        return HasScenePrimitives(InSceneRenderData) && InEditorRenderData.bShowSelectionOutline &&
               InSceneRenderData.ViewMode != EViewModeIndex::VMI_Wireframe;
    }

    bool ShouldTintSelectedWireframe(const FEditorRenderData& InEditorRenderData,
                                     const FSceneRenderData&  InSceneRenderData)
    {
        return HasScenePrimitives(InSceneRenderData) && InEditorRenderData.bShowSelectionOutline &&
               InSceneRenderData.ViewMode == EViewModeIndex::VMI_Wireframe;
    }

    TArray<FPrimitiveRenderItem>
    BuildWireframePrimitiveSubmission(const FSceneRenderData& InSceneRenderData)
    {
        TArray<FPrimitiveRenderItem> SubmissionItems;
        SubmissionItems.reserve(InSceneRenderData.Primitives.size());

        for (const FPrimitiveRenderItem& Item : InSceneRenderData.Primitives)
        {
            SubmissionItems.push_back(Item);
        }

        const FColor SelectionColor = FD3D11OutlineRenderer::GetVisibleOutlineColor();

        for (FPrimitiveRenderItem& Item : SubmissionItems)
        {
            if (Item.State.IsSelected())
            {
                Item.Color = SelectionColor;
            }
        }

        return SubmissionItems;
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

    if (!OutlineRenderer.Initialize(&RHI))
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
    DebugDevice.Reset();

    ObjectIdRenderer.Shutdown();
    SpriteRenderer.Shutdown();
    TextRenderer.Shutdown();
    LineRenderer.Shutdown();
    OutlineRenderer.Shutdown();
    MeshBatchRenderer.Shutdown();

#if defined(_DEBUG)
    if (DebugDevice != nullptr)
    {
        DebugDevice->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);
        DebugDevice.Reset();
    }
#endif

    RHI.Shutdown();
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
}

/**
 * @brief Render Order: World Pass -> Overlay Pass
 */
void FRendererModule::Render(const FEditorRenderData& InEditorRenderData,
                             const FSceneRenderData&  InSceneRenderData)
{
    RenderWorldPass(InEditorRenderData, InSceneRenderData);
    RenderOverlayPass(InEditorRenderData, InSceneRenderData);
}

/**
 * World: Primitives -> Grid -> Axes
 */
void FRendererModule::RenderWorldPass(const FEditorRenderData& InEditorRenderData,
                                      const FSceneRenderData&  InSceneRenderData)
{
    if (HasScenePrimitives(InSceneRenderData))
    {
        FMeshBatchPassParams ScenePassParams = {};
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
    }

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

    if (ShouldRenderSelectionOutline(InEditorRenderData, InSceneRenderData))
    {
        OutlineRenderer.BeginFrame(InSceneRenderData.SceneView);
        OutlineRenderer.AddPrimitives(InSceneRenderData.Primitives);
        OutlineRenderer.EndFrame();
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
        FMeshBatchPassParams GizmoPassParams = {};
        GizmoPassParams.SceneView = InEditorRenderData.SceneView;
        GizmoPassParams.ViewMode = EViewModeIndex::VMI_Unlit;
        GizmoPassParams.bUseInstancing = true;
        GizmoPassParams.bDisableDepth = false;

        MeshBatchRenderer.BeginFrame(GizmoPassParams);
        OverlayMeshSubmitter.Submit(MeshBatchRenderer, InEditorRenderData);
        MeshBatchRenderer.EndFrame();

        // ================= Gizmo Center =================
        FMeshBatchPassParams GizmoCenterPassParams = GizmoPassParams;
        GizmoCenterPassParams.bDisableDepth = true;

        MeshBatchRenderer.BeginFrame(GizmoCenterPassParams);
        OverlayMeshSubmitter.SubmitCenterHandle(MeshBatchRenderer, InEditorRenderData);
        MeshBatchRenderer.EndFrame();
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
