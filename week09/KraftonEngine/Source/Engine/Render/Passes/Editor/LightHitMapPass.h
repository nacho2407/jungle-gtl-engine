#pragma once
#include "Render/Passes/Base/PostProcessPassBase.h"

struct FRenderPipelineContext;
class FPrimitiveSceneProxy;

/*
	라이트 히트맵을 생성하는 fullscreen 후처리 패스입니다.
	라이트 히트맵은 씬의 각 픽셀에 대해 해당 픽셀에 영향을 주는 라이트의 정보를 인코딩한 텍스처입니다.
	이 패스는 라이트 히트맵을 생성하여 디버깅 목적으로 활용하거나, 특정 라이팅 효과를 구현하는 데 사용할 수 있습니다.
*/

class FLightHitMapPass : public FPostProcessPassBase
{
public:
    void PrepareInputs(FRenderPipelineContext& Context) override;
    void BuildDrawCommands(FRenderPipelineContext& Context) override;
    // Outline은 fullscreen post-process라서 프록시 입력을 사용하지 않는다.
    void BuildDrawCommands(FRenderPipelineContext& Context, const FPrimitiveSceneProxy& Proxy) override
    {
        (void)Context;
        (void)Proxy;
    }
    void SubmitDrawCommands(FRenderPipelineContext& Context) override;
};
