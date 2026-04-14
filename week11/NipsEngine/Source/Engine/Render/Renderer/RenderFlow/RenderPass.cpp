#include "RenderPass.h"

bool FBaseRenderPass::Render(const FRenderPassContext* Context)
{
    bool bResult;
    
	bResult = Begin(Context);
    if (!bResult)
        return false;

    bResult = DrawCommand(Context);
    if (!bResult)
        return false;

    bResult = End(Context);
    if (!bResult)
        return false;

	return true;
}

void FBaseRenderPass::SetInput(const FString& Name, ID3D11ShaderResourceView* SRV)
{
    Inputs[Name] = SRV;
}

ID3D11ShaderResourceView* FBaseRenderPass::GetInput(const FString& Name) const
{
    auto it = Inputs.find(Name);
    return it != Inputs.end()? it->second : nullptr;
}

void FBaseRenderPass::SetOutput(const FString& Name, ID3D11RenderTargetView* RTV)
{
    Outputs[Name] = RTV;
}

ID3D11RenderTargetView* FBaseRenderPass::GetOutput(const FString& Name) const
{
    auto it = Outputs.find(Name);
    return it != Outputs.end() ? it->second : nullptr;
}
