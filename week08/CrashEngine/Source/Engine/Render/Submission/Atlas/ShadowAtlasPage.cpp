#include "Render/Submission/Atlas/ShadowAtlasPage.h"

namespace
{
void ReleaseView(ID3D11DeviceChild*& Resource)
{
    if (Resource)
    {
        Resource->Release();
        Resource = nullptr;
    }
}
} // namespace

FShadowAtlas::~FShadowAtlas()
{
    Release();
}

bool FShadowAtlas::Initialize(ID3D11Device* Device)
{
    if (!Device)
    {
        return false;
    }

    if (DepthTexture)
    {
        return true;
    }

    D3D11_TEXTURE2D_DESC DepthDesc = {};
    DepthDesc.Width = ShadowAtlas::AtlasSize;
    DepthDesc.Height = ShadowAtlas::AtlasSize;
    DepthDesc.MipLevels = 1;
    DepthDesc.ArraySize = ShadowAtlas::SliceCount;
    DepthDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    DepthDesc.SampleDesc.Count = 1;
    DepthDesc.Usage = D3D11_USAGE_DEFAULT;
    DepthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

    if (FAILED(Device->CreateTexture2D(&DepthDesc, nullptr, &DepthTexture)))
    {
        Release();
        return false;
    }

    for (uint32 SliceIndex = 0; SliceIndex < ShadowAtlas::SliceCount; ++SliceIndex)
    {
        D3D11_DEPTH_STENCIL_VIEW_DESC DSVDesc = {};
        DSVDesc.Format = DXGI_FORMAT_D32_FLOAT;
        DSVDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
        DSVDesc.Texture2DArray.MipSlice = 0;
        DSVDesc.Texture2DArray.FirstArraySlice = SliceIndex;
        DSVDesc.Texture2DArray.ArraySize = 1;
        if (FAILED(Device->CreateDepthStencilView(DepthTexture, &DSVDesc, &SliceDSVs[SliceIndex])))
        {
            Release();
            return false;
        }

        D3D11_SHADER_RESOURCE_VIEW_DESC PreviewDesc = {};
        PreviewDesc.Format = DXGI_FORMAT_R32_FLOAT;
        PreviewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
        PreviewDesc.Texture2DArray.MostDetailedMip = 0;
        PreviewDesc.Texture2DArray.MipLevels = 1;
        PreviewDesc.Texture2DArray.FirstArraySlice = SliceIndex;
        PreviewDesc.Texture2DArray.ArraySize = 1;
        if (FAILED(Device->CreateShaderResourceView(DepthTexture, &PreviewDesc, &PreviewSliceSRVs[SliceIndex])))
        {
            Release();
            return false;
        }
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC DepthArrayDesc = {};
    DepthArrayDesc.Format = DXGI_FORMAT_R32_FLOAT;
    DepthArrayDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
    DepthArrayDesc.Texture2DArray.MostDetailedMip = 0;
    DepthArrayDesc.Texture2DArray.MipLevels = 1;
    DepthArrayDesc.Texture2DArray.FirstArraySlice = 0;
    DepthArrayDesc.Texture2DArray.ArraySize = ShadowAtlas::SliceCount;
    if (FAILED(Device->CreateShaderResourceView(DepthTexture, &DepthArrayDesc, &DepthArraySRV)))
    {
        Release();
        return false;
    }

    D3D11_TEXTURE2D_DESC MomentDesc = {};
    MomentDesc.Width = ShadowAtlas::AtlasSize;
    MomentDesc.Height = ShadowAtlas::AtlasSize;
    MomentDesc.MipLevels = 0;
    MomentDesc.ArraySize = ShadowAtlas::SliceCount;
    MomentDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
    MomentDesc.SampleDesc.Count = 1;
    MomentDesc.Usage = D3D11_USAGE_DEFAULT;
    MomentDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    MomentDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

    if (FAILED(Device->CreateTexture2D(&MomentDesc, nullptr, &MomentTexture)))
    {
        Release();
        return false;
    }

    for (uint32 SliceIndex = 0; SliceIndex < ShadowAtlas::SliceCount; ++SliceIndex)
    {
        D3D11_RENDER_TARGET_VIEW_DESC RTVDesc = {};
        RTVDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
        RTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
        RTVDesc.Texture2DArray.MipSlice = 0;
        RTVDesc.Texture2DArray.FirstArraySlice = SliceIndex;
        RTVDesc.Texture2DArray.ArraySize = 1;
        if (FAILED(Device->CreateRenderTargetView(MomentTexture, &RTVDesc, &MomentSliceRTVs[SliceIndex])))
        {
            Release();
            return false;
        }

        D3D11_SHADER_RESOURCE_VIEW_DESC SliceMomentDesc = {};
        SliceMomentDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
        SliceMomentDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
        SliceMomentDesc.Texture2DArray.MostDetailedMip = 0;
        SliceMomentDesc.Texture2DArray.MipLevels = 1;
        SliceMomentDesc.Texture2DArray.FirstArraySlice = SliceIndex;
        SliceMomentDesc.Texture2DArray.ArraySize = 1;
        if (FAILED(Device->CreateShaderResourceView(MomentTexture, &SliceMomentDesc, &MomentSliceSRVs[SliceIndex])))
        {
            Release();
            return false;
        }
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC MomentArrayDesc = {};
    MomentArrayDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
    MomentArrayDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
    MomentArrayDesc.Texture2DArray.MostDetailedMip = 0;
    MomentArrayDesc.Texture2DArray.MipLevels = static_cast<UINT>(-1);
    MomentArrayDesc.Texture2DArray.FirstArraySlice = 0;
    MomentArrayDesc.Texture2DArray.ArraySize = ShadowAtlas::SliceCount;
    if (FAILED(Device->CreateShaderResourceView(MomentTexture, &MomentArrayDesc, &MomentArraySRV)))
    {
        Release();
        return false;
    }

    return true;
}

void FShadowAtlas::Release()
{
    for (FBuddyAllocator2D& Allocator : SliceAllocators)
    {
        Allocator.Reset();
    }

    for (uint32 SliceIndex = 0; SliceIndex < ShadowAtlas::SliceCount; ++SliceIndex)
    {
        ReleaseView(reinterpret_cast<ID3D11DeviceChild*&>(SliceDSVs[SliceIndex]));
        ReleaseView(reinterpret_cast<ID3D11DeviceChild*&>(PreviewSliceSRVs[SliceIndex]));
        ReleaseView(reinterpret_cast<ID3D11DeviceChild*&>(MomentSliceRTVs[SliceIndex]));
        ReleaseView(reinterpret_cast<ID3D11DeviceChild*&>(MomentSliceSRVs[SliceIndex]));
    }

    ReleaseView(reinterpret_cast<ID3D11DeviceChild*&>(DepthArraySRV));
    ReleaseView(reinterpret_cast<ID3D11DeviceChild*&>(MomentArraySRV));
    ReleaseView(reinterpret_cast<ID3D11DeviceChild*&>(DepthTexture));
    ReleaseView(reinterpret_cast<ID3D11DeviceChild*&>(MomentTexture));
}

bool FShadowAtlas::Allocate(uint32 Resolution, uint32 AtlasPageIndex, FShadowMapData& OutData)
{
    // 같은 page 안에서는 slice를 앞에서부터 순회하며 먼저 맞는 자리를 찾습니다.
    for (uint32 SliceIndex = 0; SliceIndex < ShadowAtlas::SliceCount; ++SliceIndex)
    {
        FShadowMapData Candidate = {};
        if (!SliceAllocators[SliceIndex].Allocate(Resolution, Candidate))
        {
            continue;
        }

        Candidate.AtlasPageIndex = AtlasPageIndex;
        Candidate.SliceIndex = SliceIndex;
        OutData = Candidate;
        return true;
    }

    return false;
}

void FShadowAtlas::Free(const FShadowMapData& Allocation)
{
    if (!Allocation.bAllocated || Allocation.SliceIndex >= ShadowAtlas::SliceCount)
    {
        return;
    }

    SliceAllocators[Allocation.SliceIndex].Free(Allocation);
}

void FShadowAtlas::GatherSliceAllocations(uint32 SliceIndex, uint32 AtlasPageIndex, TArray<FShadowMapData>& OutAllocations) const
{
    if (SliceIndex >= ShadowAtlas::SliceCount)
    {
        return;
    }

    const size_t BaseCount = OutAllocations.size();
    SliceAllocators[SliceIndex].GatherAllocated(OutAllocations);
    for (size_t AllocationIndex = BaseCount; AllocationIndex < OutAllocations.size(); ++AllocationIndex)
    {
        OutAllocations[AllocationIndex].AtlasPageIndex = AtlasPageIndex;
        OutAllocations[AllocationIndex].SliceIndex = SliceIndex;
    }
}

ID3D11DepthStencilView* FShadowAtlas::GetSliceDSV(uint32 SliceIndex) const
{
    return (SliceIndex < ShadowAtlas::SliceCount) ? SliceDSVs[SliceIndex] : nullptr;
}

ID3D11RenderTargetView* FShadowAtlas::GetMomentSliceRTV(uint32 SliceIndex) const
{
    return (SliceIndex < ShadowAtlas::SliceCount) ? MomentSliceRTVs[SliceIndex] : nullptr;
}

ID3D11ShaderResourceView* FShadowAtlas::GetMomentSliceSRV(uint32 SliceIndex) const
{
    return (SliceIndex < ShadowAtlas::SliceCount) ? MomentSliceSRVs[SliceIndex] : nullptr;
}

ID3D11ShaderResourceView* FShadowAtlas::GetPreviewSliceSRV(uint32 SliceIndex) const
{
    return (SliceIndex < ShadowAtlas::SliceCount) ? PreviewSliceSRVs[SliceIndex] : nullptr;
}

FShadowAtlasManager::~FShadowAtlasManager()
{
    Release();
}

void FShadowAtlasManager::Release()
{
    for (FShadowAtlas* Page : Pages)
    {
        delete Page;
    }
    Pages.clear();
}

bool FShadowAtlasManager::Allocate(ID3D11Device* Device, uint32 Resolution, FShadowMapData& OutData)
{
    for (uint32 PageIndex = 0; PageIndex < Pages.size(); ++PageIndex)
    {
        if (Pages[PageIndex] && Pages[PageIndex]->Allocate(Resolution, PageIndex, OutData))
        {
            return true;
        }
    }

    if (Pages.size() >= ShadowAtlas::MaxPages)
    {
        return false;
    }

    // 기존 page에 빈 자리가 없으면 새 page를 만들지만, MaxPages 상한을 넘기지는 않습니다.
    FShadowAtlas* NewPage = new FShadowAtlas();
    if (!NewPage->Initialize(Device))
    {
        delete NewPage;
        return false;
    }

    const uint32 PageIndex = static_cast<uint32>(Pages.size());
    Pages.push_back(NewPage);
    return NewPage->Allocate(Resolution, PageIndex, OutData);
}

void FShadowAtlasManager::Free(const FShadowMapData& Allocation)
{
    if (!Allocation.bAllocated || Allocation.AtlasPageIndex >= Pages.size() || !Pages[Allocation.AtlasPageIndex])
    {
        return;
    }

    Pages[Allocation.AtlasPageIndex]->Free(Allocation);
}

FShadowAtlas* FShadowAtlasManager::GetPage(uint32 PageIndex)
{
    return (PageIndex < Pages.size()) ? Pages[PageIndex] : nullptr;
}

const FShadowAtlas* FShadowAtlasManager::GetPage(uint32 PageIndex) const
{
    return (PageIndex < Pages.size()) ? Pages[PageIndex] : nullptr;
}
