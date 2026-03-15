#pragma once

#include <d3d11.h>
#include <d3dcompiler.h>
#include <string>
#include <unordered_map>

#include "CoreTypes.h"

#include "World/FScene.h"
#include "Math/FMatrix.h"
#include "Structs.h"
#include "Resource/MeshTypes.h"

class UStaticMesh;
class UCameraComponent;

class URenderer
{
public:
	URenderer();
	~URenderer();

public:
	bool Create(HWND hWindow);
	void Release();

	void BeginFrame();
	void Render(const FScene& Scene, const UCameraComponent* Camera);
	void EndFrame();

	void Resize(UINT width, UINT Height);

    // 마우스 위치에 따른 Hit Proxy 쿼리
    FHitProxy PickPrimitiveProxy(int MouseX, int MouseY);

public:
	ID3D11Device* Device = nullptr;
	ID3D11DeviceContext* DeviceContext = nullptr;
	IDXGISwapChain* SwapChain = nullptr;

	ID3D11Texture2D* FrameBuffer = nullptr;
	ID3D11RenderTargetView* FrameBufferRTV = nullptr;

	ID3D11Texture2D* DepthStencilBuffer = nullptr;
	ID3D11DepthStencilView* DepthStencilView = nullptr;

	ID3D11RasterizerState* RasterizerState = nullptr;

	ID3D11VertexShader* SimpleVertexShader = nullptr;
	ID3D11PixelShader* SimplePixelShader = nullptr;
	ID3D11InputLayout* SimpleInputLayout = nullptr;
	ID3D11Buffer* ConstantBuffer = nullptr;

	FLOAT ClearColor[4] = { 0.025f, 0.025f, 0.025f, 1.0f };
	D3D11_VIEWPORT ViewportInfo = {};

	UINT Stride = sizeof(FVertexSimple);

private:
    struct FVSConstants
    {
        FMatrix World;
        FMatrix View;
        FMatrix Projection;
        FVector4 Color;
    };

    struct FMeshGPUResource
    {
        ID3D11Buffer* VertexBuffer = nullptr;
        ID3D11Buffer* IndexBuffer = nullptr;
        UINT VertexCount = 0;
        UINT IndexCount = 0;
        D3D11_PRIMITIVE_TOPOLOGY Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    };

private:
    bool CreateDeviceAndSwapChain(HWND hWindow);
    bool CreateFrameBuffer();
    bool CreateDepthStencilBuffer(UINT Width, UINT Height);
    bool CreateRasterizerState();
    bool CreateShader(const wchar_t* ShaderPath);
    bool CreateConstantBuffer();

    void ReleaseFrameBuffer();
    void ReleaseDepthStencilBuffer();
    void ReleaseRasterizerState();
    void ReleaseShader();
    void ReleaseConstantBuffer();
    void ReleaseDeviceAndSwapChain();
    void ReleaseMeshResources();

    void PreparePipeline();
    void UpdateVSConstants(const FMatrix& World, const FMatrix& View, const FMatrix& Projection, const FVector4& Color);

    bool GetOrCreateMeshResource(UStaticMesh* Mesh, FMeshGPUResource& OutResource);
    D3D11_PRIMITIVE_TOPOLOGY ConvertTopology(EMeshTopology Topology) const;
    void DrawMeshItem(const FRenderItem& Item, const FMatrix& View, const FMatrix& Projection);

    // Hit Proxy를 위한 버퍼 생성 / 해제
    bool CreateHitProxyBuffer(UINT Width, UINT Height);
    void ReleaseHitProxyBuffer();

    // Hit Proxy를 위한 셰이더 생성 / 해제
    bool CreateHitProxyShader(const wchar_t* ShaderPath);
    void ReleaseHitProxyShader();

    // Hit Proxy를 위한 별도의 파이프라인 준비
    void PrepareHitProxyPipeline();

    // Hit Proxy Pass 렌더
    void RenderHitProxy(const FScene& Scene, const UCameraComponent* Camera);
    void DrawHitProxyItem(const FRenderItem& Item, const FMatrix& View, const FMatrix& Projection, uint32 ProxyId);

    // ID <> 픽셀 색상 변환
    static FVector4 EncodeHitProxyIdColor(uint32 Id);
    static uint32 DecodeHitProxyIdColor(uint8 R, uint8 G, uint8 B);

private:

    std::unordered_map<UStaticMesh*, FMeshGPUResource> MeshResourceMap;

    // Hit Proxy Pass 결과를 그리는 실제 텍스처
    ID3D11Texture2D* HitProxyTexture = nullptr;

    // HitProxyTexture를 렌더 타깃으로 쓰기 위한 뷰
    ID3D11RenderTargetView* HitProxyRTV = nullptr;

    // GPU 결과를 CPU가 읽기 위해 복사해 두는 스테이징 텍스처(Picking을 위해 선택된 곳의 색을 읽음)
    ID3D11Texture2D* HitProxyReadbackTexture = nullptr; 

    /* 일반 렌더링과 입력 레이아웃이나 정점 셰이더는 같은 것을 쓰지만, 픽셀 셰이더는 
       ID 별로 색을 칠해줘야하기 때문에 일반 렌더링용 픽셀 셰이더와 별도로 제작 */
    ID3D11PixelShader* HitProxyPixelShader = nullptr;

    /* Hit Proxy Pass에서 각 RenderItem에 ID를 부여하고, Picking 결과
       픽셀을 읽은 뒤 그 ID로 다시 RenderItem을 찾아내기 위한 맵 */
    TMap<uint32, FHitProxy> HitProxyMap;
};
