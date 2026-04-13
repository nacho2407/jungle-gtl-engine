#pragma once
#include "Core/CoreMinimal.h"
#include "RenderPassContext.h"

class FOpaqueRenderPass;

class FRenderPipeline
{
public:
    bool Initialize();
    bool Render(const FRenderPassContext* Context);
    void Release();
private:
    std::shared_ptr<FOpaqueRenderPass> OpaqueRenderPass;
};
