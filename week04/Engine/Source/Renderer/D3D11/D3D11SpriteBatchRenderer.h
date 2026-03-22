#pragma once

#include "Core/Math/Vector.h"
#include "Core/Math/Color.h"
#include "Core/Containers/Array.h"
#include "Renderer/Types/VertexTypes.h"
#include "Renderer/D3D11/D3D11Common.h"

class FD3D11DynamicRHI;
class FSceneView;
class UParticleSubUVComp;

class FD3D11SpriteBatchRenderer
{
  public:
    void Initialize(FD3D11DynamicRHI* InRHI) {}
    void Shutdown() {}

    void BeginFrame() {}

    void SubmitSpriteWorldSpace(const FVector& InCenter, float InWidth, float InHeight,
                                ID3D11ShaderResourceView* InTextureSRV, const FColor& InColor)
    {
    }

    void SubmitSubUVWorldSpace(const FVector& InCenter, float InWidth, float InHeight,
                               int32 InFrameIndex, int32 InNumFramesX, int32 InNumFramesY,
                               ID3D11ShaderResourceView* InTextureSRV, const FColor& InColor)
    {
    }

    void SubmitParticle(const UParticleSubUVComp* InParticleComp, const FSceneView* InSceneView) {}

    void EndFrame(const FSceneView* InSceneView) {}

  private:
    void CreateShaders() {}
    void CreateInputLayout() {}
    void CreateConstantBuffer() {}
    void CreateStates() {}
    void CreateBuffers() {}

    void BuildBillboardQuad(const FSceneView* InSceneView, const FVector& InCenter, float InWidth,
                            float InHeight, float InU0, float InV0, float InU1, float InV1,
                            const FColor& InColor)
    {
    }

    void CalcSubUV(int32 InFrameIndex, int32 InNumFramesX, int32 InNumFramesY, float& OutU0,
                   float& OutV0, float& OutU1, float& OutV1) const
    {
    }

    void Flush(const FSceneView* InSceneView) {}

  private:
    FD3D11DynamicRHI* RHI = nullptr;

    TArray<FSpriteVertex> Vertices;
    TArray<uint32>        Indices;

    TComPtr<ID3D11VertexShader> VertexShader;
    TComPtr<ID3D11PixelShader>  PixelShader;
    TComPtr<ID3D11InputLayout>  InputLayout;

    TComPtr<ID3D11Buffer> ConstantBuffer;
    TComPtr<ID3D11Buffer> DynamicVertexBuffer;
    TComPtr<ID3D11Buffer> DynamicIndexBuffer;

    TComPtr<ID3D11SamplerState>      SamplerState;
    TComPtr<ID3D11BlendState>        AlphaBlendState;
    TComPtr<ID3D11DepthStencilState> DepthStencilState;
};