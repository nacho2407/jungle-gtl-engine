#pragma once

#include "Core/HAL/PlatformTypes.h"
#include "Renderer/D3D11/D3D11Common.h"
#include "Renderer/EditorRenderData.h"

class FD3D11DynamicRHI;
class FSceneView;

struct FVertexSimple;

struct FGizmoMeshResource
{
    TComPtr<ID3D11Buffer> VertexBuffer = nullptr;
    TComPtr<ID3D11Buffer> IndexBuffer = nullptr;
    uint32                IndexCount = 0;
};

class FD3D11GizmoRenderer
{
  public:
    static constexpr const wchar_t* DefaultShaderPath = L"Resources/Shaders/ShaderMesh.hlsl";

  public:
    bool Initialize(FD3D11DynamicRHI* InRHI);
    void Shutdown();

    void Render(const FEditorRenderData& InEditorRenderData);

  private:
    bool CreateShaders();
    bool CreateConstantBuffer();
    bool CreateStates();

    bool CreateGizmoMeshes();
    void ReleaseGizmoMeshes();

    bool CreateTranslationMesh(FGizmoMeshResource& OutResource);
    bool CreateRotationMesh(FGizmoMeshResource& OutResource);
    bool CreateScalingMesh(FGizmoMeshResource& OutResource);

    bool CreateMeshResource(const FVertexSimple* InVertices, uint32 InVertexCount,
                            const uint16* InIndices, uint32 InIndexCount,
                            FGizmoMeshResource& OutResource);

    void BindPipeline();

    void DrawTranslation(const FGizmoDrawData& InGizmo, const FSceneView* InSceneView);
    void DrawRotation(const FGizmoDrawData& InGizmo, const FSceneView* InSceneView);
    void DrawScaling(const FGizmoDrawData& InGizmo, const FSceneView* InSceneView);

    void DrawAxisMesh(const FGizmoMeshResource& InMesh, const FMatrix& InWorld,
                      const FSceneView* InSceneView, const FVector4& InColor);

    FVector4 GetAxisColor(EAxis InAxis, EGizmoHighlight InHighlight) const;

    FMatrix MakeAxisRotationX() const;
    FMatrix MakeAxisRotationY() const;
    FMatrix MakeAxisRotationZ() const;

  private:
    FD3D11DynamicRHI* RHI = nullptr;

    TComPtr<ID3D11VertexShader> VertexShader;
    TComPtr<ID3D11PixelShader>  PixelShader;
    TComPtr<ID3D11InputLayout>  InputLayout;
    TComPtr<ID3D11Buffer>       ConstantBuffer;

    TComPtr<ID3D11RasterizerState>   SolidRasterizerState;
    TComPtr<ID3D11DepthStencilState> DepthStencilState;

    FGizmoMeshResource GizmoMeshes[static_cast<uint8>(EGizmoType::Count)];
};