#pragma once

#include "Render/Core/DrawCommandList.h"
#include "Render/Core/PassRenderState.h"
#include "Render/Types/ViewTypes.h"

class FViewModePassRegistry;

FDrawCommand* BuildUberLitPassCommand(
	FDrawCommandList& DrawCommandList,
	const FPassRenderState (&PassRenderStates)[(uint32)ERenderPass::MAX],
	const FViewModePassRegistry* ViewModePassRegistry,
	EViewMode ActiveViewMode);
