#include "LightCullingPass.h"
#include "RenderPassRegistry.h"

#include "Render/Device/D3DDevice.h"
#include "Render/Types/FrameContext.h"
#include "Render/Resource/RenderResources.h"

REGISTER_RENDER_PASS(FLightCullingPass)

FLightCullingPass::FLightCullingPass()
{
	PassType = ERenderPass::LightCulling;
}

void FLightCullingPass::Execute(const FPassContext& Ctx)
{
	const FFrameContext& Frame = Ctx.Frame;
	if (Frame.RenderOptions.LightCullingMode == ELightCullingMode::Off) return;

	ID3D11DeviceContext* DC = Ctx.Device.GetDeviceContext();
	DC->OMSetRenderTargets(0, nullptr, nullptr);

	if (Frame.RenderOptions.LightCullingMode == ELightCullingMode::Tile)
	{
		Ctx.Resources.UnbindClusterCullingResources(Ctx.Device);
		Ctx.Resources.UnbindTileCullingBuffers(Ctx.Device);
		Ctx.Resources.DispatchTileCulling(DC, Frame);
		Ctx.Resources.BindTileCullingBuffers(Ctx.Device);
	}
	else if (Frame.RenderOptions.LightCullingMode == ELightCullingMode::Cluster)
	{
		Ctx.Resources.DispatchClusterCulling(Ctx.Device);
	}
}
