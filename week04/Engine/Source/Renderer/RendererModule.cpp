#include "Renderer/RendererModule.h"
#include "Renderer/Types/PickId.h"
#include "Renderer/Types/PickResult.h"

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

    if (!MeshRenderer.Initialize(&RHI))
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
    MeshRenderer.Shutdown();

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

    static const FLOAT ClearColor[4] = {0.15f, 0.15f, 0.15f, 1.0f};

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

void FRendererModule::Render(const FEditorRenderData& InEditorRenderData,
                             const FSceneRenderData&  InSceneRenderData)
{
    CachedEditorRenderData = InEditorRenderData;
    CachedSceneRenderData = InSceneRenderData;

    MeshRenderer.BeginFrame(InSceneRenderData.SceneView, InSceneRenderData.ViewMode,
                            InSceneRenderData.bUseInstancing);

    if (IsFlagSet(InSceneRenderData.ShowFlags, ESceneShowFlags::SF_Primitives))
    {
        PrimitiveSubmitter.Submit(MeshRenderer, InSceneRenderData);
    }

    if (IsFlagSet(InEditorRenderData.ShowFlags, EEditorShowFlags::SF_Gizmo))
    {
        GizmoSubmitter.Submit(MeshRenderer, InEditorRenderData);
    }

    MeshRenderer.EndFrame();

    if (InSceneRenderData.SceneView != nullptr &&
        IsFlagSet(InSceneRenderData.ShowFlags, ESceneShowFlags::SF_Sprites))
    {
        SpriteRenderer.BeginFrame(InSceneRenderData.SceneView);
        SpriteSubmitter.Submit(SpriteRenderer, InSceneRenderData);
        SpriteRenderer.EndFrame(InSceneRenderData.SceneView);
    }

    if (InSceneRenderData.SceneView != nullptr &&
        IsFlagSet(InSceneRenderData.ShowFlags, ESceneShowFlags::SF_BillboardText))
    {
        TextRenderer.BeginFrame(InSceneRenderData.SceneView);
        TextSubmitter.Submit(TextRenderer, InSceneRenderData);
        TextRenderer.EndFrame(InSceneRenderData.SceneView);
    }

    if (InEditorRenderData.SceneView != nullptr)
    {
        LineRenderer.BeginFrame(InEditorRenderData.SceneView);

        if (IsFlagSet(InEditorRenderData.ShowFlags, EEditorShowFlags::SF_Grid))
        {
            WorldGridSubmitter.Submit(LineRenderer, InEditorRenderData);
        }

        if (IsFlagSet(InEditorRenderData.ShowFlags, EEditorShowFlags::SF_WorldAxes))
        {
            WorldAxesSubmitter.Submit(LineRenderer, InEditorRenderData);
        }

        AABBSubmitter.Submit(LineRenderer, InSceneRenderData);

        LineRenderer.EndFrame();
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

    if (IsFlagSet(InEditorRenderData.ShowFlags, EEditorShowFlags::SF_Gizmo))
    {
        TArray<FObjectIdRenderItem> GizmoItems;
        GizmoSubmitter.BuildObjectIdItems(GizmoItems, InEditorRenderData);
        ObjectIdRenderer.AddPrimitives(GizmoItems);
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
