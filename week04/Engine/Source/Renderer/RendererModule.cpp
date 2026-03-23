#include "Renderer/RendererModule.h"

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

    // PickingPass.Shutdown();
    // SpriteRenderer.Shutdown();
    // FontRenderer.Shutdown();
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
    // PickingPass.Resize(InWidth, InHeight);
}

void FRendererModule::Render(const FEditorRenderData& InEditorRenderData,
                             const FSceneRenderData&  InSceneRenderData)
{
    // ================ Mesh =================
    MeshRenderer.BeginFrame(InSceneRenderData.SceneView, InSceneRenderData.ViewMode,
                            InSceneRenderData.bUseInstancing);
    if (IsFlagSet(InSceneRenderData.ShowFlags, ESceneShowFlags::SF_Primitives))
    {
        PrimitiveDrawer.Draw(MeshRenderer, InSceneRenderData);
    }
    if (IsFlagSet(InEditorRenderData.ShowFlags, EEditorShowFlags::SF_Gizmo))
    {
        GizmoDrawer.Draw(MeshRenderer, InEditorRenderData);
    }
    MeshRenderer.EndFrame();

    // ================ Line =================
    if (InEditorRenderData.SceneView != nullptr)
    {
        LineRenderer.BeginFrame(InEditorRenderData.SceneView);

        if (IsFlagSet(InEditorRenderData.ShowFlags, EEditorShowFlags::SF_Grid))
        {
            WorldGridDrawer.Draw(LineRenderer, InEditorRenderData);
        }

        if (IsFlagSet(InEditorRenderData.ShowFlags, EEditorShowFlags::SF_WorldAxes))
        {
            WorldAxesDrawer.Draw(LineRenderer, InEditorRenderData);
        }

        LineRenderer.EndFrame();
    }

    // TODO:
    // FontRenderer.BeginFrame(...);
    // SpriteRenderer.BeginFrame(...);
    // Draw...
    // FontRenderer.EndFrame();
    // SpriteRenderer.EndFrame();
}

bool FRendererModule::TryConsumePickResult(uint32& OutPickId) { return false; }

void FRendererModule::RequestPick(const FEditorRenderData& InEditorRenderData, int32 MouseX,
                                  int32 MouseY)
{
    return;
}
