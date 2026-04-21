#pragma once

#include "Render/Passes/Base/RenderPassTypes.h"
#include "Render/Pipelines/Registry/ViewModePassRegistry.h"

class FPrimitiveSceneProxy;
class FTextRenderSceneProxy;
class FDrawCommandList;
struct FRenderPipelineContext;

namespace DrawCommandBuilder
{
    // 일반 메시 프록시를 드로우 커맨드로 변환합니다.
    void BuildMeshDrawCommand(const FPrimitiveSceneProxy& Proxy, ERenderPass Pass, FRenderPipelineContext& Context, FDrawCommandList& OutList);

    // 풀스크린 패스용 드로우 커맨드를 생성합니다.
    void BuildFullscreenDrawCommand(ERenderPass Pass, FRenderPipelineContext& Context, FDrawCommandList& OutList, EViewModePostProcessVariant PostProcessVariant = EViewModePostProcessVariant::None);

    // 디버그 라인 배치를 드로우 커맨드로 변환합니다.
    void BuildLineDrawCommand(FRenderPipelineContext& Context, FDrawCommandList& OutList);

    // 화면 오버레이 텍스트용 드로우 커맨드를 생성합니다.
    void BuildOverlayTextDrawCommand(FRenderPipelineContext& Context, FDrawCommandList& OutList);

    // 월드 텍스트 프록시를 드로우 커맨드로 변환합니다.
    void BuildWorldTextDrawCommand(const FTextRenderSceneProxy& Proxy, FRenderPipelineContext& Context, FDrawCommandList& OutList);

    // 데칼 프록시를 드로우 커맨드로 변환합니다.
    void BuildDecalDrawCommand(const FPrimitiveSceneProxy& Proxy, FRenderPipelineContext& Context, FDrawCommandList& OutList);

    // 데칼 리시버 프록시용 드로우 커맨드를 생성합니다.
    void BuildDecalReceiverDrawCommand(const FPrimitiveSceneProxy& ReceiverProxy, const FPrimitiveSceneProxy& DecalProxy, FRenderPipelineContext& Context, FDrawCommandList& OutList);
}
