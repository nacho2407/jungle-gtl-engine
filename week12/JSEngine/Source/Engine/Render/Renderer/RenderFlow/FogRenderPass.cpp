#include "FogRenderPass.h"
#include "Core/ResourceManager.h"
#include "Render/Scene/RenderBus.h"
#include "Render/Resource/RenderResources.h"
#include "Render/Resource/ShaderPaths.h"
#include "Render/Scene/RenderCommand.h"
#include <algorithm>

namespace
{
    bool IsDebugViewMode(EViewMode ViewMode)
    {
        return ViewMode == EViewMode::Normal ||
            ViewMode == EViewMode::Heatmap ||
            ViewMode == EViewMode::BoneWeightHeatmap ||
            ViewMode == EViewMode::Depth;
    }

    FShaderProgram* GetFogProgram()
    {
        FShaderStageKey VSKey;
        VSKey.FilePath = FShaderPaths::PostProcessFog;
        VSKey.EntryPoint = "mainVS";

        FShaderStageKey PSKey;
        PSKey.FilePath = FShaderPaths::PostProcessFog;
        PSKey.EntryPoint = "mainPS";

        return FResourceManager::Get().GetOrCreateShaderProgram(VSKey, PSKey);
    }
}

bool FFogRenderPass::Initialize()
{
    return true;
}

bool FFogRenderPass::Release()
{
    return true;
}

bool FFogRenderPass::Begin(const FRenderPassContext* Context)
{
    const TArray<FRenderCommand>& Commands = Context->RenderBus->GetCommands(ERenderPass::Fog);
    if (Commands.empty() || IsDebugViewMode(Context->RenderBus->GetViewMode()) || PrevPassSRV == nullptr)
    {
        // Fog command가 없으면 이전 패스 결과를 그대로 넘긴다.
        OutSRV = PrevPassSRV;
        OutRTV = PrevPassRTV;
        return true;
    }

    if (!Context->RenderTargets->SceneFogRTV || !Context->RenderTargets->SceneFogSRV)
    {
        return false;
    }

    FShaderProgram* FogProgram = GetFogProgram();
    if (FogProgram)
    {
        FogProgram->Bind(Context->DeviceContext);
    }

    ID3D11DepthStencilState* DSState = FResourceManager::Get().GetOrCreateDepthStencilState(EDepthStencilType::StencilWrite);
    Context->DeviceContext->OMSetDepthStencilState(DSState, 1);

    ID3D11BlendState* BlendState = FResourceManager::Get().GetOrCreateBlendState(EBlendType::Opaque);
    Context->DeviceContext->OMSetBlendState(BlendState, nullptr, 0xFFFFFFFF);

    ID3D11RenderTargetView* RTVs[1] = { Context->RenderTargets->SceneFogRTV };
    Context->DeviceContext->OMSetRenderTargets(1, RTVs, nullptr);

    Context->DeviceContext->IASetInputLayout(nullptr);
    Context->DeviceContext->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
    Context->DeviceContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
    Context->DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    OutSRV = Context->RenderTargets->SceneFogSRV;
    OutRTV = Context->RenderTargets->SceneFogRTV;
    return true;
}

bool FFogRenderPass::DrawCommand(const FRenderPassContext* Context)
{
    const TArray<FRenderCommand>& Commands = Context->RenderBus->GetCommands(ERenderPass::Fog);
    if (Commands.empty() || IsDebugViewMode(Context->RenderBus->GetViewMode()) || PrevPassSRV == nullptr)
    {
        return true;
    }

    ID3D11ShaderResourceView* srvs[] = {
        PrevPassSRV,
        Context->RenderTargets->SceneDepthSRV
    };
    Context->DeviceContext->PSSetShaderResources(0, 2, srvs);

    FFogPassConstants FogPassConstants = {};
    FogPassConstants.FogCount = std::min<uint32>(static_cast<uint32>(Commands.size()), MaxFogLayerCount);
    for (uint32 FogIndex = 0; FogIndex < FogPassConstants.FogCount; ++FogIndex)
    {
        FogPassConstants.Layers[FogIndex] = Commands[FogIndex].Constants.Fog;
    }

    Context->RenderResources->FogPassConstantBuffer.Update(Context->DeviceContext, &FogPassConstants, sizeof(FFogPassConstants));
    ID3D11Buffer* cb9 = Context->RenderResources->FogPassConstantBuffer.GetBuffer();
    Context->DeviceContext->VSSetConstantBuffers(9, 1, &cb9);
    Context->DeviceContext->PSSetConstantBuffers(9, 1, &cb9);

    Context->DeviceContext->Draw(3, 0);

    return true;
}

bool FFogRenderPass::End(const FRenderPassContext* Context)
{
    ID3D11ShaderResourceView* nullSRVs[] = { nullptr, nullptr };
    Context->DeviceContext->PSSetShaderResources(0, 2, nullSRVs);
    return true;
}
