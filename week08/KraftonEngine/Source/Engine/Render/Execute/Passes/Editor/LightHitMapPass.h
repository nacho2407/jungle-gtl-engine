#pragma once
#include "Render/Execute/Passes/Base/PostProcessPassBase.h"

struct FRenderPipelineContext;
class FPrimitiveSceneProxy;

/*
	����Ʈ ��Ʈ���� �����ϴ� fullscreen ��ó�� �н��Դϴ�.
	����Ʈ ��Ʈ���� ���� �� �ȼ��� ���� �ش� �ȼ��� ������ �ִ� ����Ʈ�� ������ ���ڵ��� �ؽ�ó�Դϴ�.
	�� �н��� ����Ʈ ��Ʈ���� �����Ͽ� ����� �������� Ȱ���ϰų�, Ư�� ������ ȿ���� �����ϴ� �� ����� �� �ֽ��ϴ�.
*/

class FLightHitMapPass : public FPostProcessPassBase
{
public:
    void PrepareInputs(FRenderPipelineContext& Context) override;
    void BuildDrawCommands(FRenderPipelineContext& Context) override;
    // Outline�� fullscreen post-process�� ���Ͻ� �Է��� ������� �ʴ´�.
    void BuildDrawCommands(FRenderPipelineContext& Context, const FPrimitiveSceneProxy& Proxy) override
    {
        (void)Context;
        (void)Proxy;
    }
    void SubmitDrawCommands(FRenderPipelineContext& Context) override;
};
