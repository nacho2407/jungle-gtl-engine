// 뷰포트 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Render/RHI/D3D11/Common/D3D11API.h"

class FViewportClient;

// FViewport는 화면 출력용 렌더 타깃과 깊이 리소스를 관리합니다.
class FViewport
{
public:
    FViewport() = default;
    ~FViewport();

    // D3D 리소스 생성/해제/리사이즈
    bool Initialize(ID3D11Device* InDevice, uint32 InWidth, uint32 InHeight);
    void Release();
    void Resize(uint32 InWidth, uint32 InHeight);

    // 지연 리사이즈 — ImGui 렌더 중에 요청, RenderViewport 직전에 적용
    void RequestResize(uint32 InWidth, uint32 InHeight);
    bool ApplyPendingResize();

    // 오프스크린 RT 클리어 + 바인딩 (렌더 시작 시 호출)
    void BeginRender(ID3D11DeviceContext* Ctx, const float ClearColor[4] = nullptr);

    // ViewportClient 참조
    void SetClient(FViewportClient* InClient) { ViewportClient = InClient; }
    FViewportClient* GetClient() const { return ViewportClient; }

    // 크기
    uint32 GetWidth() const { return Width; }
    uint32 GetHeight() const { return Height; }

    // D3D 리소스 접근자
    ID3D11RenderTargetView* GetRTV() const { return RTV; }
    ID3D11ShaderResourceView* GetSRV() const { return SRV; }
    ID3D11Texture2D* GetRTTexture() const { return RTTexture; }
    ID3D11ShaderResourceView* GetSceneColorCopySRV() const { return SceneColorCopySRV; }
    ID3D11Texture2D* GetSceneColorCopyTexture() const { return SceneColorCopyTexture; }
    ID3D11DepthStencilView* GetDSV() const { return DSV; }
    ID3D11Texture2D* GetDepthTexture() const { return DepthTexture; }

    // CopyResource 대상 — 패스 간 안전하게 Depth/Stencil 읽기용
    ID3D11Texture2D* GetDepthCopyTexture() const { return DepthCopyTexture; }
    ID3D11ShaderResourceView* GetDepthCopySRV() const { return DepthCopySRV; }
    ID3D11ShaderResourceView* GetStencilCopySRV() const { return StencilCopySRV; }
    const D3D11_VIEWPORT& GetViewportRect() const { return ViewportRect; }

private:
    bool CreateResources();
    void ReleaseResources();

private:
    FViewportClient* ViewportClient = nullptr;

    ID3D11Device* Device = nullptr;

    // 렌더 타깃
    ID3D11Texture2D* RTTexture = nullptr;
    ID3D11RenderTargetView* RTV = nullptr;
    ID3D11ShaderResourceView* SRV = nullptr; // ImGui::Image() 출력용

    // 뎁스/스텐실
    ID3D11Texture2D* DepthTexture = nullptr;
    ID3D11DepthStencilView* DSV = nullptr;

    // CopyResource 대상 — DSV 전환 없이 안전하게 Depth/Stencil 읽기
    ID3D11Texture2D* DepthCopyTexture = nullptr;
    ID3D11ShaderResourceView* DepthCopySRV = nullptr;   // t10: SceneDepth
    ID3D11ShaderResourceView* StencilCopySRV = nullptr; // t13: (미래) Stencil

    // SceneColor 복사본 — FXAA 등 PostProcess에서 최종 화면을 읽기 위한 CopyResource 대상
    ID3D11Texture2D* SceneColorCopyTexture = nullptr;
    ID3D11ShaderResourceView* SceneColorCopySRV = nullptr;

    D3D11_VIEWPORT ViewportRect = {};

    uint32 Width = 0;
    uint32 Height = 0;

    // 지연 리사이즈 요청
    uint32 PendingWidth = 0;
    uint32 PendingHeight = 0;
    bool bPendingResize = false;
};
