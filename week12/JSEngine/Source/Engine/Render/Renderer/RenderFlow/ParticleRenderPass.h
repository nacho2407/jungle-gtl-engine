#pragma once
#include "RenderPass.h"
#include "Render/Common/ComPtr.h"
#include "Render/Resource/VertexTypes.h"

struct ID3D11Buffer;

struct FParticleSpriteInstanceData
{
    FVector Center;
    FVector AxisX;
    FVector AxisY;
    FColor Color;
};

class FParticleRenderPass : public FBaseRenderPass
{
public:
    bool Initialize() override;
    bool Release() override;
    
private:
    TArray<FParticleSpriteInstanceData> Instances;
    TComPtr<ID3D11Buffer> QuadVertexBuffer;
    TComPtr<ID3D11Buffer> IndexBuffer;
    TComPtr<ID3D11Buffer> InstanceBuffer;
    uint32 MaxInstanceCount = 0;

    bool Begin(const FRenderPassContext* Context) override;
    bool DrawCommand(const FRenderPassContext* Context) override;
    bool End(const FRenderPassContext* Context) override;
    bool EnsureQuadBuffers(const FRenderPassContext* Context);
    bool EnsureInstanceBuffer(const FRenderPassContext* Context, uint32 InstanceCount);
    void ClearBatch();
};
