#include "LightRenderPass.h"
#include "Core/ResourceManager.h"
#include "Render/Resource/ShaderPaths.h"

namespace
{
    FShaderProgram* GetLightPassProgram()
    {
        FShaderStageKey VSKey;
        VSKey.FilePath = FShaderPaths::PostProcessLight;
        VSKey.EntryPoint = "mainVS";

        FShaderStageKey PSKey;
        PSKey.FilePath = FShaderPaths::PostProcessLight;
        PSKey.EntryPoint = "mainPS";

        return FResourceManager::Get().GetOrCreateShaderProgram(VSKey, PSKey);
    }
}

bool FLightRenderPass::Initialize()
{
    return true;
}

bool FLightRenderPass::Release()
{
    return true;
}

bool FLightRenderPass::Begin(const FRenderPassContext* Context)
{
    const FRenderTargetSet* RenderTargets = Context->RenderTargets;
    ID3D11RenderTargetView* RTVs[1] = {
        RenderTargets->SceneLightRTV
    };
    ID3D11DepthStencilView* DSV = nullptr;

    Context->DeviceContext->OMSetRenderTargets(ARRAYSIZE(RTVs), RTVs, DSV);
    OutSRV = RenderTargets->SceneLightSRV;
    OutRTV = RenderTargets->SceneLightRTV;

	const FRenderBus* RenderBus = Context->RenderBus;

    Context->DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    //const auto& Lights = RenderBus->GetLight();
    //Context->RenderResources->LightStructuredBuffer.Update(Context->DeviceContext, Lights.data(), (uint32)Lights.size());

    FLightPassConstants LightPassConstant = {};

    switch (RenderBus->GetViewMode())
    {
    case (EViewMode::Unlit):
	case (EViewMode::Wireframe):
        LightPassConstant.WorldLit = 1;
        break;
    case (EViewMode::Lit_Gouraud):
    case (EViewMode::Lit_Lambert):
    case (EViewMode::Lit_BlinnPhong):
		LightPassConstant.WorldLit = 0;
		break;
    default:
		break;
    }

    LightPassConstant.LightCount = static_cast<uint32>(RenderBus->LightInfos.size());
    LightPassConstant.CameraWorldPos = RenderBus->GetCameraPosition();
    LightPassConstant.ViewMode = static_cast<uint32>(RenderBus->GetViewMode());
    Context->RenderResources->LightPassConstantBuffer.Update(Context->DeviceContext, &LightPassConstant, sizeof(LightPassConstant));
    ID3D11Buffer* cb7 = Context->RenderResources->LightPassConstantBuffer.GetBuffer();
    Context->DeviceContext->PSSetConstantBuffers(7, 1, &cb7);

    ID3D11ShaderResourceView* srvs[] = {
        Context->RenderTargets->SceneColorSRV,
        Context->RenderTargets->SceneNormalSRV,
        Context->RenderTargets->SceneDepthSRV,
        Context->RenderTargets->SceneWorldPosSRV,
    };

    Context->DeviceContext->PSSetShaderResources(0, 4, srvs);

    ID3D11ShaderResourceView* ShadowInfoSRVs[] = {
        Context->RenderResources->LightShadowIndexBuffer.GetSRV(),
        Context->RenderResources->AtlasShadowBuffer.GetSRV(),
    };
    Context->DeviceContext->PSSetShaderResources(14, 2, ShadowInfoSRVs);

    FShaderProgram* LightPassProgram = GetLightPassProgram();
    if (!LightPassProgram)
    {
        return false;
    }
    LightPassProgram->Bind(Context->DeviceContext);

    /**
     * LightPass 는 풀스크린 쿼드에 그려지는데, mainVS 에서	정점 데이터를 생성하기 때문에 IA 단계에서 별도의
     * 버퍼 바인딩이 필요 없다.
     */
    Context->DeviceContext->IASetInputLayout(nullptr);
    Context->DeviceContext->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
    Context->DeviceContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
    Context->DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    return true;
}

bool FLightRenderPass::DrawCommand(const FRenderPassContext* Context)
{
    Context->DeviceContext->Draw(3, 0);

    return true;
}

bool FLightRenderPass::End(const FRenderPassContext* Context)
{
    // SRV 해제 => RTV 와 SRV 가 동시에 쓰이지 않게 방지
    ID3D11ShaderResourceView* nullSRVs[] = { nullptr, nullptr, nullptr, nullptr, nullptr };
    Context->DeviceContext->PSSetShaderResources(0, 5, nullSRVs);
    ID3D11ShaderResourceView* nullShadowSRVs[] = { nullptr, nullptr };
    Context->DeviceContext->PSSetShaderResources(14, 2, nullShadowSRVs);
    return true;
}
