// 렌더 영역의 세부 동작을 구현합니다.
#include "Render/Execute/Context/Viewport/ViewportRenderTargets.h"

#include "Viewport/Viewport.h"

void FViewportRenderTargets::Reset()
{
    SourceViewport        = nullptr;
    ViewportRTV           = nullptr;
    ViewportDSV           = nullptr;
    SceneColorCopySRV     = nullptr;
    SceneColorCopyTexture = nullptr;
    ViewportRenderTexture = nullptr;
    DepthTexture          = nullptr;
    DepthCopyTexture      = nullptr;
    DepthCopySRV          = nullptr;
    StencilCopySRV        = nullptr;
}

void FViewportRenderTargets::SetFromViewport(const FViewport* VP)
{
    if (!VP)
    {
        Reset();
        return;
    }

    SourceViewport        = VP;
    ViewportRTV           = VP->GetRTV();
    ViewportDSV           = VP->GetDSV();
    SceneColorCopySRV     = VP->GetSceneColorCopySRV();
    SceneColorCopyTexture = VP->GetSceneColorCopyTexture();
    ViewportRenderTexture = VP->GetRTTexture();
    DepthTexture          = VP->GetDepthTexture();
    DepthCopyTexture      = VP->GetDepthCopyTexture();
    DepthCopySRV          = VP->GetDepthCopySRV();
    StencilCopySRV        = VP->GetStencilCopySRV();
}
