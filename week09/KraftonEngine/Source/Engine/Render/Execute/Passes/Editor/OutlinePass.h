#pragma once
#include "Render/Execute/Passes/Base/PostProcessPassBase.h"
struct FRenderPipelineContext;
class FPrimitiveSceneProxy;
/*
    ���� ����ũ�� �о� �ܰ����� �ռ��ϴ� ��ó�� �н��Դϴ�.
*/
class FOutlinePass : public FPostProcessPassBase
{
public:
    void PrepareInputs(FRenderPipelineContext& Context) override;
    void BuildDrawCommands(FRenderPipelineContext& Context) override;
    // Outline�� fullscreen post-process�� ���Ͻ� �Է��� ������� �ʴ´�.
    void BuildDrawCommands(FRenderPipelineContext& Context, const FPrimitiveSceneProxy& Proxy) override { (void)Context; (void)Proxy; }
    void SubmitDrawCommands(FRenderPipelineContext& Context) override;
};
