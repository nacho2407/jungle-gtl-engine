#include "Render/Submission/Atlas/ShadowMomentFilter.h"

#include "Render/Execute/Context/RenderPipelineContext.h"
#include "Render/Resources/Bindings/RenderBindingSlots.h"
#include "Render/Resources/Buffers/ConstantBufferData.h"
#include "Render/Resources/Shadows/ShadowFilterSettings.h"
#include "Render/RHI/D3D11/Device/D3DDevice.h"
#include "Render/RHI/D3D11/Shaders/ShaderProgramBase.h"
#include "Render/Submission/Atlas/ShadowAtlasPage.h"
#include "Render/Submission/Command/DrawCommandList.h"

#include <cstring>

namespace
{
template <typename T>
void SafeRelease(T*& Ptr)
{
    if (Ptr)
    {
        Ptr->Release();
        Ptr = nullptr;
    }
}

FMomentBlurCBData BuildMomentBlurCBData(const FShadowMapData& Allocation)
{
    FMomentBlurCBData BlurCBData = {};
    BlurCBData.TexelSizeX = 1.0f / static_cast<float>(ShadowAtlas::AtlasSize);
    BlurCBData.TexelSizeY = 1.0f / static_cast<float>(ShadowAtlas::AtlasSize);

    const float AtlasSize = static_cast<float>(ShadowAtlas::AtlasSize);
    const float HalfTexel = 0.5f / AtlasSize;
    BlurCBData.UVScaleX = static_cast<float>(Allocation.ViewportRect.Width) / AtlasSize;
    BlurCBData.UVScaleY = static_cast<float>(Allocation.ViewportRect.Height) / AtlasSize;
    BlurCBData.UVOffsetX = static_cast<float>(Allocation.ViewportRect.X) / AtlasSize;
    BlurCBData.UVOffsetY = static_cast<float>(Allocation.ViewportRect.Y) / AtlasSize;
    BlurCBData.UVMinX = BlurCBData.UVOffsetX + HalfTexel;
    BlurCBData.UVMinY = BlurCBData.UVOffsetY + HalfTexel;
    BlurCBData.UVMaxX = BlurCBData.UVOffsetX + BlurCBData.UVScaleX - HalfTexel;
    BlurCBData.UVMaxY = BlurCBData.UVOffsetY + BlurCBData.UVScaleY - HalfTexel;
    return BlurCBData;
}
} // namespace

FShadowMomentFilter::~FShadowMomentFilter()
{
    Release();
}

void FShadowMomentFilter::EnsureResources(ID3D11Device* Device)
{
    if (Device == nullptr)
    {
        return;
    }

    if (BlurVS == nullptr || BlurPSHorizontal == nullptr || BlurPSVertical == nullptr)
    {
        FShaderStageDesc BlurVSDesc = {};
        BlurVSDesc.FilePath         = "Shaders/Passes/Scene/Shared/ShadowMomentBlurPass.hlsl";
        BlurVSDesc.EntryPoint       = "VS";

        FShaderStageDesc BlurPSHorizontalDesc = {};
        BlurPSHorizontalDesc.FilePath         = "Shaders/Passes/Scene/Shared/ShadowMomentBlurPass.hlsl";
        BlurPSHorizontalDesc.EntryPoint       = "PS_Horizontal";

        FShaderStageDesc BlurPSVerticalDesc = {};
        BlurPSVerticalDesc.FilePath         = "Shaders/Passes/Scene/Shared/ShadowMomentBlurPass.hlsl";
        BlurPSVerticalDesc.EntryPoint       = "PS_Vertical";

        ID3DBlob* VsBlob = nullptr;
        ID3DBlob* PsHorizontalBlob = nullptr;
        ID3DBlob* PsVerticalBlob = nullptr;

        const bool bCompiledVS = FShaderProgramBase::CompileShaderBlobStandalone(
            &VsBlob, BlurVSDesc, "vs_5_0", "Shadow Moment Blur VS Compile Error");
        const bool bCompiledH = FShaderProgramBase::CompileShaderBlobStandalone(
            &PsHorizontalBlob, BlurPSHorizontalDesc, "ps_5_0", "Shadow Moment Blur Horizontal PS Compile Error");
        const bool bCompiledV = FShaderProgramBase::CompileShaderBlobStandalone(
            &PsVerticalBlob, BlurPSVerticalDesc, "ps_5_0", "Shadow Moment Blur Vertical PS Compile Error");
        if (!bCompiledVS || !bCompiledH || !bCompiledV)
        {
            SafeRelease(VsBlob);
            SafeRelease(PsHorizontalBlob);
            SafeRelease(PsVerticalBlob);
            return;
        }

        const HRESULT HrVS = Device->CreateVertexShader(VsBlob->GetBufferPointer(), VsBlob->GetBufferSize(), nullptr, &BlurVS);
        const HRESULT HrH = Device->CreatePixelShader(PsHorizontalBlob->GetBufferPointer(), PsHorizontalBlob->GetBufferSize(), nullptr, &BlurPSHorizontal);
        const HRESULT HrV = Device->CreatePixelShader(PsVerticalBlob->GetBufferPointer(), PsVerticalBlob->GetBufferSize(), nullptr, &BlurPSVertical);

        SafeRelease(VsBlob);
        SafeRelease(PsHorizontalBlob);
        SafeRelease(PsVerticalBlob);

        if (FAILED(HrVS) || FAILED(HrH) || FAILED(HrV))
        {
            Release();
            return;
        }
    }

    if (BlurCB == nullptr)
    {
        D3D11_BUFFER_DESC CbDesc = {};
        CbDesc.ByteWidth = sizeof(FMomentBlurCBData);
        CbDesc.Usage = D3D11_USAGE_DYNAMIC;
        CbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        CbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if (FAILED(Device->CreateBuffer(&CbDesc, nullptr, &BlurCB)))
        {
            return;
        }
    }

    if (BlurTemp2D && BlurTempSize == ShadowAtlas::AtlasSize)
    {
        return;
    }

    SafeRelease(BlurTempSRV);
    SafeRelease(BlurTempRTV);
    SafeRelease(BlurTemp2D);
    BlurTempSize = 0;

    D3D11_TEXTURE2D_DESC TempDesc = {};
    TempDesc.Width = ShadowAtlas::AtlasSize;
    TempDesc.Height = ShadowAtlas::AtlasSize;
    TempDesc.MipLevels = 1;
    TempDesc.ArraySize = 1;
    TempDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
    TempDesc.SampleDesc.Count = 1;
    TempDesc.Usage = D3D11_USAGE_DEFAULT;
    TempDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    if (FAILED(Device->CreateTexture2D(&TempDesc, nullptr, &BlurTemp2D)) ||
        FAILED(Device->CreateRenderTargetView(BlurTemp2D, nullptr, &BlurTempRTV)) ||
        FAILED(Device->CreateShaderResourceView(BlurTemp2D, nullptr, &BlurTempSRV)))
    {
        Release();
        return;
    }

    BlurTempSize = ShadowAtlas::AtlasSize;
}

void FShadowMomentFilter::Release()
{
    SafeRelease(BlurTempSRV);
    SafeRelease(BlurTempRTV);
    SafeRelease(BlurTemp2D);
    SafeRelease(BlurCB);
    SafeRelease(BlurPSVertical);
    SafeRelease(BlurPSHorizontal);
    SafeRelease(BlurVS);
    BlurTempSize = 0;
}

void FShadowMomentFilter::BlurMomentTextureSlice(FRenderPipelineContext& Context, FShadowAtlasPage& AtlasPage, uint32 SliceIndex)
{
    if ((GetShadowFilterMethod() != EShadowFilterMethod::VSM &&
         GetShadowFilterMethod() != EShadowFilterMethod::ESM) ||
        !Context.Device || !Context.Context)
    {
        return;
    }

    EnsureResources(Context.Device->GetDevice());
    if (!BlurVS || !BlurPSHorizontal || !BlurPSVertical || !BlurCB || !BlurTempRTV || !BlurTempSRV)
    {
        return;
    }

    ID3D11RenderTargetView* TargetRTV = AtlasPage.GetMomentSliceRTV(SliceIndex);
    ID3D11ShaderResourceView* TargetSRV = AtlasPage.GetMomentSliceSRV(SliceIndex);
    if (!TargetRTV || !TargetSRV)
    {
        return;
    }

    Context.Device->SetDepthStencilState(EDepthStencilState::NoDepth);
    Context.Device->SetBlendState(EBlendState::Opaque);
    Context.Device->SetRasterizerState(ERasterizerState::SolidNoCull);

    Context.Context->IASetInputLayout(nullptr);
    Context.Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    Context.Context->VSSetShader(BlurVS, nullptr, 0);
    Context.Context->PSSetConstantBuffers(ECBSlot::PerShader0, 1, &BlurCB);

    TArray<FShadowMapData> SliceAllocations;
    AtlasPage.GatherSliceAllocations(SliceIndex, 0, SliceAllocations);

    ID3D11ShaderResourceView* NullSRV = nullptr;
    for (const FShadowMapData& Allocation : SliceAllocations)
    {
        if (!Allocation.bAllocated || Allocation.ViewportRect.Width == 0 || Allocation.ViewportRect.Height == 0)
        {
            continue;
        }

        const FMomentBlurCBData BlurCBData = BuildMomentBlurCBData(Allocation);
        D3D11_MAPPED_SUBRESOURCE Mapped = {};
        if (SUCCEEDED(Context.Context->Map(BlurCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped)))
        {
            std::memcpy(Mapped.pData, &BlurCBData, sizeof(BlurCBData));
            Context.Context->Unmap(BlurCB, 0);
        }

        D3D11_VIEWPORT Viewport = {};
        Viewport.TopLeftX = static_cast<float>(Allocation.ViewportRect.X);
        Viewport.TopLeftY = static_cast<float>(Allocation.ViewportRect.Y);
        Viewport.Width = static_cast<float>(Allocation.ViewportRect.Width);
        Viewport.Height = static_cast<float>(Allocation.ViewportRect.Height);
        Viewport.MinDepth = 0.0f;
        Viewport.MaxDepth = 1.0f;
        Context.Context->RSSetViewports(1, &Viewport);

        D3D11_RECT ScissorRect = {};
        ScissorRect.left = static_cast<LONG>(Allocation.ViewportRect.X);
        ScissorRect.top = static_cast<LONG>(Allocation.ViewportRect.Y);
        ScissorRect.right = static_cast<LONG>(Allocation.ViewportRect.X + Allocation.ViewportRect.Width);
        ScissorRect.bottom = static_cast<LONG>(Allocation.ViewportRect.Y + Allocation.ViewportRect.Height);
        Context.Context->RSSetScissorRects(1, &ScissorRect);

        Context.Context->OMSetRenderTargets(1, &BlurTempRTV, nullptr);
        Context.Context->PSSetShader(BlurPSHorizontal, nullptr, 0);
        Context.Context->PSSetShaderResources(0, 1, &TargetSRV);
        Context.Context->Draw(3, 0);
        Context.Context->PSSetShaderResources(0, 1, &NullSRV);

        Context.Context->OMSetRenderTargets(1, &TargetRTV, nullptr);
        Context.Context->PSSetShader(BlurPSVertical, nullptr, 0);
        Context.Context->PSSetShaderResources(0, 1, &BlurTempSRV);
        Context.Context->Draw(3, 0);
        Context.Context->PSSetShaderResources(0, 1, &NullSRV);
    }

    if (Context.StateCache)
    {
        Context.StateCache->Reset();
    }
}
