#include "Renderer/RendererModule.h"

void FRendererModule::StartupModule(HWND hWnd)
{
    if (!RHI.Initialize(hWnd))
    {
        return;
    }

    if (!MeshRenderer.Initialize(&RHI))
    {
        ShutdownModule();
        return;
    }

    if (!LineRenderer.Initialize(&RHI))
    {
        ShutdownModule();
        return;
    }

    if (!GizmoRenderer.Initialize(&RHI))
    {
        ShutdownModule();
        return;
    }

#if defined(_DEBUG)
    if (RHI.GetDevice() != nullptr)
    {
        RHI.GetDevice()->QueryInterface(__uuidof(ID3D11Debug),
                                        reinterpret_cast<void**>(DebugDevice.GetAddressOf()));
    }
#endif
}

void FRendererModule::ShutdownModule()
{
    // D3D 리소스들을 먼저 해제
    LineRenderer.Shutdown();
    MeshRenderer.Shutdown();
    GizmoRenderer.Shutdown();

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

    static const FLOAT ClearColor[4] = {0.10f, 0.10f, 0.12f, 1.0f};

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

void FRendererModule::RenderFrame(const FEditorRenderData& InEditorRenderData,
                                  const FSceneRenderData&  InSceneRenderData)
{
    (void)InEditorRenderData;

    BeginFrame();

    // Scene
    MeshRenderer.Render(InSceneRenderData);

    // Editor Overlay (Grid -> World Axes -> Gizmo)
    // TODO: 그리드, 월드 축은 나중에 추가
    GizmoRenderer.Render(InEditorRenderData);

    EndFrame();
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