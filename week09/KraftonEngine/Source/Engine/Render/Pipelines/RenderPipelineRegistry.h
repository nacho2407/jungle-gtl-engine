#pragma once

#include "Core/CoreTypes.h"
#include "Render/Pipelines/RenderPipelineType.h"
#include "Render/Pipelines/RenderPassRegistry.h"

enum class ERenderNodeKind
{
    Pipeline,
    Pass,
};

struct FRenderNodeRef
{
    ERenderNodeKind Kind = ERenderNodeKind::Pipeline;
    int32 TypeValue = 0;
};

struct FRenderPipelineDesc
{
    ERenderPipelineType Type = ERenderPipelineType::Scene;
    TArray<FRenderNodeRef> Children;
};

class FRenderPipelineRegistry
{
public:
    void Initialize();
    void Release();

    const FRenderPipelineDesc* FindPipeline(ERenderPipelineType Type) const;

private:
    TMap<int32, FRenderPipelineDesc> Pipelines;
};
