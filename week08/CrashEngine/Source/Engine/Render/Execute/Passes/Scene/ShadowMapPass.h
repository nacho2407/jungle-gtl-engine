#pragma once
#include "Render/Execute/Passes/Base/MeshPassBase.h"
#include "Render/Resources/Bindings/RenderBindingSlots.h"

class FShadowMapPass : public FMeshPassBase
{
public:
    ~FShadowMapPass() override;

    void PrepareInputs(FRenderPipelineContext& Context) override;
    void PrepareTargets(FRenderPipelineContext& Context) override;
    void BuildDrawCommands(FRenderPipelineContext& Context) override;
    void BuildDrawCommands(FRenderPipelineContext& Context, const FPrimitiveProxy& Proxy) override;
    void SubmitDrawCommands(FRenderPipelineContext& Context) override;

    ID3D11ShaderResourceView* GetShadowSRV2D(uint32 Index) const 
    { 
        return (Index < ESystemTexSlot::MaxShadowMaps2DCount) ? ShadowResources2D[Index].SRV2D : nullptr; 
    }

    ID3D11ShaderResourceView* GetShadowSRVCube(uint32 Index) const 
    { 
        return (Index < ESystemTexSlot::MaxShadowMapsCubeCount) ? ShadowResourcesCube[Index].SRVCube : nullptr; 
    }
    ID3D11ShaderResourceView* GetShadowPreviewSRV(uint32 Index, bool bIsCube, uint32 Face, ID3D11DeviceContext* Context);
    uint32 GetShadowMapSize() const { return ShadowMapSize; }
    void SetShadowMapSize(uint32 InShadowMapSize);

private:
    void EnsureShadowMapResources(ID3D11Device* Device);
    void ReleaseShadowMapResources();

    struct FShadowResource2D
    {
        ID3D11Texture2D*          Texture2D   = nullptr;
        ID3D11DepthStencilView*   DSV2D       = nullptr;
        ID3D11ShaderResourceView* SRV2D       = nullptr;
        ID3D11ShaderResourceView* PreviewSRV  = nullptr;
    };
    FShadowResource2D ShadowResources2D[ESystemTexSlot::MaxShadowMaps2DCount];
    struct FShadowResourceCube
    {
        ID3D11Texture2D*          TextureCube = nullptr;
        ID3D11DepthStencilView*   DSVCubes[6] = {};
        ID3D11ShaderResourceView* SRVCube     = nullptr;
        ID3D11ShaderResourceView* PreviewSRVs[6] = {};
    };
    FShadowResourceCube ShadowResourcesCube[ESystemTexSlot::MaxShadowMapsCubeCount];
    
    uint32          ShadowMapSize = 2048;
};
