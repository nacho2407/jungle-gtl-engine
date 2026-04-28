#pragma once

#include "Render/Submission/Atlas/Allocator/BuddyAllocator2D.h"
#include "Render/Submission/Atlas/AtlasPageBase.h"

// Shadow atlas page는 depth/moment texture와 slice별 view를 직접 관리합니다.
class FShadowAtlasPage : public FAtlasPageBase
{
public:
    FShadowAtlasPage()
        : FAtlasPageBase(ShadowAtlas::AtlasSize, ShadowAtlas::SliceCount)
    {
    }

    FShadowAtlasPage(const FShadowAtlasPage&) = delete;
    FShadowAtlasPage& operator=(const FShadowAtlasPage&) = delete;

    ~FShadowAtlasPage();

    bool Initialize(ID3D11Device* Device) override;
    bool EnsureMomentResources(ID3D11Device* Device);
    void Release() override;
    bool HasMomentResources() const { return MomentTexture != nullptr; }

    bool Allocate(uint32 Resolution, uint32 AtlasPageIndex, FShadowMapData& OutData);
    void Free(const FShadowMapData& Allocation);
    void GatherSliceAllocations(uint32 SliceIndex, uint32 AtlasPageIndex, TArray<FShadowMapData>& OutAllocations) const;

    ID3D11DepthStencilView*   GetSliceDSV(uint32 SliceIndex) const;
    ID3D11RenderTargetView*   GetMomentSliceRTV(uint32 SliceIndex) const;
    ID3D11ShaderResourceView* GetMomentSliceSRV(uint32 SliceIndex) const;
    ID3D11ShaderResourceView* GetPreviewSliceSRV(uint32 SliceIndex) const;
    ID3D11ShaderResourceView* GetDepthArraySRV() const { return DepthArraySRV; }
    ID3D11ShaderResourceView* GetMomentArraySRV() const { return MomentArraySRV; }

private:
    FBuddyAllocator2D SliceAllocators[ShadowAtlas::SliceCount];

    ID3D11Texture2D*          DepthTexture                              = nullptr;
    ID3D11DepthStencilView*   SliceDSVs[ShadowAtlas::SliceCount]        = {};
    ID3D11ShaderResourceView* DepthArraySRV                             = nullptr;
    ID3D11ShaderResourceView* PreviewSliceSRVs[ShadowAtlas::SliceCount] = {};

    ID3D11Texture2D*          MomentTexture                            = nullptr;
    ID3D11RenderTargetView*   MomentSliceRTVs[ShadowAtlas::SliceCount] = {};
    ID3D11ShaderResourceView* MomentArraySRV                           = nullptr;
    ID3D11ShaderResourceView* MomentSliceSRVs[ShadowAtlas::SliceCount] = {};
};

// 여러 atlas page를 보유하면서 page 생성과 해제를 중재합니다.
class FShadowAtlasPool
{
public:
    FShadowAtlasPool() = default;
    FShadowAtlasPool(const FShadowAtlasPool&) = delete;
    FShadowAtlasPool& operator=(const FShadowAtlasPool&) = delete;
    ~FShadowAtlasPool();

    void Release();
    bool Allocate(ID3D11Device* Device, uint32 Resolution, FShadowMapData& OutData);
    void Free(const FShadowMapData& Allocation);

    uint32                  GetPageCount() const { return static_cast<uint32>(Pages.size()); }
    FShadowAtlasPage*       GetPage(uint32 PageIndex);
    const FShadowAtlasPage* GetPage(uint32 PageIndex) const;

private:
    TArray<FShadowAtlasPage*> Pages;
};
