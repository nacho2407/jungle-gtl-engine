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

