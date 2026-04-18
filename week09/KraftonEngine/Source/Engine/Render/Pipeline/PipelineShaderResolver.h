#pragma once

#include "Render/Types/RenderTypes.h"
#include "Render/Pipeline/ViewModeRenderPipeline.h"

class FShader;
class FViewModeRenderPipeline;
struct FRenderPipelinePassDesc;

inline FShader* ResolvePipelineShader(const FViewModeRenderPipeline* ActiveViewPipeline, ERenderPass Pass, FShader* FallbackShader)
{
	if (!ActiveViewPipeline)
	{
		return FallbackShader;
	}

	EPipelineStage Stage = EPipelineStage::BaseDraw;
	switch (Pass)
	{
	case ERenderPass::Opaque:
		Stage = EPipelineStage::BaseDraw;
		break;
	case ERenderPass::Decal:
		Stage = EPipelineStage::Decal;
		break;
	case ERenderPass::Lighting:
		Stage = EPipelineStage::Lighting;
		break;
	default:
		return FallbackShader;
	}

	if (const FRenderPipelinePassDesc* PassDesc = ActiveViewPipeline->FindPass(Stage))
	{
		if (PassDesc->CompiledShader)
		{
			return PassDesc->CompiledShader;
		}
	}

	return FallbackShader;
}
