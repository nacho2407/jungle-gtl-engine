#include "Render/Pipelines/Context/View/ViewportRenderTargets.h"

#include "Viewport/Viewport.h"

void FViewportRenderTargets::Reset()
{
    ViewportRTV = nullptr;
    ViewportDSV = nullptr;
    SceneColorCopySRV = nullptr;
    SceneColorCopyTexture = nullptr;
    ViewportRenderTexture = nullptr;
    DepthTexture = nullptr;
    DepthCopyTexture = nullptr;
    DepthCopySRV = nullptr;
    StencilCopySRV = nullptr;
}

void FViewportRenderTargets::SetFromViewport(const FViewport* VP)
{
    if (!VP)
    {
        Reset();
        return;
    }

    ViewportRTV = VP->GetRTV();
    ViewportDSV = VP->GetDSV();
    SceneColorCopySRV = VP->GetSceneColorCopySRV();
    SceneColorCopyTexture = VP->GetSceneColorCopyTexture();
    ViewportRenderTexture = VP->GetRTTexture();
    DepthTexture = VP->GetDepthTexture();
    DepthCopyTexture = VP->GetDepthCopyTexture();
    DepthCopySRV = VP->GetDepthCopySRV();
    StencilCopySRV = VP->GetStencilCopySRV();
}
