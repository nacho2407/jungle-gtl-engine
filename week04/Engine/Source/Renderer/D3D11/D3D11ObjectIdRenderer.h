#pragma once

#include "Core/Containers/Array.h"
#include "Core/HAL/PlatformTypes.h"
#include "Core/Math/Matrix.h"
#include "Renderer/D3D11/D3D11Common.h"
#include "Renderer/Types/BasicMeshType.h"
#include "Resources/Mesh/MeshPrimitiveTopology.h"

class FD3D11RHI;
class FSceneView;

struct FVertexSimple;

struct FObjectIdRenderItem
{
    FMatrix        World = FMatrix::Identity;
    EBasicMeshType MeshType = EBasicMeshType::Cube;
    uint32         ObjectId = 0;
};

struct FObjectIdMeshResource
{
    TComPtr<ID3D11Buffer>  VertexBuffer = nullptr;
    TComPtr<ID3D11Buffer>  IndexBuffer = nullptr;
    uint32                 IndexCount = 0;
    EMeshPrimitiveTopology Topology = EMeshPrimitiveTopology::TriangleList;
};

class FD3D11ObjectIdRenderer
{
  public:
    static constexpr const wchar_t* ShaderPath = L"Content\\Shader\\ShaderObjectId.hlsl";

  public:
    bool Initialize(FD3D11RHI* InRHI);
    void Shutdown();

    bool Resize(int32 InWidth, int32 InHeight);

    void BeginFrame(const FSceneView* InSceneView, int32 InMouseX, int32 InMouseY);
    void AddPrimitive(const FObjectIdRenderItem& InItem);
    void AddPrimitives(const TArray<FObjectIdRenderItem>& InItems);
    bool RenderAndReadBack(uint32& OutPickId);

  private:
    bool CreateShaders();
    bool CreateConstantBuffer();
    bool CreateStates();

    bool CreatePickResources(int32 InWidth, int32 InHeight);
    void ReleasePickResources();

    bool CreateBasicMeshes();
    void ReleaseBasicMeshes();

    bool CreateBasicMeshResource(const FVertexSimple* InVertices, uint32 InVertexCount,
                                 const uint16* InIndices, uint32 InIndexCount,
                                 EMeshPrimitiveTopology InTopology,
                                 FObjectIdMeshResource& OutResource);

    bool CreateBasicCubeMesh(FObjectIdMeshResource& OutResource);
    bool CreateBasicQuadMesh(FObjectIdMeshResource& OutResource);
    bool CreateBasicTriangleMesh(FObjectIdMeshResource& OutResource);
    bool CreateBasicSphereMesh(FObjectIdMeshResource& OutResource);
    bool CreateBasicConeMesh(FObjectIdMeshResource& OutResource);
    bool CreateBasicCylinderMesh(FObjectIdMeshResource& OutResource);
    bool CreateBasicRingMesh(FObjectIdMeshResource& OutResource);

    FObjectIdMeshResource* GetBasicMeshResource(EBasicMeshType InType);

    void BindPipeline();
    void BindPrimitiveTopology(EMeshPrimitiveTopology InTopology);

    bool ReadBackMousePixel(uint32& OutObjectId);

  private:
    FD3D11RHI* RHI = nullptr;
    const FSceneView* CurrentSceneView = nullptr;

    int32 TargetWidth = 0;
    int32 TargetHeight = 0;

    int32 MouseX = -1;
    int32 MouseY = -1;


    TArray<FObjectIdRenderItem> RenderItems;

    TComPtr<ID3D11VertexShader> VertexShader;
    TComPtr<ID3D11PixelShader>  PixelShader;
    TComPtr<ID3D11InputLayout>  InputLayout;
    TComPtr<ID3D11Buffer>       ConstantBuffer;

    TComPtr<ID3D11RasterizerState>   RasterizerState;
    TComPtr<ID3D11DepthStencilState> DepthStencilState;

    TComPtr<ID3D11Texture2D>        PickColorTexture;
    TComPtr<ID3D11RenderTargetView> PickRTV;

    TComPtr<ID3D11Texture2D>        PickDepthTexture;
    TComPtr<ID3D11DepthStencilView> PickDSV;

    TComPtr<ID3D11Texture2D> ReadbackTexture; // 1x1 staging

    FObjectIdMeshResource MeshResources[static_cast<int32>(EBasicMeshType::Count)];
};