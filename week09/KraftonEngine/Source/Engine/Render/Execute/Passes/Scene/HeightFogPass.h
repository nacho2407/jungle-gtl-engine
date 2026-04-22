#pragma once
#include "Render/Execute/Passes/Base/PostProcessPassBase.h"
struct FRenderPipelineContext;
class FPrimitiveSceneProxy;
/*
    SceneDepth�� ������� ���� �Ȱ��� �ռ��ϴ� fullscreen �н��Դϴ�.
*/
class FHeightFogPass : public FPostProcessPassBase
{
public:
    void PrepareInputs(FRenderPipelineContext& Context) override;
    void BuildDrawCommands(FRenderPipelineContext& Context) override;
    // Height fog�� fullscreen pass�̹Ƿ� ���Ͻ� �Է��� ������� �ʴ´�.
    void BuildDrawCommands(FRenderPipelineContext& Context, const FPrimitiveSceneProxy& Proxy) override { (void)Context; (void)Proxy; }
    void SubmitDrawCommands(FRenderPipelineContext& Context) override;
};
