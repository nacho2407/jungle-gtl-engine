#include "RenderPipeline.h"
#include "OpaqueRenderPass.h"
#include "DecalRenderPass.h"
#include "LightRenderPass.h"
#include "FogRenderPass.h"
#include "FXAARenderPass.h"
#include "FontRenderPass.h"
#include "SubUVRenderPass.h"
#include "TranslucentRenderPass.h"
#include "SelectionMaskRenderPass.h"
#include "GridRenderPass.h"
#include "EditorRenderPass.h"
#include "DepthLessRenderPass.h"
#include "PostProcessOutlineRenderPass.h"

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

    TranslucentRenderPass = std::make_shared<FTranslucentRenderPass>();
    TranslucentRenderPass->Initialize();

    SelectionMaskRenderPass = std::make_shared<FSelectionMaskRenderPass>();
    SelectionMaskRenderPass->Initialize();

    GridRenderPass = std::make_shared<FGridRenderPass>();
    GridRenderPass->Initialize();

    EditorRenderPass = std::make_shared<FEditorRenderPass>();
    EditorRenderPass->Initialize();

    DepthLessRenderPass = std::make_shared<FDepthLessRenderPass>();
    DepthLessRenderPass->Initialize();

    PostProcessOutlineRenderPass = std::make_shared<FPostProcessOutlineRenderPass>();
    PostProcessOutlineRenderPass->Initialize();

	RenderPasses.push_back(OpaqueRenderPass);
    RenderPasses.push_back(DecalRenderPass);
    RenderPasses.push_back(LightRenderPass);
    RenderPasses.push_back(FogRenderPass);
    RenderPasses.push_back(FXAARenderPass);
    RenderPasses.push_back(FontRenderPass);
    RenderPasses.push_back(SubUVRenderPass);
    RenderPasses.push_back(TranslucentRenderPass);
    RenderPasses.push_back(SelectionMaskRenderPass);
    RenderPasses.push_back(GridRenderPass);
    RenderPasses.push_back(EditorRenderPass);
    RenderPasses.push_back(DepthLessRenderPass);
    RenderPasses.push_back(PostProcessOutlineRenderPass);

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

    if (TranslucentRenderPass)
    {
        TranslucentRenderPass->Release();
        TranslucentRenderPass.reset();
    }

    if (SelectionMaskRenderPass)
    {
        SelectionMaskRenderPass->Release();
        SelectionMaskRenderPass.reset();
    }

    if (GridRenderPass)
    {
        GridRenderPass->Release();
        GridRenderPass.reset();
    }

    if (EditorRenderPass)
    {
        EditorRenderPass->Release();
        EditorRenderPass.reset();
    }

    if (DepthLessRenderPass)
    {
        DepthLessRenderPass->Release();
        DepthLessRenderPass.reset();
    }

    if (PostProcessOutlineRenderPass)
    {
        PostProcessOutlineRenderPass->Release();
        PostProcessOutlineRenderPass.reset();
    }
}
