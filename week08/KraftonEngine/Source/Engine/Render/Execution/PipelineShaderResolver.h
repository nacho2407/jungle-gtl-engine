#pragma once

#include "Render/Types/RenderTypes.h"
#include "Render/Core/PassTypes.h"

class FShader;
class FViewModePassRegistry;
struct FRenderPipelinePassDesc;

inline FShader* ResolvePipelineShader(const FViewModePassRegistry* ViewModePassRegistry, EViewMode ViewMode, ERenderPass Pass, FShader* FallbackShader)
{
	if (!ViewModePassRegistry)
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

	if (const FRenderPipelinePassDesc* PassDesc = ViewModePassRegistry->FindPassDesc(ViewMode, Stage))
	{
		if (PassDesc->CompiledShader)
		{
			return PassDesc->CompiledShader;
		}
	}

	return FallbackShader;
}
