#pragma once
#include "Core/CoreMinimal.h"
#include "RenderPassContext.h"

class FOpaqueRenderPass;
class FBaseRenderPass;

class FRenderPipeline
{
public:
    bool Initialize();
    bool Render(const FRenderPassContext* Context);
    void Release();

	ID3D11ShaderResourceView* GetOutSRV() const { return OutSRV; }

private:
    std::shared_ptr<FOpaqueRenderPass> OpaqueRenderPass;
    ID3D11ShaderResourceView* OutSRV = nullptr;

	TArray<std::shared_ptr<FBaseRenderPass>> RenderPasses;
};
