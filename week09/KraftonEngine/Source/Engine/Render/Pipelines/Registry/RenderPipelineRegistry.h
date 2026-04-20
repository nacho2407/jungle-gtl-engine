#pragma once

#include "Core/CoreTypes.h"
#include "Render/Pipelines/RenderPipelineType.h"
#include "Render/Pipelines/Registry/RenderPassRegistry.h"

/*
    파이프라인 그래프에서 자식 노드가 또 다른 파이프라인인지, 단일 패스인지 구분합니다.
*/
enum class ERenderNodeKind
{
    Pipeline,
    Pass,
};

/*
    파이프라인 그래프의 자식 노드 1개를 표현합니다.
*/
struct FRenderNodeRef
{
    ERenderNodeKind Kind = ERenderNodeKind::Pipeline;
    int32 TypeValue = 0;
};

/*
    하나의 렌더 파이프라인과 그 자식 노드 목록을 정의합니다.
*/
struct FRenderPipelineDesc
{
    ERenderPipelineType Type = ERenderPipelineType::Scene;
    TArray<FRenderNodeRef> Children;
};

/*
    루트/서브 파이프라인 구성을 등록해 두는 레지스트리입니다.
    Renderer는 이 레지스트리를 통해 Scene, EditorOverlay 같은 파이프라인 순서를 조회합니다.
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
