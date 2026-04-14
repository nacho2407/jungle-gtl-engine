#include "RenderPipeline.h"
#include "OpaqueRenderPass.h"
#include "DecalRenderPass.h"
#include "LightRenderPass.h"
#include "FogRenderPass.h"
#include "FXAARenderPass.h"
#include "FontRenderPass.h"
#include "SubUVRenderPass.h"

bool FRenderPipeline::Initialize()
{
    OpaqueRenderPass = std::make_shared<FOpaqueRenderPass>();
    OpaqueRenderPass->Initialize();

    DecalRenderPass = std::make_shared<FDecalRenderPass>();
    DecalRenderPass->Initialize();

	LightRenderPass = std::make_shared<FLightRenderPass>();
    LightRenderPass->Initialize();

	FogRenderPass = std::make_shared<FFogRenderPass>();
    FogRenderPass->Initialize();

	FXAARenderPass = std::make_shared<FFXAARenderPass>();
    FXAARenderPass->Initialize();

    FontRenderPass = std::make_shared<FFontRenderPass>();
    FontRenderPass->Initialize();

    SubUVRenderPass = std::make_shared<FSubUVRenderPass>();
    SubUVRenderPass->Initialize();

	RenderPasses.push_back(OpaqueRenderPass);
    RenderPasses.push_back(DecalRenderPass);
    RenderPasses.push_back(LightRenderPass);
    RenderPasses.push_back(FontRenderPass);
    RenderPasses.push_back(SubUVRenderPass);
    // RenderPasses.push_back(FogRenderPass);
    // RenderPasses.push_back(FXAARenderPass);

    return true;
}

bool FRenderPipeline::Render(const FRenderPassContext* Context)
{
	for (std::shared_ptr<FBaseRenderPass> Pass : RenderPasses)
	{
        Pass->SetPrevPassSRV(OutSRV);
        Pass->SetPrevPassRTV(OutRTV);
        Pass->Render(Context);
        OutSRV = Pass->GetOutSRV();
        OutRTV = Pass->GetOutRTV();
	}

    return true;
}

void FRenderPipeline::Release()
{
	if (OpaqueRenderPass)
    {
        OpaqueRenderPass->Release();
        OpaqueRenderPass.reset();
	}

    if (DecalRenderPass)
    {
        DecalRenderPass->Release();
        DecalRenderPass.reset();
    }

	if (LightRenderPass)
	{
        LightRenderPass->Release();
        LightRenderPass.reset();
	}

	if (FogRenderPass)
	{
        FogRenderPass->Release();
        FogRenderPass.reset();
	}

	if (FXAARenderPass)
	{
        FXAARenderPass->Release();
        FXAARenderPass.reset();
	}

    if (FontRenderPass)
    {
        FontRenderPass->Release();
        FontRenderPass.reset();
    }

    if (SubUVRenderPass)
    {
        SubUVRenderPass->Release();
        SubUVRenderPass.reset();
    }
}
