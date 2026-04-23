#pragma once
#include "Render/Execute/Passes/Base/FullscreenPassBase.h"
#include <wrl/client.h>
struct FRenderPipelineContext;
class FPrimitiveSceneProxy;
class FLightingPass : public FFullscreenPassBase
{
public:
    void PrepareInputs(FRenderPipelineContext& Context) override;
    void PrepareTargets(FRenderPipelineContext& Context) override;
    void BuildDrawCommands(FRenderPipelineContext& Context) override;
    void BuildDrawCommands(FRenderPipelineContext& Context, const FPrimitiveSceneProxy& Proxy) override
    {
        (void)Context;
        (void)Proxy;
    }
    void SubmitDrawCommands(FRenderPipelineContext& Context) override;

protected:
    bool IsEnabled(const FRenderPipelineContext& Context) const override;

private:
    Microsoft::WRL::ComPtr<ID3D11Query> DisjointQuery       = nullptr;
    Microsoft::WRL::ComPtr<ID3D11Query> TimestampStartQuery = nullptr;
    Microsoft::WRL::ComPtr<ID3D11Query> TimestampEndQuery   = nullptr;
    float                               LastGPUTimeMs       = 0.0f;
    bool                                bQueryInitialized   = false; // 💡 쿼리가 한 번이라도 생성되었는지 체크하는 플래그

    // 💡 연산 횟수 측정용 자원들 추가
    Microsoft::WRL::ComPtr<ID3D11Buffer>              EvalCounterBuffer;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> EvalCounterUAV;
    Microsoft::WRL::ComPtr<ID3D11Buffer>              EvalStagingBuffer; // CPU 읽기용
};
