#pragma once

#include "Core/Containers/Array.h"
#include "Core/HAL/PlatformTypes.h"
#include "Core/Math/Matrix.h"
#include "Core/Math/Vector.h"
#include "Core/Math/Color.h"
#include "Renderer/D3D11/D3D11Common.h"
#include "Renderer/SceneRenderData.h"
#include "Renderer/Types/BasicMeshType.h"
#include "Renderer/Types/ViewMode.h"

class FD3D11DynamicRHI;
class FSceneView;

struct FVertexSimple;

// Per-instance
struct FMeshInstanceData
{
    FMatrix  World;
    FColor Color = FColor::White();
};

struct FBasicMeshResource
{
    TComPtr<ID3D11Buffer> VertexBuffer = nullptr;
    TComPtr<ID3D11Buffer> IndexBuffer = nullptr;
    uint32                IndexCount = 0;
};

// TODO: Instance 유무에 따라 분기

class FD3D11MeshBatchRenderer
{
  public:
    static constexpr const wchar_t* DefaultShaderPath =
        L"Resources/Shaders/ShaderInstancedMesh.hlsl";
    static constexpr uint32 DefaultMaxInstances = 4096;

  public:
    bool Initialize(FD3D11DynamicRHI* InRHI);
    void Shutdown();

    void Render(const FSceneRenderData& InRenderData);

  private:
    bool CreateShaders();
    bool CreateConstantBuffers();
    bool CreateStates();
    bool CreateDynamicInstanceBuffer(uint32 InMaxInstanceCount);

    bool CreateBasicMeshes();
    void ReleaseBasicMeshes();

    bool CreateBasicCubeMesh(FBasicMeshResource& OutResource);
    bool CreateBasicPlaneMesh(FBasicMeshResource& OutResource);
    bool CreateBasicTriangleMesh(FBasicMeshResource& OutResource);
    bool CreateBasicSphereMesh(FBasicMeshResource& OutResource);

    bool CreateBasicMeshResource(const FVertexSimple* InVertices, uint32 InVertexCount,
                                 const uint16* InIndices, uint32 InIndexCount,
                                 FBasicMeshResource& OutResource);

    void ResetBatches();
    void GatherRenderItems(const FSceneRenderData& InRenderData);

    void Flush(const FSceneView* InSceneView);
    void UpdatePerFrameConstants(const FSceneView* InSceneView);

    void BindPipeline();
    void BindSolidRasterizer();
    void BindWireframeRasterizer();

    void DrawMeshBatch(EBasicMeshType InType, const FSceneView* InSceneView);

    FBasicMeshResource*       GetMeshResource(EBasicMeshType InType);
    const FBasicMeshResource* GetMeshResource(EBasicMeshType InType) const;

  private:
    FD3D11DynamicRHI* RHI = nullptr;
    EViewModeIndex    ViewMode = EViewModeIndex::Lit;
    uint32            MaxInstanceCount = DefaultMaxInstances;
    bool              bUseInstancing = true; // TODO

    TComPtr<ID3D11VertexShader> VertexShader;
    TComPtr<ID3D11PixelShader>  PixelShader;
    TComPtr<ID3D11InputLayout>  InputLayout;
    TComPtr<ID3D11Buffer>       ConstantBuffer;
    TComPtr<ID3D11Buffer>       InstanceBuffer;

    TComPtr<ID3D11RasterizerState>   SolidRasterizerState;
    TComPtr<ID3D11RasterizerState>   WireframeRasterizerState;
    TComPtr<ID3D11DepthStencilState> DepthStencilState;

    FBasicMeshResource        BasicMeshes[static_cast<int32>(EBasicMeshType::Count)];
    TArray<FMeshInstanceData> MeshInstances[static_cast<int32>(EBasicMeshType::Count)];
};