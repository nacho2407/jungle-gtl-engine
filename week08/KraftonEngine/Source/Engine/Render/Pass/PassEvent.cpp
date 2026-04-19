#include "Render/Pass/PassEvent.h"

#include "Render/Pipeline/FrameContext.h"
#include "Render/Pipeline/ViewModeRenderPipeline.h"
#include "Render/Pipeline/ViewModeSurfaceResources.h"
#include "Render/Types/ShadingTypes.h"
#include "Render/Pipeline/RenderConstants.h"
#include "Render/Resource/RenderResources.h"

void BuildDefaultPassEvents(
	TArray<FPassEvent>& OutPrePassEvents,
	ID3D11DeviceContext* Context,
	const FFrameContext& Frame,
	FStateCache& Cache,
	const FViewModeRenderPipeline* ActiveViewPipeline,
	FViewModeSurfaceResources* ActiveViewSurfaces,
	FRenderResources& Resources)
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
			[Context, &Frame, &Cache, &Resources, ActiveViewSurfaces]()
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

				// b4: GlobalLights (Ambient + Directional)
				Resources.GlobalLightBuffer.Update(Context,
					&Frame.CollectedLights.GlobalLights,
					sizeof(FGlobalLightConstants));
				ID3D11Buffer* LightCB = Resources.GlobalLightBuffer.GetBuffer();
				if (LightCB)
					Context->PSSetConstantBuffers(ECBSlot::Light, 1, &LightCB);

				// t6: LocalLights StructuredBuffer (Point + Spot)
				ID3D11Device* Device = nullptr;
				Context->GetDevice(&Device);
				Resources.UpdateLocalLights(Device, Context, Frame.CollectedLights.LocalLights);
				if (Device) Device->Release();
				if (Resources.LocalLightSRV)
					Context->PSSetShaderResources(ESystemTexSlot::LocalLights, 1, &Resources.LocalLightSRV);

				// Cache를 GPU 실제 상태와 동기화
				// SubmitCommand의 bForce 경로가 b4/t6을 잘못 덮어쓰지 않도록 보장
				Cache.LightCB        = &Resources.GlobalLightBuffer;
				Cache.LocalLightSRV  = Resources.LocalLightSRV;
				Cache.DiffuseSRV     = nullptr;
				Cache.bForceAll      = true;
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
