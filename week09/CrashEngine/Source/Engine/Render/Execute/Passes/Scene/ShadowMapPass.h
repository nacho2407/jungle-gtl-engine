#pragma once
#include "Render/Execute/Passes/Base/MeshPassBase.h"

class FShadowMapPass : public FMeshPassBase
{
public:
    ~FShadowMapPass() override;

    void PrepareInputs(FRenderPipelineContext& Context) override;
    void PrepareTargets(FRenderPipelineContext& Context) override;
    void BuildDrawCommands(FRenderPipelineContext& Context) override;
    void BuildDrawCommands(FRenderPipelineContext& Context, const FPrimitiveProxy& Proxy) override;
    void SubmitDrawCommands(FRenderPipelineContext& Context) override;

private:
    void EnsureShadowMapResources(ID3D11Device* Device);

    ID3D11Texture2D*          ShadowDepthTexture = nullptr;
    ID3D11DepthStencilView*   ShadowDSVs[6]      = {};           // For each face
    ID3D11ShaderResourceView* ShadowSRV          = nullptr;

    uint32 ShadowMapSize = 2048;
};
