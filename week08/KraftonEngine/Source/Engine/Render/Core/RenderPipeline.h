#pragma once

#include "Core/CoreTypes.h"

enum class ERenderPipelineType
{
    DefaultScene,
    EditorScene,
    Scene,
    SceneViewMode,
    ScenePostProcess,
    EditorOverlay,
    Outline
};

struct FFrameContext;
class FRenderer;

void ExecuteRenderPipeline(FRenderer& Renderer, ERenderPipelineType Type, const FFrameContext& Frame);
