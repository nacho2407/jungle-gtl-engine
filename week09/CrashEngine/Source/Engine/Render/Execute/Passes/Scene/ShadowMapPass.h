#pragma once

#include "Render/Execute/Passes/Base/MeshPassBase.h"
#include "Render/Submission/Atlas/ShadowAtlasSystem.h"

class FLightProxy;
struct FSceneView;

class FShadowMapPass : public FMeshPassBase
{
public:
    ~FShadowMapPass() override;

    void PrepareInputs(FRenderPipelineContext& Context) override;
    void PrepareTargets(FRenderPipelineContext& Context) override;
    void BuildDrawCommands(FRenderPipelineContext& Context) override;
    void BuildDrawCommands(FRenderPipelineContext& Context, const FPrimitiveProxy& Proxy) override;
    void SubmitDrawCommands(FRenderPipelineContext& Context) override;

    bool UpdateLightShadowAllocation(FLightProxy& Light, ID3D11Device* Device);
    void ReleaseShadowAtlasResources();

    ID3D11ShaderResourceView* GetShadowAtlasSRV(uint32 PageIndex) const;
    ID3D11ShaderResourceView* GetShadowMomentSRV(uint32 PageIndex) const;
    ID3D11ShaderResourceView* GetShadowPreviewSRV(const FShadowMapData& ShadowMapData) const;
    ID3D11ShaderResourceView* GetShadowDebugPreviewSRV(
        const FShadowMapData& ShadowMapData,
        const FMatrix&        ViewProj,
        ID3D11Device*         Device,
        ID3D11DeviceContext*  DeviceContext);
    ID3D11ShaderResourceView* GetShadowPageSlicePreviewSRV(uint32 PageIndex, uint32 SliceIndex) const;
    void                      GetShadowPageSliceAllocations(uint32 PageIndex, uint32 SliceIndex, TArray<FShadowMapData>& OutAllocations) const;
    uint32                    GetShadowAtlasPageCount() const;
    uint32                    GetShadowAtlasSize() const { return ShadowAtlas::AtlasSize; }

private:
    struct FShadowRenderItem
    {
        FLightProxy*          Light      = nullptr;
        const FShadowMapData* Allocation = nullptr;
        FMatrix               ViewProj   = FMatrix::Identity;
    };

    void EnsureMomentBlurResources(ID3D11Device* Device);
    void EnsureDebugPreviewResources(ID3D11Device* Device);
    void ReleaseMomentBlurResources();
    void ReleaseDebugPreviewResources();
    void BlurMomentTextureSlice(FRenderPipelineContext& Context, FShadowAtlas& AtlasPage, uint32 SliceIndex);
    bool HasPSMCameraChanged(const FSceneView& SceneView);

private:
    FShadowAtlasManager       AtlasManager;
    FShadowAtlasRegistry      ShadowRegistry;
    TArray<FShadowRenderItem> RenderItems;

    FVector LastPSMCameraPosition     = FVector::ZeroVector;
    FVector LastPSMCameraForward      = FVector::ZeroVector;
    FVector LastPSMCameraUp           = FVector::ZeroVector;
    bool    bHasLastPSMCamera         = false;
    bool    bLoggedPSMRedrawThisFrame = false;

    ID3D11VertexShader*       MomentBlurVS           = nullptr;
    ID3D11PixelShader*        MomentBlurPSHorizontal = nullptr;
    ID3D11PixelShader*        MomentBlurPSVertical   = nullptr;
    ID3D11Buffer*             MomentBlurCB           = nullptr;
    ID3D11Texture2D*          MomentBlurTemp2D       = nullptr;
    ID3D11RenderTargetView*   MomentBlurTempRTV      = nullptr;
    ID3D11ShaderResourceView* MomentBlurTempSRV      = nullptr;
    uint32                    MomentBlurTempSize     = 0;

    ID3D11VertexShader*       DebugPreviewVS      = nullptr;
    ID3D11PixelShader*        DebugPreviewPS      = nullptr;
    ID3D11Buffer*             DebugPreviewCB      = nullptr;
    ID3D11Texture2D*          DebugPreviewTexture = nullptr;
    ID3D11RenderTargetView*   DebugPreviewRTV     = nullptr;
    ID3D11ShaderResourceView* DebugPreviewSRV     = nullptr;
    uint32                    DebugPreviewSize    = 512;
};
