#include "FSceneViewport.h"
#include "Render/Renderer/RenderTarget/RenderTargetFactory.h"
#include "Render/Renderer/RenderTarget/DepthStencilFactory.h"


void FSceneViewport::Draw()
{
}

bool FSceneViewport::ContainsPoint(int32 X, int32 Y) const
{
	return false;
}

void FSceneViewport::WindowToLocal(int32 X, int32 Y, int32& OutX, int32& OutY) const
{
}

bool FSceneViewport::OnMouseMove(const FViewportMouseEvent& Ev)
{
	return false;
}

bool FSceneViewport::OnMouseButtonDown(const FViewportMouseEvent& Ev)
{
	return false;
}

bool FSceneViewport::OnMouseButtonUp(const FViewportMouseEvent& Ev)
{
	return false;
}

bool FSceneViewport::OnMouseWheel(float Delta)
{
	return false;
}

bool FSceneViewport::OnKeyDown(uint32 Key)
{
	return false;
}

bool FSceneViewport::OnKeyUp(uint32 Key)
{
	return false;
}

bool FSceneViewport::OnChar(uint32 Codepoint)
{
	return false;
}


FRenderTargetSet FSceneViewport::GetViewportRenderTargets() const
{
    FRenderTargetSet Targets;
    Targets.SceneColorRTV = ViewportSceneColorRTV.Get();
    Targets.SceneColorSRV = ViewportSceneColorSRV.Get();

    Targets.SceneNormalRTV = ViewportSceneNormalRTV.Get();
    Targets.SceneNormalSRV = ViewportSceneNormalSRV.Get();

    Targets.SceneLightRTV = ViewportSceneLightRTV.Get();
    Targets.SceneLightSRV = ViewportSceneLightSRV.Get();

    Targets.SceneFogRTV = ViewportSceneFogRTV.Get();
    Targets.SceneFogSRV = ViewportSceneFogSRV.Get();

    Targets.SceneWorldPosRTV = ViewportSceneWorldPosRTV.Get();
    Targets.SceneWorldPosSRV = ViewportSceneWorldPosSRV.Get();

    Targets.SceneFXAARTV = ViewportSceneFXAARTV.Get();
    Targets.SceneFXAASRV = ViewportSceneFXAASRV.Get();

    Targets.SceneDepthSRV = ViewportDepthStencilSRV.Get();
    Targets.SelectionMaskRTV = ViewportSelectionMaskRTV.Get();
    Targets.SelectionMaskSRV = ViewportSelectionMaskSRV.Get();
    Targets.DepthStencilView = ViewportDepthStencilView.Get();
    Targets.Width = static_cast<float>(ViewportRenderTargetWidth);
    Targets.Height = static_cast<float>(ViewportRenderTargetHeight);

    return Targets;
}

void FSceneViewport::InitializeResource(ID3D11Device* Device, uint32 Width, uint32 Height)
{
    /**
     * Texture, RTV, SRV 생성
     */
    FRenderTarget RT;
    ViewportRenderTargetWidth = Width;
    ViewportRenderTargetHeight = Height;

    RT = FRenderTargetFactory::CreateSceneColor(Device, Width, Height);

    ViewportSceneColorTexture = RT.Texture;
    ViewportSceneColorRTV = RT.RTV;
    ViewportSceneColorSRV = RT.SRV;

    RT = FRenderTargetFactory::CreateSceneNormal(Device, Width, Height);

    ViewportSceneNormalTexture = RT.Texture;
    ViewportSceneNormalRTV = RT.RTV;
    ViewportSceneNormalSRV = RT.SRV;


    RT = FRenderTargetFactory::CreateSelectionMask(Device, Width, Height);
    ViewportSelectionMaskTexture = RT.Texture;
    ViewportSelectionMaskRTV = RT.RTV;
    ViewportSelectionMaskSRV = RT.SRV;

    RT = FRenderTargetFactory::CreateSceneLight(Device, Width, Height);
    ViewportSceneLightTexture = RT.Texture;
    ViewportSceneLightRTV = RT.RTV;
    ViewportSceneLightSRV = RT.SRV;

    RT = FRenderTargetFactory::CreateSceneFog(Device, Width, Height);
    ViewportSceneFogTexture = RT.Texture;
    ViewportSceneFogRTV = RT.RTV;
    ViewportSceneFogSRV = RT.SRV;

    RT = FRenderTargetFactory::CreateSceneWorldPos(Device, Width, Height);
    ViewportSceneWorldPosTexture = RT.Texture;
    ViewportSceneWorldPosRTV = RT.RTV;
    ViewportSceneWorldPosSRV = RT.SRV;

    RT = FRenderTargetFactory::CreateSceneFXAA(Device, Width, Height);
    ViewportSceneFXAATexture = RT.Texture;
    ViewportSceneFXAARTV = RT.RTV;
    ViewportSceneFXAASRV = RT.SRV;

    /**
     * DepthStencil Texture, DSV, SRV 생성
     */

    FDepthStencilResource DSR;

    DSR = FDepthStencilFactory::CreateDepthStencilView(Device, Width, Height);

    ViewportDepthStencilTexture = DSR.Texture;
    ViewportDepthStencilView = DSR.DSV;
    ViewportDepthStencilSRV = DSR.SRV;
}

void FSceneViewport::ReleaseResource()
{
    ViewportSelectionMaskSRV.Reset();
    ViewportSelectionMaskRTV.Reset();
    ViewportSelectionMaskTexture.Reset();

    ViewportSceneColorSRV.Reset();
    ViewportSceneColorRTV.Reset();
    ViewportSceneColorTexture.Reset();

    ViewportSceneNormalRTV.Reset();
    ViewportSceneNormalSRV.Reset();
    ViewportSceneNormalTexture.Reset();

    ViewportSceneLightRTV.Reset();
    ViewportSceneLightSRV.Reset();
    ViewportSceneLightTexture.Reset();

    ViewportDepthStencilView.Reset();
    ViewportDepthStencilTexture.Reset();
    ViewportDepthStencilSRV.Reset();

    ViewportSceneFogTexture.Reset();
    ViewportSceneFogRTV.Reset();
    ViewportSceneFogSRV.Reset();

    ViewportSceneWorldPosRTV.Reset();
    ViewportSceneWorldPosSRV.Reset();
    ViewportSceneWorldPosTexture.Reset();

    ViewportSceneFXAARTV.Reset();
    ViewportSceneFXAASRV.Reset();
    ViewportSceneFXAATexture.Reset();

    ViewportRenderTargetWidth = 0;
    ViewportRenderTargetHeight = 0;
}
