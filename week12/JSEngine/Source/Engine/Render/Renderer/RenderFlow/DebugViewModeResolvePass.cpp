#include "DebugViewModeResolvePass.h"
#include "Core/ResourceManager.h"
#include "Render/Resource/ShaderPaths.h"
#include "Render/Scene/RenderBus.h"

namespace
{
    bool IsDepthDebugViewMode(EViewMode ViewMode)
    {
        return ViewMode == EViewMode::Depth;
    }

    FShaderProgram* GetDebugViewModeResolveProgram()
    {
        FShaderStageKey VSKey;
        VSKey.FilePath = FShaderPaths::PostProcessDebugViewModeResolve;
        VSKey.EntryPoint = "mainVS";

        FShaderStageKey PSKey;
        PSKey.FilePath = FShaderPaths::PostProcessDebugViewModeResolve;
        PSKey.EntryPoint = "mainPS";

        return FResourceManager::Get().GetOrCreateShaderProgram(VSKey, PSKey);
    }
}

bool FDebugViewModeResolvePass::Initialize()
{
    return true;
}

bool FDebugViewModeResolvePass::Release()
{
    return true;
}

bool FDebugViewModeResolvePass::Begin(const FRenderPassContext* Context)
{
    const EViewMode ViewMode = Context->RenderBus->GetViewMode();
    if (!IsDepthDebugViewMode(ViewMode))
    {
        OutSRV = PrevPassSRV;
        OutRTV = PrevPassRTV;
        return true;
    }

    const FRenderTargetSet* RenderTargets = Context->RenderTargets;
    ID3D11RenderTargetView* RTVs[1] = {
        RenderTargets->DebugViewModeRTV
    };
    ID3D11DepthStencilView* DSV = nullptr;

    Context->DeviceContext->OMSetRenderTargets(ARRAYSIZE(RTVs), RTVs, DSV);
    OutSRV = RenderTargets->DebugViewModeSRV;
    OutRTV = RenderTargets->DebugViewModeRTV;

    FDebugViewModeResolveConstants DebugViewModeResolveConstant = {};
    DebugViewModeResolveConstant.ViewMode = static_cast<uint32>(ViewMode);
    Context->RenderResources->DebugViewModeResolveConstantBuffer.Update(
        Context->DeviceContext,
        &DebugViewModeResolveConstant,
        sizeof(DebugViewModeResolveConstant));
    ID3D11Buffer* cb7 = Context->RenderResources->DebugViewModeResolveConstantBuffer.GetBuffer();
    Context->DeviceContext->PSSetConstantBuffers(7, 1, &cb7);

    ID3D11ShaderResourceView* srvs[] = {
        Context->RenderTargets->SceneDepthSRV,
    };
    Context->DeviceContext->PSSetShaderResources(0, ARRAYSIZE(srvs), srvs);

    FShaderProgram* DebugViewModeResolveProgram = GetDebugViewModeResolveProgram();
    if (!DebugViewModeResolveProgram)
    {
        return false;
    }
    DebugViewModeResolveProgram->Bind(Context->DeviceContext);

    Context->DeviceContext->IASetInputLayout(nullptr);
    Context->DeviceContext->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
    Context->DeviceContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
    Context->DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    return true;
}

bool FDebugViewModeResolvePass::DrawCommand(const FRenderPassContext* Context)
{
    if (!IsDepthDebugViewMode(Context->RenderBus->GetViewMode()))
    {
        return true;
    }

    Context->DeviceContext->Draw(3, 0);
    return true;
}

bool FDebugViewModeResolvePass::End(const FRenderPassContext* Context)
{
    if (!IsDepthDebugViewMode(Context->RenderBus->GetViewMode()))
    {
        return true;
    }

    ID3D11ShaderResourceView* nullSRVs[] = { nullptr };
    Context->DeviceContext->PSSetShaderResources(0, ARRAYSIZE(nullSRVs), nullSRVs);
    return true;
}
