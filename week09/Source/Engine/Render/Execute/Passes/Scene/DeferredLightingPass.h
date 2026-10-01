#pragma once

#include "Render/Execute/Passes/Base/FullscreenPassBase.h"
#include <wrl/client.h>

struct FRenderPipelineContext;
class FPrimitiveProxy;

/*
    Pass Summary
    - Role: read deferred surfaces and compose lit scene color.
    - Inputs: view-mode surfaces, depth copy, global light CB, local lights, tile mask/debug hit map.
    - Outputs: viewport color and optional GPU timing/evaluation stats.
    - Registers: PS t0-t5 surfaces, PS t6 LocalLights, PS t7 TileMask, PS t8 DebugHitMap, PS t10 SceneDepth,
      PS b2 LightCullingParams, PS/VS b4 GlobalLight.
*/
class FDeferredLightingPass : public FFullscreenPassBase
{
public:
    void PrepareInputs(FRenderPipelineContext& Context) override;
    void PrepareTargets(FRenderPipelineContext& Context) override;
    void BuildDrawCommands(FRenderPipelineContext& Context) override;
    void BuildDrawCommands(FRenderPipelineContext& Context, const FPrimitiveProxy& Proxy) override
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
    bool                                bQueryInitialized   = false; // ?? 쿼리가 한 번이라도 생성되었는지 체크하는 플래그

    // 연산 횟수 측정용 자원들 추가
    Microsoft::WRL::ComPtr<ID3D11Buffer>              EvalCounterBuffer;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> EvalCounterUAV;
    Microsoft::WRL::ComPtr<ID3D11Buffer>              EvalStagingBuffer; // CPU 읽기용
};
