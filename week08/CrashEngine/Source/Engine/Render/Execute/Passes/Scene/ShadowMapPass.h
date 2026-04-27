#pragma once
#include "Render/Execute/Passes/Base/MeshPassBase.h"

class FShadowMapPass : public FMeshPassBase
{
public:
    // TODO: 상수 위치 적당한 곳으로 옮기기
    static constexpr uint32 MAX_SHADOW_MAPS_2D = 5;
    static constexpr uint32 MAX_SHADOW_MAPS_CUBE = 5;

    ~FShadowMapPass() override;

    void PrepareInputs(FRenderPipelineContext& Context) override;
    void PrepareTargets(FRenderPipelineContext& Context) override;
    void BuildDrawCommands(FRenderPipelineContext& Context) override;
    void BuildDrawCommands(FRenderPipelineContext& Context, const FPrimitiveProxy& Proxy) override;
    void SubmitDrawCommands(FRenderPipelineContext& Context) override;

    ID3D11ShaderResourceView* GetShadowSRV2D(uint32 Index) const 
    { 
        return (Index < MAX_SHADOW_MAPS_2D) ? ShadowResources2D[Index].SRV2D : nullptr; 
    }

    ID3D11ShaderResourceView* GetShadowSRVCube(uint32 Index) const 
    { 
        return (Index < MAX_SHADOW_MAPS_CUBE) ? ShadowResourcesCube[Index].SRVCube : nullptr; 
    }

private:
    void EnsureShadowMapResources(ID3D11Device* Device);

    struct FShadowResource2D
    {
        ID3D11Texture2D*          Texture2D   = nullptr;
        ID3D11DepthStencilView*   DSV2D       = nullptr;
        ID3D11ShaderResourceView* SRV2D       = nullptr;
    };
    FShadowResource2D ShadowResources2D[MAX_SHADOW_MAPS_2D];
    struct FShadowResourceCube
    {
        ID3D11Texture2D*          TextureCube = nullptr;
        ID3D11DepthStencilView*   DSVCubes[6] = {};
        ID3D11ShaderResourceView* SRVCube     = nullptr;
    };
    FShadowResourceCube ShadowResourcesCube[MAX_SHADOW_MAPS_CUBE];
    
    uint32          ShadowMapSize = 2048;
};
