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
#include "Resources/Mesh/MeshPrimitiveTopology.h"

class FD3D11DynamicRHI;
class FSceneView;

struct FVertexSimple;

enum class EMeshDrawPath : uint8
{
    Single,
    Instanced
};

struct FMeshDrawData
{
    FMatrix World;
    FColor  Color = FColor::White();
};

struct FBasicMeshResource
{
    TComPtr<ID3D11Buffer>  VertexBuffer = nullptr;
    TComPtr<ID3D11Buffer>  IndexBuffer = nullptr;
    uint32                 IndexCount = 0;
    EMeshPrimitiveTopology Topology = EMeshPrimitiveTopology::TriangleList;
};

class FD3D11MeshBatchRenderer
{
  public:
    static constexpr const wchar_t* InstancedShaderPath =
        L"../Engine/Resources/Shader/ShaderInstancedMesh.hlsl";
    static constexpr const wchar_t* SingleShaderPath =
        L"../Engine/Resources/Shader/ShaderMesh.hlsl";
    static constexpr uint32         MaxInstanceCapacity = 4096;

  public:
    bool Initialize(FD3D11DynamicRHI* InRHI);
    void Shutdown();

    void Render(const FSceneRenderData& InRenderData);
    void Flush(EMeshDrawPath DrawPath, const FSceneView* InSceneView);

  private:
    bool CreateShaders();
    bool CreateConstantBuffers();
    bool CreateStates();
    bool CreateDynamicInstanceBuffer(uint32 InMaxInstanceCount);

    // Basic Meshes (Sphere, Cube, Plane, Triangle, Cone, Cylinder, Ring)
    bool CreateBasicMeshes();
    bool CreateBasicCubeMesh(FBasicMeshResource& OutResource);
    bool CreateBasicPlaneMesh(FBasicMeshResource& OutResource);
    bool CreateBasicTriangleMesh(FBasicMeshResource& OutResource);
    bool CreateBasicSphereMesh(FBasicMeshResource& OutResource);
    bool CreateBasicConeMesh(FBasicMeshResource& OutResource);
    bool CreateBasicCylinderMesh(FBasicMeshResource& OutResource);
    bool CreateBasicRingMesh(FBasicMeshResource& OutResource);
    void ReleaseBasicMeshes();

    bool CreateBasicMeshResource(const FVertexSimple* InVertices, uint32 InVertexCount,
                                 const uint16* InIndices, uint32 InIndexCount,
                                 EMeshPrimitiveTopology InTopology,
                                 FBasicMeshResource&    OutResource);

    void ResetBatches();
    void GatherRenderItems(const FSceneRenderData& InRenderData);

    void UpdatePerFrameConstants(const FSceneView* InSceneView, EMeshDrawPath DrawPath);

    void BindPipeline(EMeshDrawPath DrawPath);
    void BindPrimitiveTopology(EMeshPrimitiveTopology InTopology);
    void BindSolidRasterizer();
    void BindWireframeRasterizer();

    void DrawMeshBatch(EBasicMeshType InType, EMeshDrawPath DrawPath,
                       const FSceneView* InSceneView);

    FBasicMeshResource*       GetBasicMeshResource(EBasicMeshType InType);
    const FBasicMeshResource* GetBasicMeshResource(EBasicMeshType InType) const;

  private:
    FD3D11DynamicRHI* RHI = nullptr;
    EViewModeIndex    ViewMode = EViewModeIndex::Lit;
    uint32            MaxInstanceCount = MaxInstanceCapacity;

    TComPtr<ID3D11VertexShader> InstancedVertexShader;
    TComPtr<ID3D11PixelShader>  InstancedPixelShader;
    TComPtr<ID3D11InputLayout>  InstancedInputLayout;
    TComPtr<ID3D11Buffer>       InstancedConstantBuffer;
    TComPtr<ID3D11Buffer>       InstanceBuffer;

    TComPtr<ID3D11VertexShader> SingleVertexShader;
    TComPtr<ID3D11PixelShader>  SinglePixelShader;
    TComPtr<ID3D11InputLayout>  SingleInputLayout;
    TComPtr<ID3D11Buffer>       SingleConstantBuffer;

    TComPtr<ID3D11RasterizerState>   SolidRasterizerState;
    TComPtr<ID3D11RasterizerState>   WireframeRasterizerState;
    TComPtr<ID3D11DepthStencilState> DepthStencilState;

    FBasicMeshResource    BasicMeshResources[static_cast<int32>(EBasicMeshType::Count)];
    TArray<FMeshDrawData> MeshDraws[static_cast<int32>(EBasicMeshType::Count)];
};