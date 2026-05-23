#include "ViewModeResolvePass.h"
#include "Core/ResourceManager.h"
#include "Render/Resource/ShaderPaths.h"

namespace
{
    FShaderProgram* GetViewModeResolveProgram()
    {
        FShaderStageKey VSKey;
        VSKey.FilePath = FShaderPaths::PostProcessViewModeResolve;
        VSKey.EntryPoint = "mainVS";

        FShaderStageKey PSKey;
        PSKey.FilePath = FShaderPaths::PostProcessViewModeResolve;
        PSKey.EntryPoint = "mainPS";

        return FResourceManager::Get().GetOrCreateShaderProgram(VSKey, PSKey);
    }
}

bool FViewModeResolvePass::Initialize()
{
    return true;
}

bool FViewModeResolvePass::Release()
{
    return true;
}

bool FViewModeResolvePass::Begin(const FRenderPassContext* Context)
{
    const FRenderTargetSet* RenderTargets = Context->RenderTargets;
    ID3D11RenderTargetView* RTVs[1] = {
        RenderTargets->SceneViewModeRTV
    };
    ID3D11DepthStencilView* DSV = nullptr;

    Context->DeviceContext->OMSetRenderTargets(ARRAYSIZE(RTVs), RTVs, DSV);
    OutSRV = RenderTargets->SceneViewModeSRV;
    OutRTV = RenderTargets->SceneViewModeRTV;

	const FRenderBus* RenderBus = Context->RenderBus;

    Context->DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    FViewModeResolveConstants ViewModeResolveConstant = {};
    ViewModeResolveConstant.ViewMode = static_cast<uint32>(RenderBus->GetViewMode());
    Context->RenderResources->ViewModeResolveConstantBuffer.Update(Context->DeviceContext, &ViewModeResolveConstant, sizeof(ViewModeResolveConstant));
    ID3D11Buffer* cb7 = Context->RenderResources->ViewModeResolveConstantBuffer.GetBuffer();
    Context->DeviceContext->PSSetConstantBuffers(7, 1, &cb7);

    ID3D11ShaderResourceView* srvs[] = {
        Context->RenderTargets->SceneColorSRV,
        Context->RenderTargets->SceneNormalSRV,
        Context->RenderTargets->SceneDepthSRV,
    };

    Context->DeviceContext->PSSetShaderResources(0, ARRAYSIZE(srvs), srvs);

    FShaderProgram* ViewModeResolveProgram = GetViewModeResolveProgram();
    if (!ViewModeResolveProgram)
    {
        return false;
    }
    ViewModeResolveProgram->Bind(Context->DeviceContext);

    /**
     * ViewModeResolvePass 는 풀스크린 쿼드에 그려지는데, mainVS 에서 정점 데이터를 생성하기 때문에 IA 단계에서 별도의
     * 버퍼 바인딩이 필요 없다.
     */
    Context->DeviceContext->IASetInputLayout(nullptr);
    Context->DeviceContext->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
    Context->DeviceContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
    Context->DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    return true;
}

bool FViewModeResolvePass::DrawCommand(const FRenderPassContext* Context)
{
    Context->DeviceContext->Draw(3, 0);

    return true;
}

bool FViewModeResolvePass::End(const FRenderPassContext* Context)
{
    // SRV 해제 => RTV 와 SRV 가 동시에 쓰이지 않게 방지
    ID3D11ShaderResourceView* nullSRVs[] = { nullptr, nullptr, nullptr };
    Context->DeviceContext->PSSetShaderResources(0, ARRAYSIZE(nullSRVs), nullSRVs);
    return true;
}
