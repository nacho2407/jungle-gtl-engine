#pragma once

#include "Core/Containers/Array.h"
#include "Core/HAL/PlatformTypes.h"
#include "Renderer/D3D11/D3D11Common.h"
#include "Renderer/Types/RenderItem.h" // FStaticMeshRenderItem 등
#include "Renderer/Types/ViewMode.h"
#include "Renderer/Types/MeshPassParams.h"

class FD3D11RHI;
class FSceneView;

// MeshBatchRenderer와 동일한 패스 파라미터를 사용합니다.
struct FMeshPassParams;
struct FMaterialData;

class FD3D11StaticMeshRenderer
{
  public:
    // 스태틱 메시 전용 셰이더 경로 (프로젝트 구조에 맞게 수정 필요)
    static constexpr const wchar_t* ShaderPath = L"Content\\Shader\\StaticMeshShader.hlsl";

  public:
    bool Initialize(FD3D11RHI* InRHI);
    void Shutdown();

    void BeginFrame(const FMeshPassParams& InPassParams);
    void AddStaticMesh(const FStaticMeshRenderItem& InItem);
    void AddStaticMeshes(const TArray<FStaticMeshRenderItem>& InItems);
    void EndFrame();
    void Flush();

  private:
    bool CreateShaders();
    bool CreateConstantBuffers();
    bool CreateStates();

    bool CreateDefaultResources();

    void ResetBatches();

    void BindPipeline();
    void BindSolidRasterizer();
    void BindWireframeRasterizer();

    void BindMaterial(const FMaterialData* InMaterialData);
    void BindDefaultMaterial();

  private:
    FD3D11RHI*           RHI = nullptr;
    FMeshPassParams CurrentPassParams = {};

    TArray<FStaticMeshRenderItem> StaticMeshDraws;

    TComPtr<ID3D11VertexShader> VertexShader;
    TComPtr<ID3D11PixelShader>  PixelShader;
    TComPtr<ID3D11InputLayout>  InputLayout;
    TComPtr<ID3D11Buffer>       ConstantBuffer;

    TComPtr<ID3D11RasterizerState>   SolidRasterizerState;        // CULL_Back
#if IS_OBJ_VIEWER
    TComPtr<ID3D11RasterizerState>   SolidNoneRasterizerState;    // CULL_None
    TComPtr<ID3D11RasterizerState>   SolidFrontRasterizerState;   // CULL_Front
#endif 

    TComPtr<ID3D11RasterizerState>   WireframeRasterizerState;
    TComPtr<ID3D11DepthStencilState> DepthStencilState;
    TComPtr<ID3D11DepthStencilState> DepthDisabledState;

    TComPtr<ID3D11SamplerState>       LinearSamplerState;
    TComPtr<ID3D11Texture2D>          DefaultWhiteTexture;
    TComPtr<ID3D11ShaderResourceView> DefaultWhiteSRV;
};