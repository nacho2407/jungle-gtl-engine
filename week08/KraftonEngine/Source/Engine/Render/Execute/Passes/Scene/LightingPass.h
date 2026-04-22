#pragma once
#include "Render/Execute/Passes/Base/FullscreenPassBase.h"
struct FRenderPipelineContext;
class FPrimitiveSceneProxy;
/*
    �?모드 ?�면???�어 최종 조명 결과�??�성?�는 fullscreen ?�스?�니??
*/
class FLightingPass : public FFullscreenPassBase
{
public:
    void PrepareInputs(FRenderPipelineContext& Context) override;
    void PrepareTargets(FRenderPipelineContext& Context) override;
    void BuildDrawCommands(FRenderPipelineContext& Context) override;
    // Lighting?� fullscreen ?�성 pass?�서 ?�록???�력???�용?��? ?�는??
    void BuildDrawCommands(FRenderPipelineContext& Context, const FPrimitiveSceneProxy& Proxy) override { (void)Context; (void)Proxy; }
    void SubmitDrawCommands(FRenderPipelineContext& Context) override;

private:
    ID3D11Query* DisjointQuery = nullptr;
    ID3D11Query* TimestampStartQuery = nullptr;
    ID3D11Query* TimestampEndQuery = nullptr;
    float LastGPUTimeMs = 0.0f;
    bool bQueryInitialized = false; // 💡 쿼리가 한 번이라도 생성되었는지 체크하는 플래그
};
