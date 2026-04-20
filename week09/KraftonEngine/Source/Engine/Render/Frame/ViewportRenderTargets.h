#pragma once

#include <d3d11.h>

class FViewport;

/*
    FViewportRenderTargets - per-viewport render target bundle.
    Holds the active color/depth outputs and readable copies used by
    post-process / decal / outline style passes.
*/
struct FViewportRenderTargets
{
    ID3D11RenderTargetView* ViewportRTV = nullptr;
    ID3D11DepthStencilView* ViewportDSV = nullptr;

    // Final / PostProcess reads
    ID3D11ShaderResourceView* SceneColorCopySRV = nullptr;
    ID3D11Texture2D* SceneColorCopyTexture = nullptr;
    ID3D11Texture2D* ViewportRenderTexture = nullptr;

    // Depth copy
    ID3D11Texture2D* DepthTexture = nullptr;
    ID3D11Texture2D* DepthCopyTexture = nullptr;
    ID3D11ShaderResourceView* DepthCopySRV = nullptr;
    ID3D11ShaderResourceView* StencilCopySRV = nullptr;

    void Reset();
    void SetFromViewport(const FViewport* VP);

    bool HasViewportTargets() const
    {
        return ViewportRTV != nullptr || ViewportDSV != nullptr;
    }
};
