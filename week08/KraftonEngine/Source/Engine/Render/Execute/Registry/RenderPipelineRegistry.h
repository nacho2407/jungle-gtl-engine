#pragma once

#include "Core/CoreTypes.h"

#include "Render/Execute/Registry/RenderPassRegistry.h"
#include "Render/Execute/Registry/RenderPipelineType.h"

enum class ERenderNodeKind
{
    Pipeline,
    Pass,
};

struct FRenderNodeRef
{
    ERenderNodeKind Kind;
    int32           TypeValue;
};

struct FRenderPipelineDesc
{
    ERenderPipelineType    Type;
    TArray<FRenderNodeRef> Children;
};

// Stores named render pipelines as ordered trees of child pipelines and pass nodes.
class FRenderPipelineRegistry
{
public:
    void Initialize();
    void Release();

    const FRenderPipelineDesc* FindPipeline(ERenderPipelineType Type) const;

private:
    TMap<int32, FRenderPipelineDesc> Pipelines;
};
