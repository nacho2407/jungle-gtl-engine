#pragma once
#include "RenderPass.h"

class FOpaqueRenderPass : public FBaseRenderPass
{
public:
    bool Initialize() override;
    bool Release() override;

    void DeclareInputs(TArray<FResourceBinding>& OutInputs) const override;
    void DeclareOutputs(TArray<FResourceBinding>& OutOutputs) const override;

protected:
    bool Begin(const FRenderPassContext* Context) override;
    bool DrawCommand(const FRenderPassContext* Context) override;
    bool End(const FRenderPassContext* Context) override;
};
