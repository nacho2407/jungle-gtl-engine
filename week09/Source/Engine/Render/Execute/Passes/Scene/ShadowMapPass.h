#pragma once

#include "Render/Execute/Passes/Base/MeshPassBase.h"
#include "Render/Submission/Atlas/LightShadowAllocationRegistry.h"
#include "Render/Submission/Atlas/ShadowMomentFilter.h"

class FLightProxy;
struct FSceneView;

enum class EShadowDepthPreviewMode : uint32
{
    RawDepth,
    LinearizedDepth,
    Moments,
};

class FShadowMapPass : public FMeshPassBase
{
public:
    ~FShadowMapPass() override;

    void PrepareInputs(FRenderPipelineContext& Context) override;
    void PrepareTargets(FRenderPipelineContext& Context) override;
    void BuildDrawCommands(FRenderPipelineContext& Context) override;
    void BuildDrawCommands(FRenderPipelineContext& Context, const FPrimitiveProxy& Proxy) override;
    void SubmitDrawCommands(FRenderPipelineContext& Context) override;

    void BeginShadowFrame();
    void EndShadowFrame();
    bool UpdateLightShadowAllocation(FLightProxy& Light, ID3D11Device* Device);
    void ReleaseShadowAtlasResources();

    ID3D11ShaderResourceView* GetShadowAtlasSRV(uint32 PageIndex) const;
    ID3D11ShaderResourceView* GetShadowMomentSRV(uint32 PageIndex) const;
    ID3D11ShaderResourceView* GetShadowPreviewSRV(const FShadowMapData& ShadowMapData) const;
    ID3D11ShaderResourceView* GetShadowMomentPreviewSRV(const FShadowMapData& ShadowMapData) const;
    ID3D11ShaderResourceView* GetShadowPageSliceMomentPreviewSRV(uint32 PageIndex, uint32 SliceIndex) const;
    ID3D11ShaderResourceView* GetShadowDebugPreviewSRV(
        const FShadowMapData& ShadowMapData,
        const FMatrix&        ViewProj,
        EShadowDepthPreviewMode ShadowDepthPreviewMode,
        float                ShadowESMExponent,
        ID3D11Device*         Device,
        ID3D11DeviceContext*  DeviceContext);
    ID3D11ShaderResourceView* GetShadowPageSlicePreviewSRV(uint32 PageIndex, uint32 SliceIndex) const;
    void                      GetShadowPageSliceAllocations(uint32 PageIndex, uint32 SliceIndex, TArray<FShadowMapData>& OutAllocations) const;
    uint32                    GetShadowAtlasPageCount() const;
    uint32                    GetShadowAtlasSize() const { return ShadowAtlas::AtlasSize; }
    FShadowAtlasBudgetStats   GetShadowAtlasBudgetStats() const { return ShadowAllocationMap.GetBudgetStats(AtlasPool); }

private:
    struct FShadowRenderItem
    {
        FLightProxy*          Light      = nullptr;
        const FShadowMapData* Allocation = nullptr;
        FShadowViewData       ShadowView = {};
    };

    struct FPSMCameraState
    {
        FVector LastPosition = FVector::ZeroVector;
        FVector LastForward  = FVector::ZeroVector;
        FVector LastUp       = FVector::ZeroVector;
        bool    bHasLastCamera = false;
        bool    bLoggedRedrawThisFrame = false;
    };

    struct FDebugPreviewResources
    {
        static constexpr uint32 PoolSize = 64;

        struct FSlot
        {
            ID3D11Texture2D*          Texture = nullptr;
            ID3D11RenderTargetView*   RTV     = nullptr;
            ID3D11ShaderResourceView* SRV     = nullptr;
        };

        ID3D11VertexShader*       VS      = nullptr;
        ID3D11PixelShader*        PS      = nullptr;
        ID3D11Buffer*             CB      = nullptr;
        uint32                    Size    = 512;
        uint32                    NextSlot = 0;
        FSlot                     Slots[PoolSize] = {};
    };

    void EnsureDebugPreviewResources(ID3D11Device* Device);
    void ReleaseDebugPreviewResources();
    bool HasPSMCameraChanged(const FSceneView& SceneView);

    FShadowAtlasPool               AtlasPool;
    FLightShadowAllocationRegistry ShadowAllocationMap;
    FShadowMomentFilter            MomentFilter;
    TArray<FShadowRenderItem>      RenderItems;
    FPSMCameraState                PSMCameraState;
    FDebugPreviewResources         DebugPreview;
};
