#pragma once

#include "Render/Execute/Passes/Base/PostProcessPassBase.h"

class FPrimitiveSceneProxy;
struct FRenderPipelineContext;

/*
    SceneDepth/Normal ���� Ư�� �� ��� ����� ���� ȭ������ Ǯ��� �н��Դϴ�.
*/
class FNonLitViewModePass : public FPostProcessPassBase
{
public:
    void PrepareInputs(FRenderPipelineContext& Context) override;
    void BuildDrawCommands(FRenderPipelineContext& Context) override;
    // ViewMode post-process�� fullscreen path�� ����Ѵ�.
    void BuildDrawCommands(FRenderPipelineContext& Context, const FPrimitiveSceneProxy& Proxy) override { (void)Context; (void)Proxy; }
    void SubmitDrawCommands(FRenderPipelineContext& Context) override;
};
