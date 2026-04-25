// 렌더 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Render/Execute/Registry/RenderPassTypes.h"
#include "Render/Execute/Registry/ViewModePassRegistry.h"

class FPrimitiveProxy;
class FTextRenderSceneProxy;
class FDrawCommandList;
struct FRenderPipelineContext;

namespace DrawCommandBuild
{
void BuildMeshDrawCommand(const FPrimitiveProxy& Proxy, ERenderPass Pass, FRenderPipelineContext& Context, FDrawCommandList& OutList, uint16 UserBits = 0);

void BuildFullscreenDrawCommand(ERenderPass Pass, FRenderPipelineContext& Context, FDrawCommandList& OutList, EViewModePostProcessVariant PostProcessVariant = EViewModePostProcessVariant::None);

void BuildLineDrawCommand(FRenderPipelineContext& Context, FDrawCommandList& OutList);

void BuildOverlayBillboardDrawCommand(FRenderPipelineContext& Context, FDrawCommandList& OutList);

void BuildOverlayTextDrawCommand(FRenderPipelineContext& Context, FDrawCommandList& OutList);

void BuildWorldTextDrawCommand(const FTextRenderSceneProxy& Proxy, FRenderPipelineContext& Context, FDrawCommandList& OutList);
void BuildOverlayWorldTextDrawCommand(const FTextRenderSceneProxy& Proxy, FRenderPipelineContext& Context, FDrawCommandList& OutList);

void BuildDecalDrawCommand(const FPrimitiveProxy& Proxy, FRenderPipelineContext& Context, FDrawCommandList& OutList);
} // namespace DrawCommandBuild

