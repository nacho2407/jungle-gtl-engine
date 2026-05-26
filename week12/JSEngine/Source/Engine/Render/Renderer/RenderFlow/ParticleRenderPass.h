#pragma once
#include "RenderPass.h"
#include "Render/Common/ComPtr.h"

struct ID3D11Buffer;

class FParticleRenderPass : public FBaseRenderPass
{
public:
	bool Initialize() override;
	bool Release() override;

private:
	TComPtr<ID3D11Buffer> QuadVertexBuffer;
	TComPtr<ID3D11Buffer> IndexBuffer;

	bool Begin(const FRenderPassContext* Context) override;
	bool DrawCommand(const FRenderPassContext* Context) override;
	bool End(const FRenderPassContext* Context) override;
	bool EnsureQuadBuffers(const FRenderPassContext* Context);
};
