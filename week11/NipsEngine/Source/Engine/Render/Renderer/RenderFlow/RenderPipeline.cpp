#include "RenderPipeline.h"
#include "DepthPrePass.h"
#include "ShadowPass.h"
#include "LightCullingPass.h"
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
#include "VSMConversionRenderPass.h"
#include "SandervistanRenderPass.h"
#include "PostProcessRenderPass.h"
#include "Core/Logging/GPUProfiler.h"

#include <cstddef>

namespace
{
    const char* GetRenderPassPerfName(size_t Index)
    {
        static constexpr const char* Names[] =
        {
            "RenderPass.DepthPre",
            "RenderPass.LightCulling",
            "RenderPass.Shadow",
            "RenderPass.VSMConversion",
            "RenderPass.Opaque",
            "RenderPass.Light",
            "RenderPass.Fog",
            "RenderPass.Sandervistan",
            "RenderPass.FXAA",
            "RenderPass.Font",
            "RenderPass.SubUV",
            "RenderPass.Translucent",
            "RenderPass.SelectionMask",
            "RenderPass.Grid",
            "RenderPass.Editor",
            "RenderPass.DepthLess",
            "RenderPass.PostProcessOutline",
        };

        return Index < (sizeof(Names) / sizeof(Names[0])) ? Names[Index] : "RenderPass.Unknown";
    }
}

bool FRenderPipeline::Initialize()
{
    DepthPrePass = std::make_shared<FDepthPrePass>();
    DepthPrePass->Initialize();

	LightCullingPass = std::make_shared<FLightCullingPass>();
	LightCullingPass->Initialize();

	ShadowPass = std::make_shared<FShadowPass>();
	ShadowPass->Initialize();

    OpaqueRenderPass = std::make_shared<FOpaqueRenderPass>();
    OpaqueRenderPass->Initialize();

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

	VSMConversionRenderPass = std::make_shared<FVSMConversionRenderPass>();
    VSMConversionRenderPass->Initialize();

	SandevistanRenderPass = std::make_shared<FSandevistanRenderPass>();
    SandevistanRenderPass->Initialize();

	PostProcessRenderPass = std::make_shared<FPostProcessRenderPass>();
    PostProcessRenderPass->Initialize();

	LightRenderPass->SetSkipWireframe(true);
    FogRenderPass->SetSkipWireframe(true);
    FXAARenderPass->SetSkipWireframe(true);

	/**
	 * 하나의 Render Pass 가 다음 Rneder Pass 에 넘기는 OutSRV 에 대해선 주의가 필요하다.
	 * LightRenderPass -> FogRenderPass 로 갈 때 LightSRV 를 넘겨도 되지만
	 * FXAARenderPass -> FontRenderPass 일 때 FXAASRV 가 아닌 ColorSRV 를 넘겨야 한다.
	 * ColorSRV 가 최종 결과물 버퍼라고 생각하면 된다.
	 */
	RenderPasses.push_back(DepthPrePass);
	RenderPasses.push_back(LightCullingPass);
	RenderPasses.push_back(ShadowPass);
    RenderPasses.push_back(VSMConversionRenderPass); // VSM 추가
	RenderPasses.push_back(OpaqueRenderPass);
    RenderPasses.push_back(LightRenderPass);

    RenderPasses.push_back(FogRenderPass);
    RenderPasses.push_back(SandevistanRenderPass);
    RenderPasses.push_back(FXAARenderPass);   
	RenderPasses.push_back(FontRenderPass);
    RenderPasses.push_back(SubUVRenderPass);
    RenderPasses.push_back(TranslucentRenderPass);
    RenderPasses.push_back(SelectionMaskRenderPass);
    RenderPasses.push_back(GridRenderPass);
    RenderPasses.push_back(EditorRenderPass);
    RenderPasses.push_back(DepthLessRenderPass);
    RenderPasses.push_back(PostProcessOutlineRenderPass);
    // RenderPasses.push_back(PostProcessRenderPass);

    return true;
}

bool FRenderPipeline::Render(const FRenderPassContext* Context)
{
    OutSRV = nullptr;
    OutRTV = nullptr;

    for (size_t PassIndex = 0; PassIndex < RenderPasses.size(); ++PassIndex)
	{
        std::shared_ptr<FBaseRenderPass> Pass = RenderPasses[PassIndex];
        Pass->SetPrevPassSRV(OutSRV);
        Pass->SetPrevPassRTV(OutRTV);
        {
#if STATS
            FGPUScopedTimer PassTimer(GetRenderPassPerfName(PassIndex));
#endif
            Pass->Render(Context);
        }
        OutSRV = Pass->GetOutSRV();
        OutRTV = Pass->GetOutRTV();
	}

    return true;
}

void FRenderPipeline::Release()
{
	if (DepthPrePass) {
		DepthPrePass->Release();
		DepthPrePass.reset();
	}

	if (LightCullingPass) {
		LightCullingPass->Release();
		LightCullingPass.reset();
	}

	if (ShadowPass) {
		ShadowPass->Release();
		ShadowPass.reset();
	}

	if (OpaqueRenderPass)
    {
        OpaqueRenderPass->Release();
        OpaqueRenderPass.reset();
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

	if (VSMConversionRenderPass)
	{
        VSMConversionRenderPass->Release();
        VSMConversionRenderPass.reset();
	}

	if (PostProcessRenderPass)
	{
        PostProcessRenderPass->Release();
        PostProcessRenderPass.reset();
	}
}
