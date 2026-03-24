#pragma once

#include "Core/Containers/Array.h"
#include "Core/HAL/PlatformTypes.h"
#include "Core/Math/Color.h"
#include "Core/Math/Matrix.h"
#include "Renderer/D3D11/D3D11Common.h"
#include "Renderer/SceneRenderData.h"
#include "Renderer/Types/BasicMeshType.h"
#include "Renderer/Types/ViewMode.h"
#include "Resources/Mesh/MeshPrimitiveTopology.h"

class FD3D11RHI;
class FSceneView;

struct FVertexSimple;

enum class EMeshDrawPath : uint8
{
    Single,
    Instanced
};

enum class EMeshDrawClass : uint8
{
    Scene = 0,
    Editor = 1,
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
        L"Content\\Shader\\ShaderInstancedMesh.hlsl";
    static constexpr const wchar_t* SingleShaderPath = L"Content\\Shader\\ShaderMesh.hlsl";
    static constexpr uint32         MaxInstanceCapacity = 4096;

  public:
    bool Initialize(FD3D11RHI* InRHI);
    void Shutdown();

    void BeginFrame(const FSceneView* InSceneView, EViewModeIndex InViewMode,
                    bool bInUseInstancing);
    void AddPrimitive(const FPrimitiveRenderItem& InItem,
                      EMeshDrawClass              InDrawClass = EMeshDrawClass::Scene);
    void AddPrimitives(const TArray<FPrimitiveRenderItem>& InItems,
                       EMeshDrawClass                      InDrawClass = EMeshDrawClass::Scene);
    void EndFrame();
    void Flush();

  private:
    bool CreateShaders();
    bool CreateConstantBuffers();
    bool CreateStates();
    bool CreateDynamicInstanceBuffer(uint32 InMaxInstanceCount);

    bool CreateBasicMeshes();
    bool CreateBasicCubeMesh(FBasicMeshResource& OutResource);
    bool CreateBasicQuadMesh(FBasicMeshResource& OutResource);
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

    void UpdatePerFrameConstants(const FSceneView* InSceneView, EMeshDrawPath DrawPath);

    void BindPipeline(EMeshDrawPath DrawPath);
    void BindPrimitiveTopology(EMeshPrimitiveTopology InTopology);
    void BindSolidRasterizer();
    void BindWireframeRasterizer();

    void PrepareFlush(EMeshDrawPath DrawPath, const FSceneView* InSceneView);
    void FlushSceneQueue(EMeshDrawPath DrawPath, const FSceneView* InSceneView);
    void FlushEditorQueue(EMeshDrawPath DrawPath, const FSceneView* InSceneView);
    void FlushCombinedSolidQueue(EMeshDrawPath DrawPath, const FSceneView* InSceneView);

    void DrawMeshBatch(EBasicMeshType InType, EMeshDrawPath DrawPath, const FSceneView* InSceneView,
                       TArray<FMeshDrawData>& Draws);

    FBasicMeshResource*       GetBasicMeshResource(EBasicMeshType InType);
    const FBasicMeshResource* GetBasicMeshResource(EBasicMeshType InType) const;

  private:
    FD3D11RHI*        RHI = nullptr;
    const FSceneView* CurrentSceneView = nullptr;
    EViewModeIndex    ViewMode = EViewModeIndex::VMI_Lit;
    bool              bUseInstancing = true;
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
    TArray<FMeshDrawData> SceneMeshDraws[static_cast<int32>(EBasicMeshType::Count)];
    TArray<FMeshDrawData> EditorMeshDraws[static_cast<int32>(EBasicMeshType::Count)];
};