#pragma once
#include "RenderPass.h"

class FSandevistanRenderPass : public FBaseRenderPass
{
public:
    bool Initialize() override;
    bool Release() override;

    virtual bool Begin(const FRenderPassContext* Context) override;
    virtual bool DrawCommand(const FRenderPassContext* Context) override;
    virtual bool End(const FRenderPassContext* Context) override;
};