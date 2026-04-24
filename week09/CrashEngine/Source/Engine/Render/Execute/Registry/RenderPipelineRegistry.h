// 렌더 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Core/CoreTypes.h"

#include "Render/Execute/Registry/RenderPassRegistry.h"
#include "Render/Execute/Registry/RenderPipelineType.h"

// ERenderNodeKind는 렌더 처리에서 사용할 선택지를 정의합니다.
enum class ERenderNodeKind
{
    Pipeline,
    Pass,
};

// FRenderNodeRef는 렌더 처리에 필요한 데이터를 묶는 구조체입니다.
struct FRenderNodeRef
{
    ERenderNodeKind Kind;
    int32           TypeValue;
};

// FRenderPipelineDesc는 렌더 처리에 필요한 데이터를 묶는 구조체입니다.
struct FRenderPipelineDesc
{
    ERenderPipelineType    Type;
    TArray<FRenderNodeRef> Children;
};

// FRenderPipelineRegistry는 실행 시 필요한 타입과 규칙의 매핑을 보관합니다.
class FRenderPipelineRegistry
{
public:
    void Initialize();
    void Release();

    const FRenderPipelineDesc* FindPipeline(ERenderPipelineType Type) const;

private:
    TMap<int32, FRenderPipelineDesc> Pipelines;
};
