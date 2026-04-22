#include "Render/Execute/Passes/Scene/LightingPass.h"
#include "Render/Execute/Context/RenderPipelineContext.h"
#include "Render/Submission/Command/DrawCommandList.h"
#include "Render/Submission/Command/BuildDrawCommand.h"
#include "Render/Scene/Proxies/Primitive/PrimitiveSceneProxy.h"
#include "Render/Execute/Context/ViewMode/SceneViewModeSurfaces.h"
#include "Render/Execute/Registry/ViewModePassRegistry.h"
#include "Render/Execute/Context/Scene/SceneView.h"
#include "Render/Resources/RenderResources.h"
#include "Render/Execute/Context/Viewport/ViewportRenderTargets.h"
#include "Render/Execute/Context/FrameRenderResources.h"
#include "Render/Visibility/TileBasedLightCulling.h"
#include "Editor/UI/EditorConsolePanel.h"

namespace
{
void BuildSurfaceSRVTable(const FRenderPipelineContext& Context, EShadingModel ShadingModel, ID3D11ShaderResourceView* OutSurfaceSRVs[6])
{
    for (uint32 i = 0; i < 6; ++i)
    {
        OutSurfaceSRVs[i] = nullptr;
    }

    if (!Context.ActiveViewSurfaces)
    {
        return;
    }

    OutSurfaceSRVs[0] = Context.ActiveViewSurfaces->GetSRV(ESceneViewModeSurfaceSlot::BaseColor);
    OutSurfaceSRVs[3] = Context.ActiveViewSurfaces->GetSRV(ESceneViewModeSurfaceSlot::ModifiedBaseColor);

    switch (ShadingModel)
    {
    case EShadingModel::Gouraud:
        // Surface1 = GouraudL, ������ ǥ���� ������� �ʽ��ϴ�.
        OutSurfaceSRVs[1] = Context.ActiveViewSurfaces->GetSRV(ESceneViewModeSurfaceSlot::Surface1);
        break;

    case EShadingModel::Lambert:
        // Surface1 = Normal, ModifiedSurface1 = Decal ���� Normal
        OutSurfaceSRVs[1] = Context.ActiveViewSurfaces->GetSRV(ESceneViewModeSurfaceSlot::Surface1);
        OutSurfaceSRVs[4] = Context.ActiveViewSurfaces->GetSRV(ESceneViewModeSurfaceSlot::ModifiedSurface1);
        break;

    case EShadingModel::BlinnPhong:
        // Surface1 = Normal, Surface2 = MaterialParam
        OutSurfaceSRVs[1] = Context.ActiveViewSurfaces->GetSRV(ESceneViewModeSurfaceSlot::Surface1);
        OutSurfaceSRVs[2] = Context.ActiveViewSurfaces->GetSRV(ESceneViewModeSurfaceSlot::Surface2);
        OutSurfaceSRVs[4] = Context.ActiveViewSurfaces->GetSRV(ESceneViewModeSurfaceSlot::ModifiedSurface1);
        OutSurfaceSRVs[5] = Context.ActiveViewSurfaces->GetSRV(ESceneViewModeSurfaceSlot::ModifiedSurface2);
        break;

    case EShadingModel::Unlit:
    default:
        break;
    }
}
} // namespace

void FLightingPass::PrepareInputs(FRenderPipelineContext& Context)
{
    const FViewportRenderTargets* Targets = Context.Targets;
    if (!Context.ActiveViewSurfaces || !Context.ViewModePassRegistry || !Context.ViewModePassRegistry->HasConfig(Context.ActiveViewMode))
    {
        return;
    }

    const EShadingModel ShadingModel = Context.ViewModePassRegistry->GetShadingModel(Context.ActiveViewMode);
    if (ShadingModel == EShadingModel::Unlit)
    {
        return;
    }

    const bool bNeedsReadableDepth = Targets && Targets->DepthTexture && Targets->DepthCopyTexture &&
                                     Targets->DepthTexture != Targets->DepthCopyTexture;

    Context.Context->OMSetRenderTargets(0, nullptr, nullptr);

    if (bNeedsReadableDepth)
    {
        Context.Context->CopyResource(Targets->DepthCopyTexture, Targets->DepthTexture);
    }

    ID3D11ShaderResourceView* SurfaceSRVs[6] = {};
    BuildSurfaceSRVTable(Context, ShadingModel, SurfaceSRVs);
    Context.Context->PSSetShaderResources(0, ARRAY_SIZE(SurfaceSRVs), SurfaceSRVs);

	// LightCulling ���� ������ ���̵�
    if (Context.LightCulling)
    {
        // TileMask SRV �߰�
        ID3D11ShaderResourceView* TileMaskSRV = Context.LightCulling->GetPerTileMaskSRV();
        Context.Context->PSSetShaderResources(7, 1, &TileMaskSRV);

		// ����� �� ��Ʈ�� SRV �߰�
		ID3D11ShaderResourceView* HipMapSRV = Context.LightCulling->GetDebugHitMapSRV();
        Context.Context->PSSetShaderResources(8, 1, &HipMapSRV);

		//b2 LightCullingParams
		ID3D11Buffer* LightCullingParamsCB = Context.LightCulling->GetLightCullingParamsCB();
        Context.Context->PSSetConstantBuffers(ECBSlot::PerShader0, 1, &LightCullingParamsCB);
    }

    if (Targets && Targets->DepthCopySRV)
    {
        ID3D11ShaderResourceView* DepthSRV = Targets->DepthCopySRV;
        Context.Context->PSSetShaderResources(ESystemTexSlot::SceneDepth, 1, &DepthSRV);
    }

    if (Context.Resources)
    {
        ID3D11Buffer* GlobalLightBuffer = Context.Resources->GlobalLightBuffer.GetBuffer();
        Context.Context->PSSetConstantBuffers(ECBSlot::Light, 1, &GlobalLightBuffer);

        ID3D11ShaderResourceView* LocalLightsSRV = Context.Resources->LocalLightSRV;
        Context.Context->PSSetShaderResources(ESystemTexSlot::LocalLights, 1, &LocalLightsSRV);
    }

    if (Context.StateCache)
    {
        Context.StateCache->LightCB = Context.Resources ? &Context.Resources->GlobalLightBuffer : nullptr;
        Context.StateCache->LocalLightSRV = Context.Resources ? Context.Resources->LocalLightSRV : nullptr;
        Context.StateCache->DiffuseSRV = nullptr;
        Context.StateCache->NormalSRV = nullptr;
        Context.StateCache->SpecularSRV = nullptr;
        Context.StateCache->bForceAll = true;
    }
}

void FLightingPass::PrepareTargets(FRenderPipelineContext& Context)
{
    BindViewportTarget(Context);
}

void FLightingPass::BuildDrawCommands(FRenderPipelineContext& Context)
{
    if (!Context.ActiveViewSurfaces || !Context.ViewModePassRegistry || !Context.ViewModePassRegistry->HasConfig(Context.ActiveViewMode))
    {
        return;
    }

    if (Context.ViewModePassRegistry->GetShadingModel(Context.ActiveViewMode) == EShadingModel::Unlit)
    {
        return;
    }

    DrawCommandBuilder::BuildFullscreenDrawCommand(ERenderPass::Lighting, Context, *Context.DrawCommandList);

    if (!Context.DrawCommandList || Context.DrawCommandList->GetCommands().empty())
    {
        return;
    }

    FDrawCommand& Command = Context.DrawCommandList->GetCommands().back();
    Command.PerShaderCB[0] = Context.LightCulling ? Context.LightCulling->GetLightCullingParamsCBWrapper() : nullptr;
}

void FLightingPass::SubmitDrawCommands(FRenderPipelineContext& Context)
{
    const FViewportRenderTargets* Targets = Context.Targets;
    if (!Context.DrawCommandList)
    {
        return;
    }

	// ---- ⏱️ 1. 쿼리 지연 초기화 (최초 1회만 실행) ----
    if (!bQueryInitialized && Context.Context)
    {
        ID3D11Device* device = nullptr;
        Context.Context->GetDevice(&device); // DeviceContext에서 Device를 얻어옴

        if (device)
        {
            D3D11_QUERY_DESC queryDesc;
            queryDesc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
            queryDesc.MiscFlags = 0;
            device->CreateQuery(&queryDesc, &DisjointQuery);

            queryDesc.Query = D3D11_QUERY_TIMESTAMP;
            device->CreateQuery(&queryDesc, &TimestampStartQuery);
            device->CreateQuery(&queryDesc, &TimestampEndQuery);

            device->Release(); // GetDevice는 참조 카운트를 올리므로 꼭 Release() 해줘야 메모리 누수가 안 생깁니다.
            bQueryInitialized = true;
        }
    }

    // ---- ⏱️ 2. GPU 타이밍 측정 시작 ----
    if (bQueryInitialized)
    {
        Context.Context->Begin(DisjointQuery);
        Context.Context->End(TimestampStartQuery);
    }

    SubmitPassRange(Context, ERenderPass::Lighting);

	if (bQueryInitialized)
    {
        Context.Context->End(TimestampEndQuery);
        Context.Context->End(DisjointQuery);

        // 주의: 이 루프는 GPU가 연산을 마칠 때까지 CPU를 멈춰 세웁니다 (성능 측정용으로만 사용)
        D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjointData;
        while (Context.Context->GetData(DisjointQuery, &disjointData, sizeof(disjointData), 0) == S_FALSE)
        {
        }

        if (disjointData.Disjoint == FALSE)
        {
            UINT64 startTime = 0;
            while (Context.Context->GetData(TimestampStartQuery, &startTime, sizeof(startTime), 0) == S_FALSE)
            {
            }

            UINT64 endTime = 0;
            while (Context.Context->GetData(TimestampEndQuery, &endTime, sizeof(endTime), 0) == S_FALSE)
            {
            }

            LastGPUTimeMs = float(endTime - startTime) / float(disjointData.Frequency) * 1000.0f;

            // 💡 여기서 브레이크 포인트를 걸거나 콘솔에 출력해서 ms를 확인하세요!
            UE_LOG("Lighting Pass GPU Time: %f ms", LastGPUTimeMs);
        }
    }

    if (Targets && Targets->ViewportRenderTexture && Targets->SceneColorCopyTexture &&
        Targets->ViewportRenderTexture != Targets->SceneColorCopyTexture)
    {
        ID3D11ShaderResourceView* NullSRV = nullptr;
        Context.Context->PSSetShaderResources(ESystemTexSlot::SceneDepth, 1, &NullSRV);
        Context.Context->OMSetRenderTargets(0, nullptr, nullptr);
        Context.Context->CopyResource(Targets->SceneColorCopyTexture, Targets->ViewportRenderTexture);

        BindViewportTarget(Context);
    }

	ID3D11ShaderResourceView* nullSRV = {};
    Context.Context->PSSetShaderResources(7, 1, &nullSRV);

    Context.Context->PSSetShaderResources(8, 1, &nullSRV);
}
