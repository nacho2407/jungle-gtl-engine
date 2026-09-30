#pragma once

#include <d3d11.h>

class FViewport;

/*
    실제 뷰포트 렌더링에 쓰는 RTV/DSV와 패스 간 복사용 SRV를 묶어 둔 구조체입니다.
    SceneDepth, Fog, FXAA, Outline 같은 후처리 패스가 안전하게 이전 결과를 읽을 수 있게 합니다.
*/
struct FViewportRenderTargets
{
    const FViewport*        SourceViewport = nullptr;
    ID3D11RenderTargetView* ViewportRTV    = nullptr;
    ID3D11DepthStencilView* ViewportDSV    = nullptr;

    // 최종 색상 복사본. Fog / FXAA / Outline 입력으로 사용합니다.
    ID3D11ShaderResourceView* SceneColorCopySRV     = nullptr;
    ID3D11Texture2D*          SceneColorCopyTexture = nullptr;
    ID3D11Texture2D*          ViewportRenderTexture = nullptr;

    // 깊이 복사본. Decal / SceneDepth / Fog / Outline 입력으로 사용합니다.
    ID3D11Texture2D*          DepthTexture     = nullptr;
    ID3D11Texture2D*          DepthCopyTexture = nullptr;
    ID3D11ShaderResourceView* DepthCopySRV     = nullptr;
    ID3D11ShaderResourceView* StencilCopySRV   = nullptr;

    void Reset();
    void SetFromViewport(const FViewport* VP);

    bool HasViewportTargets() const
    {
        return ViewportRTV != nullptr || ViewportDSV != nullptr;
    }
};
