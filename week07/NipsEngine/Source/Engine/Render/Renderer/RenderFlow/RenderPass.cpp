#include "RenderPass.h"
#include "Core/ResourceManager.h"

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

void FBaseRenderPass::CheckOverrideViewMode(const FRenderPassContext* Context)
{
    /*
        RasterizerState 를 Material 에서 들고 있는 상태라, 전역 설정인 ViewMode 간의 override 가 필요한데
        현재는 우선 Bind 이후 Override 하는 방식으로 임시 처리
        향후 수정 필요
    */
    if (Context->RenderBus->GetViewMode() == EViewMode::Wireframe)
    {
        ID3D11RasterizerState* WireRS = FResourceManager::Get().GetOrCreateRasterizerState(ERasterizerType::WireFrame);
        Context->DeviceContext->RSSetState(WireRS);
    }
}

