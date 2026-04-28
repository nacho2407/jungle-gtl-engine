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
    ID3D11ShaderResourceView* GetMomentSRV2D(uint32 Index) const
    {
        return (Index < ESystemTexSlot::MaxShadowMaps2DCount) ? ShadowResources2D[Index].MomentSRV2D : nullptr;
    }
    ID3D11ShaderResourceView* GetMomentSRVCube(uint32 Index) const
    {
        return (Index < ESystemTexSlot::MaxShadowMapsCubeCount) ? ShadowResourcesCube[Index].MomentSRVCube : nullptr;
    }
    ID3D11ShaderResourceView* GetShadowPreviewSRV(uint32 Index, bool bIsCube, uint32 Face, ID3D11DeviceContext* Context);
    uint32 GetShadowMapSize() const { return ShadowMapSize; }
    void SetShadowMapSize(uint32 InShadowMapSize);

private:
    void EnsureShadowMapResources(ID3D11Device* Device);
    void ReleaseShadowMapResources();
    void EnsureMomentBlurResources(ID3D11Device* Device);
    void ReleaseMomentBlurResources();

    struct FShadowResource2D
    {
        // 일반적인 Shadow Map용
        ID3D11Texture2D*          Texture2D   = nullptr;
        ID3D11DepthStencilView*   DSV2D       = nullptr;
        ID3D11ShaderResourceView* SRV2D       = nullptr;
        ID3D11ShaderResourceView* PreviewSRV  = nullptr;

		// VSM용
		ID3D11Texture2D*          MomentTexture2D = nullptr;
        ID3D11RenderTargetView*   MomentRTV2D     = nullptr;
        ID3D11ShaderResourceView* MomentSRV2D     = nullptr;
    };
    FShadowResource2D ShadowResources2D[ESystemTexSlot::MaxShadowMaps2DCount];
    struct FShadowResourceCube
    {
        // 일반적인 Shadow Map용
        ID3D11Texture2D*          TextureCube = nullptr;
        ID3D11DepthStencilView*   DSVCubes[6] = {};
        ID3D11ShaderResourceView* SRVCube     = nullptr;
        ID3D11ShaderResourceView* PreviewSRVs[6] = {};

		// VSM용
		ID3D11Texture2D*          MomentTextureCube = nullptr;
        ID3D11RenderTargetView*   MomentRTVCubes[6] = {};
        ID3D11ShaderResourceView* MomentSRVCube     = nullptr;
        ID3D11ShaderResourceView* MomentFaceSRVs[6] = {};
    };
    FShadowResourceCube ShadowResourcesCube[ESystemTexSlot::MaxShadowMapsCubeCount];

    void BlurMomentTexture2D(FRenderPipelineContext& Context, FShadowResource2D& Resource);
    void BlurMomentTextureCube(FRenderPipelineContext& Context, FShadowResourceCube& Resource);

    ID3D11VertexShader*        MomentBlurVS           = nullptr;
    ID3D11PixelShader*         MomentBlurPSHorizontal = nullptr;
    ID3D11PixelShader*         MomentBlurPSVertical   = nullptr;
    ID3D11Buffer*              MomentBlurCB           = nullptr;
    ID3D11Texture2D*           MomentBlurTemp2D       = nullptr;
    ID3D11RenderTargetView*    MomentBlurTempRTV      = nullptr;
    ID3D11ShaderResourceView*  MomentBlurTempSRV      = nullptr;
    uint32                     MomentBlurTempSize     = 0;
    
    uint32          ShadowMapSize = 2048;
};
