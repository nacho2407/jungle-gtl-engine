#include "Render/Pass/PassEvent.h"

#include "Render/Pipeline/FrameContext.h"
#include "Render/Pipeline/ViewModeRenderPipeline.h"
#include "Render/Pipeline/ViewModeSurfaceResources.h"
#include "Render/Types/ShadingTypes.h"
#include "Render/Pipeline/RenderConstants.h"
#include "Render/Resource/RenderResources.h"
#include "Render/Culling/TileBasedLightCulling.h"

void BuildDefaultPassEvents(
    TArray<FPassEvent>& OutPrePassEvents,
    ID3D11DeviceContext* Context,
    const FFrameContext& Frame,
    FStateCache& Cache,
    const FViewModeRenderPipeline* ActiveViewPipeline,
    FViewModeSurfaceResources* ActiveViewSurfaces,
    FTileBasedLightCulling& LightCulling,
    FRenderResources& Resources)
{
    OutPrePassEvents.push_back({ ERenderPass::Opaque, EPassCompare::Equal, false, false,
        [Context, &Frame, &Cache, &Resources, &LightCulling, ActiveViewSurfaces]()
        {

			if (Frame.ViewMode == EViewMode::Wireframe)
                                         return; 
            // 1. Clustering 등록
            Context->OMSetRenderTargets(0, nullptr, nullptr);
            ID3D11ShaderResourceView* NullSRV = nullptr;
            Context->PSSetShaderResources(7, 1, &NullSRV);

            // Z-Prepass에서 생성된 원본 Depth SRV 바인딩

            if (Frame.DepthTexture && Frame.DepthCopyTexture)
            {
                Context->CopyResource(Frame.DepthCopyTexture, Frame.DepthTexture);
            }
            if (Frame.DepthCopySRV)
            {
                ID3D11ShaderResourceView* CSDepthSRV = Frame.DepthCopySRV;
                Context->CSSetShaderResources(1, 1, &CSDepthSRV);
            }

            // TODO: Light Culling CS 및 타일 마스크 UAV 바인딩, 디스패치

            Resources.GlobalLightBuffer.Update(Context, &Frame.CollectedLights.GlobalLights, sizeof(FGlobalLightConstants));
            ID3D11Buffer* LightCB = Resources.GlobalLightBuffer.GetBuffer();
            if (LightCB)
                Context->PSSetConstantBuffers(ECBSlot::Light, 1, &LightCB);

            // t6: LocalLights StructuredBuffer (Point + Spot)
            ID3D11Device* Device = nullptr;
            Context->GetDevice(&Device);
            Resources.UpdateLocalLights(Device, Context, Frame.CollectedLights.LocalLights);
            if (Device)
                Device->Release();
            if (Resources.LocalLightSRV)
            {
                Context->CSSetShaderResources(6, 1, &Resources.LocalLightSRV);
                Context->PSSetShaderResources(6, 1, &Resources.LocalLightSRV);
            }

            LightCulling.SetPointLightData(Frame.CollectedLights.LocalLights);
            LightCulling.Dispatch(Frame, true);


            Cache.DiffuseSRV = nullptr;
            Cache.bForceAll = true;
        } });
    if (ActiveViewPipeline && ActiveViewSurfaces)
    {
        const EShadingModel ShadingModel = ActiveViewPipeline->GetShadingModel();

        OutPrePassEvents.push_back({ ERenderPass::Z_Prepass, EPassCompare::Equal, true, false,
                                     [Context, &Frame, &Cache, ShadingModel, ActiveViewSurfaces]()
                                     {
                                         ID3D11ShaderResourceView* NullSRVs[6] = {};
                                         Context->PSSetShaderResources(0, ARRAYSIZE(NullSRVs), NullSRVs);

                                         // DepthStencilView만 바인딩
                                         Context->OMSetRenderTargets(0, nullptr, Frame.ViewportDSV);

                                         // Depth Clear
                                         Context->ClearDepthStencilView(Frame.ViewportDSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 0.0f, 0);
                                         Cache.DiffuseSRV = nullptr;
                                         Cache.bForceAll = true;
                                     } });

        OutPrePassEvents.push_back({ ERenderPass::Opaque, EPassCompare::Equal, false, false,
                                     [Context, &Frame, &Cache, ShadingModel, &Resources, &LightCulling, ActiveViewSurfaces]()
                                     {
                                         ////1. Clustering 등록
                                         //            Context->OMSetRenderTargets(0, nullptr, nullptr);
                                         //            ID3D11ShaderResourceView* NullSRV = nullptr;
                                         //            Context->PSSetShaderResources(7, 1, &NullSRV);

                                         //            // Z-Prepass에서 생성된 원본 Depth SRV 바인딩

                                         // if (Frame.DepthTexture && Frame.DepthCopyTexture)
                                         //            {
                                         //                Context->CopyResource(Frame.DepthCopyTexture, Frame.DepthTexture);
                                         //            }
                                         //            if (Frame.DepthCopySRV)
                                         //            {
                                         //                ID3D11ShaderResourceView* CSDepthSRV = Frame.DepthCopySRV;
                                         //                Context->CSSetShaderResources(1, 1, &CSDepthSRV);
                                         //            }

                                         //// TODO: Light Culling CS 및 타일 마스크 UAV 바인딩, 디스패치

                                         //            Resources.GlobalLightBuffer.Update(Context, &Frame.CollectedLights.GlobalLights, sizeof(FGlobalLightConstants));
                                         //            ID3D11Buffer* LightCB = Resources.GlobalLightBuffer.GetBuffer();
                                         //            if (LightCB)
                                         //                Context->PSSetConstantBuffers(ECBSlot::Light, 1, &LightCB);

                                         //            // t6: LocalLights StructuredBuffer (Point + Spot)
                                         //            ID3D11Device* Device = nullptr;
                                         //            Context->GetDevice(&Device);
                                         //            Resources.UpdateLocalLights(Device, Context, Frame.CollectedLights.LocalLights);
                                         //            if (Device)
                                         //                Device->Release();
                                         //            if (Resources.LocalLightSRV)
                                         //            {
                                         //                Context->CSSetShaderResources(6, 1, &Resources.LocalLightSRV);
                                         //                Context->PSSetShaderResources(6, 1, &Resources.LocalLightSRV);
                                         //            }

                                         //// LightCulling.Dispatch() 호출 전에
                                         //
                                         // LightCulling.SetPointLightData(Frame.CollectedLights.LocalLights);
                                         // LightCulling.Dispatch(Frame, true);


                                         // 2. Opaque Rendering
                                         ID3D11ShaderResourceView* NullSRVs[6] = {};
                                         Context->PSSetShaderResources(0, ARRAYSIZE(NullSRVs), NullSRVs);

                                         ActiveViewSurfaces->ClearBaseTargets(Context, ShadingModel);
                                         ActiveViewSurfaces->BindBaseDrawTargets(Context, ShadingModel, Frame.ViewportDSV);

                                         // TODO: 앞선 CS에서 만들어진 TileLightMask (SRV)를 Opaque의 픽셀 셰이더(PS)에 바인딩
                                         // ID3D11ShaderResourceView* LightMaskSRV = ...
                                         // Context->PSSetShaderResources(슬롯번호, 1, &LightMaskSRV);

                                         Cache.DiffuseSRV = nullptr;
                                         Cache.bForceAll = true;
                                     } });

        OutPrePassEvents.push_back({ ERenderPass::Decal, EPassCompare::Equal, true, false,
                                     [Context, &Frame, &Cache, ShadingModel, ActiveViewSurfaces]()
                                     {
                                         ID3D11ShaderResourceView* NullSRVs[6] = {};
                                         Context->PSSetShaderResources(0, ARRAYSIZE(NullSRVs), NullSRVs);

                                         if (Frame.DepthTexture && Frame.DepthCopyTexture)
                                         {
                                             Context->OMSetRenderTargets(0, nullptr, nullptr);
                                             Context->CopyResource(Frame.DepthCopyTexture, Frame.DepthTexture);
                                         }

                                         ID3D11ShaderResourceView* BaseSRVs[3] = {
                                             ActiveViewSurfaces->GetSRV(ESurfaceSlot::BaseColor),
                                             ActiveViewSurfaces->GetSRV(ESurfaceSlot::Surface1),
                                             ActiveViewSurfaces->GetSRV(ESurfaceSlot::Surface2),
                                         };
                                         Context->PSSetShaderResources(1, ARRAYSIZE(BaseSRVs), BaseSRVs);

                                         if (Frame.DepthCopySRV)
                                         {
                                             ID3D11ShaderResourceView* DepthSRV = Frame.DepthCopySRV;
                                             Context->PSSetShaderResources(ESystemTexSlot::SceneDepth, 1, &DepthSRV);
                                         }

                                         ActiveViewSurfaces->ClearModifiedTargets(Context, ShadingModel);
                                         ActiveViewSurfaces->BindDecalTargets(Context, ShadingModel, Frame.ViewportDSV);

                                         Cache.DiffuseSRV = nullptr;
                                         Cache.bForceAll = true;
                                     } });

        OutPrePassEvents.push_back({ ERenderPass::Lighting, EPassCompare::Equal, true, false,
                                     [Context, &Frame, &Cache, ActiveViewSurfaces]()
                                     {
                                         ID3D11RenderTargetView* RTV = Frame.ViewportRTV;
                                         Context->OMSetRenderTargets(1, &RTV, Frame.ViewportDSV);


                                         // t0~t5: GBuffer 6장 (원본 + 데칼 Modified)
                                         ID3D11ShaderResourceView* SurfaceSRVs[6] = {
                                             ActiveViewSurfaces->GetSRV(ESurfaceSlot::BaseColor),
                                             ActiveViewSurfaces->GetSRV(ESurfaceSlot::Surface1),
                                             ActiveViewSurfaces->GetSRV(ESurfaceSlot::Surface2),
                                             ActiveViewSurfaces->GetSRV(ESurfaceSlot::ModifiedBaseColor),
                                             ActiveViewSurfaces->GetSRV(ESurfaceSlot::ModifiedSurface1),
                                             ActiveViewSurfaces->GetSRV(ESurfaceSlot::ModifiedSurface2),
                                         };
                                         Context->PSSetShaderResources(0, ARRAYSIZE(SurfaceSRVs), SurfaceSRVs);

                                         // t10: SceneDepth (Blinn-Phong 월드 위치 재구성용)
                                         if (Frame.DepthCopySRV)
                                         {
                                             ID3D11ShaderResourceView* DepthSRV = Frame.DepthCopySRV;
                                             Context->PSSetShaderResources(ESystemTexSlot::SceneDepth, 1, &DepthSRV);
                                         }

                                         Cache.DiffuseSRV = nullptr;
                                         Cache.bForceAll = true;
                                     } });
    }

    if (Frame.DepthTexture && Frame.DepthCopyTexture)
    {
        OutPrePassEvents.push_back({ ERenderPass::PostProcess, EPassCompare::GreaterEqual, true, false,
                                     [Context, &Frame, &LightCulling, &Cache]()
                                     {
                                         Context->OMSetRenderTargets(0, nullptr, nullptr);
                                         Context->CopyResource(Frame.DepthCopyTexture, Frame.DepthTexture);
                                         Context->OMSetRenderTargets(1, &Cache.RTV, Cache.DSV);

                                         ID3D11ShaderResourceView* depthSRV = Frame.DepthCopySRV;
                                         Context->PSSetShaderResources(ESystemTexSlot::SceneDepth, 1, &depthSRV);

                                         if (Frame.StencilCopySRV)
                                         {
                                             ID3D11ShaderResourceView* stencilSRV = Frame.StencilCopySRV;
                                             Context->PSSetShaderResources(ESystemTexSlot::Stencil, 1, &stencilSRV);
                                         }

                                         // 히트맵 텍스처를 남는 슬롯(예: t7)에 바인딩합니다.
                                         ID3D11ShaderResourceView* HitMapSRV = LightCulling.GetDebugHitMapSRV();
                                         if (HitMapSRV)
                                         {
                                             Context->PSSetShaderResources(7, 1, &HitMapSRV); // 7번 슬롯 사용
                                         }

                                         Cache.bForceAll = true;
                                     } });
    }

    if (Frame.SceneColorCopyTexture && Frame.ViewportRenderTexture)
    {
        OutPrePassEvents.push_back({ ERenderPass::FXAA, EPassCompare::Equal, true, false,
                                     [Context, &Frame, &Cache]()
                                     {
                                         Context->CopyResource(Frame.SceneColorCopyTexture, Frame.ViewportRenderTexture);
                                         Context->OMSetRenderTargets(1, &Cache.RTV, Cache.DSV);

                                         ID3D11ShaderResourceView* sceneColorSRV = Frame.SceneColorCopySRV;
                                         Context->PSSetShaderResources(ESystemTexSlot::SceneColor, 1, &sceneColorSRV);


                                         Cache.bForceAll = true;
                                     } });
    }
}
