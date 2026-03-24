#pragma once

#include "Core/Containers/Array.h"
#include "Core/HAL/PlatformTypes.h"
#include "Core/Math/Color.h"
#include "Core/Math/Matrix.h"
#include "Renderer/D3D11/D3D11Common.h"
#include "Renderer/Types/BasicMeshType.h"
#include "Renderer/Types/RenderItem.h"
#include "Resources/Mesh/MeshPrimitiveTopology.h"

class FD3D11RHI;
class FSceneView;
struct FVertexSimple;

struct FOutlineRenderItem
{
    FMatrix        World = FMatrix::Identity;
    EBasicMeshType MeshType = EBasicMeshType::Cube;
};

struct FOutlineMeshResource
{
    TComPtr<ID3D11Buffer>  VertexBuffer = nullptr;
    TComPtr<ID3D11Buffer>  IndexBuffer = nullptr;
    uint32                 IndexCount = 0;
    EMeshPrimitiveTopology Topology = EMeshPrimitiveTopology::TriangleList;
};

class FD3D11OutlineRenderer
{
  public:
    static constexpr const wchar_t* ShaderPath = L"Content\\Shader\\ShaderMesh.hlsl";
    static constexpr float DefaultOutlineScale = 1.04f;

  public:
    bool Initialize(FD3D11RHI* InRHI);
    void Shutdown();

    void BeginFrame(const FSceneView* InSceneView);
    void AddPrimitive(const FPrimitiveRenderItem& InItem);
    void AddPrimitives(const TArray<FPrimitiveRenderItem>& InItems);
    void EndFrame();

    static FColor GetVisibleOutlineColor();
    static FColor GetOccludedOutlineColor();

  private:
    bool CreateShaders();
    bool CreateConstantBuffer();
    bool CreateStates();

    bool CreateBasicMeshes();
    void ReleaseBasicMeshes();

    bool CreateBasicMeshResource(const FVertexSimple* InVertices, uint32 InVertexCount,
                                 const uint16* InIndices, uint32 InIndexCount,
                                 EMeshPrimitiveTopology InTopology,
                                 FOutlineMeshResource&  OutResource);

    bool CreateBasicCubeMesh(FOutlineMeshResource& OutResource);
    bool CreateBasicQuadMesh(FOutlineMeshResource& OutResource);
    bool CreateBasicTriangleMesh(FOutlineMeshResource& OutResource);
    bool CreateBasicSphereMesh(FOutlineMeshResource& OutResource);
    bool CreateBasicConeMesh(FOutlineMeshResource& OutResource);
    bool CreateBasicCylinderMesh(FOutlineMeshResource& OutResource);
    bool CreateBasicRingMesh(FOutlineMeshResource& OutResource);

    FOutlineMeshResource* GetBasicMeshResource(EBasicMeshType InType);

    void BindPipeline(ID3D11RasterizerState* InRasterizerState,
                      ID3D11DepthStencilState* InDepthStencilState);
    void BindPrimitiveTopology(EMeshPrimitiveTopology InTopology);
    void DrawItems(const FColor& InColor, float InScale);

  private:
    FD3D11RHI*        RHI = nullptr;
    const FSceneView* CurrentSceneView = nullptr;

    TArray<FOutlineRenderItem> RenderItems;

    TComPtr<ID3D11VertexShader> VertexShader;
    TComPtr<ID3D11PixelShader>  PixelShader;
    TComPtr<ID3D11InputLayout>  InputLayout;
    TComPtr<ID3D11Buffer>       ConstantBuffer;

    TComPtr<ID3D11BlendState>        StencilMaskBlendState;
    TComPtr<ID3D11RasterizerState>   StencilMaskRasterizerState;
    TComPtr<ID3D11RasterizerState>   OutlineRasterizerState;
    TComPtr<ID3D11DepthStencilState> StencilMarkDepthStencilState;
    TComPtr<ID3D11DepthStencilState> VisibleDepthStencilState;
    TComPtr<ID3D11DepthStencilState> OccludedDepthStencilState;

    FOutlineMeshResource MeshResources[static_cast<int32>(EBasicMeshType::Count)];
};
