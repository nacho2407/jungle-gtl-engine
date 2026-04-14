#pragma once
#include "Core/CoreMinimal.h"
#include "RenderPassContext.h"

class FFXAARenderPass;
class FFogRenderPass;
class FLightRenderPass;
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
    std::shared_ptr<FLightRenderPass> LightRenderPass;
    std::shared_ptr<FFogRenderPass> FogRenderPass;
    std::shared_ptr<FFXAARenderPass> FXAARenderPass;
    ID3D11ShaderResourceView* OutSRV = nullptr;
    ID3D11RenderTargetView* OutRTV = nullptr;

	TArray<std::shared_ptr<FBaseRenderPass>> RenderPasses;
};
