#pragma once
#include "Render/Execute/Passes/Base/PostProcessPassBase.h"
struct FRenderPipelineContext;
class FPrimitiveSceneProxy;
/*
    ���� ȭ�鿡 FXAA�� �����ϴ� fullscreen ��ó�� �н��Դϴ�.
*/
class FFXAAPass : public FPostProcessPassBase
{
public:
    void PrepareInputs(FRenderPipelineContext& Context) override;
    void BuildDrawCommands(FRenderPipelineContext& Context) override;
    // FXAA�� fullscreen pass�̹Ƿ� ���Ͻ� �Է��� ������� �ʴ´�.
    void BuildDrawCommands(FRenderPipelineContext& Context, const FPrimitiveSceneProxy& Proxy) override { (void)Context; (void)Proxy; }
    void SubmitDrawCommands(FRenderPipelineContext& Context) override;
};
