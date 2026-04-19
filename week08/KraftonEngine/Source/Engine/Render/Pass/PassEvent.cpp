#include "Render/Pass/PassEvent.h"

#include "Render/Pipeline/FrameContext.h"
#include "Render/Pipeline/ViewModeRenderPipeline.h"
#include "Render/Pipeline/ViewModeSurfaceResources.h"
#include "Render/Types/ShadingTypes.h"
#include "Render/Pipeline/RenderConstants.h"

void BuildDefaultPassEvents(
	TArray<FPassEvent>& OutPrePassEvents,
	ID3D11DeviceContext* Context,
	const FFrameContext& Frame,
	FStateCache& Cache,
	const FViewModeRenderPipeline* ActiveViewPipeline,
	FViewModeSurfaceResources* ActiveViewSurfaces)
{
	if (ActiveViewPipeline && ActiveViewSurfaces)
	{
		const EShadingModel ShadingModel = ActiveViewPipeline->GetShadingModel();

		OutPrePassEvents.push_back({ ERenderPass::Opaque, EPassCompare::Equal, true, false,
			[Context, &Frame, &Cache, ShadingModel, ActiveViewSurfaces]()
			{
				ID3D11ShaderResourceView* NullSRVs[6] = {};
				Context->PSSetShaderResources(0, ARRAYSIZE(NullSRVs), NullSRVs);

				ActiveViewSurfaces->ClearBaseTargets(Context, ShadingModel);
				ActiveViewSurfaces->BindBaseDrawTargets(Context, ShadingModel, Frame.ViewportDSV);

				Cache.DiffuseSRV = nullptr;
				Cache.bForceAll = true;
			}
		});

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
			}
		});

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

				// b4: GlobalLights — UpdateFrameBuffer에서 이미 업로드됨, 바인딩만 수행
				if (Frame.GlobalLightBuffer)
					Context->PSSetConstantBuffers(ECBSlot::Light, 1, &Frame.GlobalLightBuffer);

				// t6: LocalLights SRV — UpdateFrameBuffer에서 이미 업로드됨, 바인딩만 수행
				if (Frame.LocalLightsSRV)
					Context->PSSetShaderResources(ESystemTexSlot::LocalLights, 1, &Frame.LocalLightsSRV);

				// Cache를 GPU 실제 상태와 동기화
				Cache.LightCB       = nullptr;
				Cache.LocalLightSRV = Frame.LocalLightsSRV;
				Cache.DiffuseSRV    = nullptr;
				Cache.bForceAll     = true;
			}
		});
	}

	if (Frame.DepthTexture && Frame.DepthCopyTexture)
	{
		OutPrePassEvents.push_back({ ERenderPass::PostProcess, EPassCompare::GreaterEqual, true, false,
			[Context, &Frame, &Cache]()
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

				Cache.bForceAll = true;
			}
		});
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
			}
		});
	}
}
