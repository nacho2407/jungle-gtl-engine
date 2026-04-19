#include "Render/Core/PassEvent.h"

#include "Render/Core/FrameContext.h"
#include "Render/Core/PassTypes.h"
#include "Render/Resource/RenderResources.h"
#include "Render/Types/ShadingTypes.h"
#include "Render/Core/RenderConstants.h"

void BuildDefaultPassEvents(
	TArray<FPassEvent>& OutPrePassEvents,
	ID3D11DeviceContext* Context,
	const FFrameContext& Frame,
	FStateCache& Cache,
	const FViewModePassRegistry* ViewModePassRegistry,
	EViewMode ActiveViewMode,
	FViewModeSurfaceSet* ActiveViewSurfaceSet)
{
	if (ViewModePassRegistry && ViewModePassRegistry->HasConfig(ActiveViewMode) && ActiveViewSurfaceSet)
	{
		const EShadingModel ShadingModel = ViewModePassRegistry->GetShadingModel(ActiveViewMode);

		OutPrePassEvents.push_back({ ERenderPass::Opaque, EPassCompare::Equal, true, false,
			[Context, &Frame, &Cache, ShadingModel, ActiveViewSurfaceSet]()
			{
				ID3D11ShaderResourceView* NullSRVs[6] = {};
				Context->PSSetShaderResources(0, ARRAYSIZE(NullSRVs), NullSRVs);

				ActiveViewSurfaceSet->ClearBaseTargets(Context, ShadingModel);
				ActiveViewSurfaceSet->BindBaseDrawTargets(Context, ShadingModel, Frame.ViewportDSV);

				Cache.DiffuseSRV = nullptr;
				Cache.bForceAll = true;
			}
		});

		OutPrePassEvents.push_back({ ERenderPass::Decal, EPassCompare::Equal, true, false,
			[Context, &Frame, &Cache, ShadingModel, ActiveViewSurfaceSet]()
			{
				ID3D11ShaderResourceView* NullSRVs[6] = {};
				Context->PSSetShaderResources(0, ARRAYSIZE(NullSRVs), NullSRVs);

				if (Frame.DepthTexture && Frame.DepthCopyTexture)
				{
					Context->OMSetRenderTargets(0, nullptr, nullptr);
					Context->CopyResource(Frame.DepthCopyTexture, Frame.DepthTexture);
				}

				ID3D11ShaderResourceView* BaseSRVs[3] = {
					ActiveViewSurfaceSet->GetSRV(ESurfaceSlot::BaseColor),
					ActiveViewSurfaceSet->GetSRV(ESurfaceSlot::Surface1),
					ActiveViewSurfaceSet->GetSRV(ESurfaceSlot::Surface2),
				};
				Context->PSSetShaderResources(1, ARRAYSIZE(BaseSRVs), BaseSRVs);

				if (Frame.DepthCopySRV)
				{
					ID3D11ShaderResourceView* DepthSRV = Frame.DepthCopySRV;
					Context->PSSetShaderResources(ESystemTexSlot::SceneDepth, 1, &DepthSRV);
				}

				ActiveViewSurfaceSet->ClearModifiedTargets(Context, ShadingModel);
				ActiveViewSurfaceSet->BindDecalTargets(Context, ShadingModel, Frame.ViewportDSV);

				Cache.DiffuseSRV = nullptr;
				Cache.bForceAll = true;
			}
		});

		OutPrePassEvents.push_back({ ERenderPass::Lighting, EPassCompare::Equal, true, false,
			[Context, &Frame, &Cache, ActiveViewSurfaceSet]()
			{
				ID3D11RenderTargetView* RTV = Frame.ViewportRTV;
				Context->OMSetRenderTargets(1, &RTV, Frame.ViewportDSV);

				ID3D11ShaderResourceView* SurfaceSRVs[6] = {
					ActiveViewSurfaceSet->GetSRV(ESurfaceSlot::BaseColor),
					ActiveViewSurfaceSet->GetSRV(ESurfaceSlot::Surface1),
					ActiveViewSurfaceSet->GetSRV(ESurfaceSlot::Surface2),
					ActiveViewSurfaceSet->GetSRV(ESurfaceSlot::ModifiedBaseColor),
					ActiveViewSurfaceSet->GetSRV(ESurfaceSlot::ModifiedSurface1),
					ActiveViewSurfaceSet->GetSRV(ESurfaceSlot::ModifiedSurface2),
				};
				Context->PSSetShaderResources(0, ARRAYSIZE(SurfaceSRVs), SurfaceSRVs);

				if (Frame.DepthCopySRV)
				{
					ID3D11ShaderResourceView* DepthSRV = Frame.DepthCopySRV;
					Context->PSSetShaderResources(ESystemTexSlot::SceneDepth, 1, &DepthSRV);
				}

				Cache.DiffuseSRV = nullptr;
				Cache.bForceAll = true;
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
