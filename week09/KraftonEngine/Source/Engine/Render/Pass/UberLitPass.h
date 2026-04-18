#pragma once

#include "Render/Pass/DrawCommandList.h"
#include "Render/Pass/PassRenderState.h"

class FViewModeRenderPipeline;

FDrawCommand* BuildUberLitPassCommand(
	FDrawCommandList& DrawCommandList,
	const FPassRenderState (&PassRenderStates)[(uint32)ERenderPass::MAX],
	const FViewModeRenderPipeline* ActiveViewPipeline);
