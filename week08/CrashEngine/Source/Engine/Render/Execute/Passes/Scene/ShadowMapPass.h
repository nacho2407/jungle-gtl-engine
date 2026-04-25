#pragma once
#include "Render/Execute/Passes/Base/MeshPassBase.h"

class FShadowMapPass : public FMeshPassBase
{
public:
    static constexpr uint32 MAX_SHADOW_MAPS = 5;

    ~FShadowMapPass() override;

    void PrepareInputs(FRenderPipelineContext& Context) override;
    void PrepareTargets(FRenderPipelineContext& Context) override;
    void BuildDrawCommands(FRenderPipelineContext& Context) override;
    void BuildDrawCommands(FRenderPipelineContext& Context, const FPrimitiveProxy& Proxy) override;
    void SubmitDrawCommands(FRenderPipelineContext& Context) override;

    ID3D11ShaderResourceView* GetShadowSRV(uint32 Index) const 
    { 
        return (Index < MAX_SHADOW_MAPS) ? ShadowResources[Index].SRV : nullptr; 
    }

private:
    void EnsureShadowMapResources(ID3D11Device* Device);

    struct FShadowResource
    {
        ID3D11Texture2D*          Texture = nullptr;
        ID3D11DepthStencilView*   DSVs[6] = {};
        ID3D11ShaderResourceView* SRV     = nullptr;
    };

    FShadowResource ShadowResources[MAX_SHADOW_MAPS];
    uint32          ShadowMapSize = 2048;
};
