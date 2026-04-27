#pragma once
#include "RenderPass.h"

class FVSMConversionRenderPass : public FBaseRenderPass
{
public:
    bool Initialize() override;
    bool Release() override;

protected:
    bool Begin(const FRenderPassContext* Context) override;
    bool DrawCommand(const FRenderPassContext* Context) override;
    bool End(const FRenderPassContext* Context) override;


	private:
    bool DrawVSMConversion(const FRenderPassContext* Context);      // depth, depth² 변환
    bool DispatchHorizontalBlur(const FRenderPassContext* Context); // Horizontal CS
    bool DispatchVerticalBlur(const FRenderPassContext* Context);   // Vertical CS
};
