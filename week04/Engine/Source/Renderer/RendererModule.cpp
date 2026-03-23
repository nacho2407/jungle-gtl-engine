#include "Renderer/RendererModule.h"

bool FRendererModule::StartupModule(HWND hWnd)
{
    if (!RHI.Initialize(hWnd))
    {
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
    // D3D 리소스들을 먼저 해제
    LineRenderer.Shutdown();
    MeshRenderer.Shutdown();

    // TODO: 나중에 사용 시작하면 활성화
    // FontRenderer.Shutdown();
    // SpriteRenderer.Shutdown();
    // PickingPass.Shutdown();

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
}

void FRendererModule::Render(const FEditorRenderData& InEditorRenderData,
                                  const FSceneRenderData&  InSceneRenderData)
{
    // Scene
    MeshRenderer.Render(InSceneRenderData);

    //// Editor Overlay (Grid -> World Axes -> Gizmo)
    // WorldGridDrawer.Draw(LineRenderer, InEditorRenderData);
    // WorldAxesDrawer.Draw(LineRenderer, InEditorRenderData);
    // GizmoDrawer.Draw(MeshRenderer, InEditorRenderData);
}

bool FRendererModule::TryConsumePickResult(uint32& OutPickId)
{
    (void)OutPickId;

    // TODO: PickingPass 연결 후 구현
    return false;
}

void FRendererModule::RequestPick(const FEditorRenderData& InEditorRenderData, int32 MouseX,
                                  int32 MouseY)
{
    (void)InEditorRenderData;
    (void)MouseX;
    (void)MouseY;

    // TODO: PickingPass 연결 후 구현
}