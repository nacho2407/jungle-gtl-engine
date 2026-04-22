#pragma once

#include "Core/CoreTypes.h"

#include "Render/Execute/Registry/RenderPassRegistry.h"
#include "Render/Execute/Registry/RenderPipelineType.h"

/*
    ?�이?�라??그래?�에???�식 ?�드가 ???�른 ?�이?�라?�인지, ?�일 ?�스?��? 구분?�니??
*/
enum class ERenderNodeKind
{
    Pipeline,
    Pass,
};

/*
    ?�이?�라??그래?�의 ?�식 ?�드 1개�? ?�현?�니??
*/
struct FRenderNodeRef
{
    ERenderNodeKind Kind;
    int32 TypeValue;
};

/*
    ?�나???�더 ?�이?�라?�과 �??�식 ?�드 목록???�의?�니??
*/
struct FRenderPipelineDesc
{
    ERenderPipelineType Type;
    TArray<FRenderNodeRef> Children;
};

/*
    루트/?�브 ?�이?�라??구성???�록???�는 ?��??�트리입?�다.
    Renderer?????��??�트리�? ?�해 Scene, OverlayPipeline 같�? ?�이?�라???�서�?조회?�니??
*/
class FRenderPipelineRegistry
{
public:
    void Initialize();
    void Release();

    const FRenderPipelineDesc* FindPipeline(ERenderPipelineType Type) const;

private:
    TMap<int32, FRenderPipelineDesc> Pipelines;
};
