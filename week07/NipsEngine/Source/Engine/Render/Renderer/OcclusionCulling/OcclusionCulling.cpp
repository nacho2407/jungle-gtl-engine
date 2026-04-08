#include "Renderer.h"

#include <algorithm>

void FRenderer::BuildTemporalSurvivorIndices(const TArray<uint32>& VisibleIndices,
                                             uint32               TotalObjectCount,
                                             TArray<uint32>&      OutSurvivorIndices) const
{
    OutSurvivorIndices.clear();
    OutSurvivorIndices.reserve(VisibleIndices.size());

    if (!bHasAcceptedOcclusionResults || CachedOcclusionVisibleIndices.empty())
    {
        OutSurvivorIndices = VisibleIndices;
        return;
    }

    TArray<uint8> WasVisibleInAcceptedResult(TotalObjectCount, 0);
    TArray<uint8> WasSurvivorInAcceptedResult(TotalObjectCount, 0);

    for (uint32 Idx : CachedOcclusionVisibleIndices)
    {
        if (Idx < TotalObjectCount)
        {
            WasVisibleInAcceptedResult[Idx] = 1;
        }
    }

    for (uint32 Idx : CachedOcclusionSurvivorIndices)
    {
        if (Idx < TotalObjectCount)
        {
            WasSurvivorInAcceptedResult[Idx] = 1;
        }
    }

    for (uint32 Idx : VisibleIndices)
    {
        if (Idx >= TotalObjectCount)
        {
            continue;
        }

        // Newly visible objects should not wait for the next readback, so keep them.
        if (!WasVisibleInAcceptedResult[Idx] || WasSurvivorInAcceptedResult[Idx])
        {
            OutSurvivorIndices.push_back(Idx);
        }
    }

    // Avoid blanking the frame when the temporal result is too stale.
    if (OutSurvivorIndices.empty() && !VisibleIndices.empty())
    {
        OutSurvivorIndices = VisibleIndices;
    }
}

void FRenderer::ResetOcclusionCullState()
{
    bOcclusionReadbackPending = false;
    bStagingValid = false;
    bHasCachedOcclusionResults = false;
    bHasAcceptedOcclusionResults = false;
    OcclusionPendingSlot = 0;
    CachedOcclusionVisibleIndices.clear();
    CachedOcclusionSurvivorIndices.clear();
    OcclusionConsecutiveOccludedCounts.clear();
    OcclusionSubmittedVisibleIndices[0].clear();
    OcclusionSubmittedVisibleIndices[1].clear();

    OcclusionDebugState.bUsingCachedResults = false;
    OcclusionDebugState.bSubmittedThisFrame = false;
    OcclusionDebugState.bReadbackPending = false;
    OcclusionDebugState.bHasCachedResults = false;
    OcclusionDebugState.bHasReadbackResult = false;
    OcclusionDebugState.bAcceptedCullResult = false;
    OcclusionDebugState.ProposedSurvivorCount = 0;
    OcclusionDebugState.CachedSurvivorCount = 0;
    OcclusionDebugState.CullSavings = 0;
    OcclusionDebugState.RequiredSavings = 0;
    OcclusionDebugState.LastSubmittedCount = 0;
}

void FRenderer::ExecuteDepthPrepass(const TArray<FRenderCommand>& Commands,
                                    ID3D11DeviceContext*          Context)
{
    if (Commands.empty())
    {
        return;
    }

    // Bind only DSV for z-only depth prepass.
    ID3D11DepthStencilView* PrepassDSV = Device.GetDepthPrepassDSV();
    Context->OMSetRenderTargets(0, nullptr, PrepassDSV);

    Device.SetDepthStencilState(EDepthStencilState::DepthPrepassWrite);
    Device.SetBlendState(EBlendState::NoColor);
    Device.SetRasterizerState(ERasterizerState::SolidBackCull);
    Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // VS-only depth prepass path.
    Resources.DepthPrepassShader.Bind(Context);

    // b1: per-object transform.
    ID3D11Buffer* cb1 = Resources.PerObjectConstantBuffer.GetBuffer();
    Context->VSSetConstantBuffers(1, 1, &cb1);

    FMeshBuffer* LastMB = nullptr;

    for (const auto& Cmd : Commands)
    {
        if (!Cmd.MeshBuffer || !Cmd.MeshBuffer->IsValid())
        {
            continue;
        }

        Resources.PerObjectConstantBuffer.Update(Context, &Cmd.PerObjectConstants,
                                                 sizeof(FPerObjectConstants));

        if (Cmd.MeshBuffer != LastMB)
        {
            uint32 Stride = Cmd.MeshBuffer->GetVertexBuffer().GetStride();
            uint32 Offset = 0;
            ID3D11Buffer* VB = Cmd.MeshBuffer->GetVertexBuffer().GetBuffer();
            Context->IASetVertexBuffers(0, 1, &VB, &Stride, &Offset);

            ID3D11Buffer* IB = Cmd.MeshBuffer->GetIndexBuffer().GetBuffer();
            if (IB)
            {
                Context->IASetIndexBuffer(IB, DXGI_FORMAT_R32_UINT, 0);
            }

            LastMB = Cmd.MeshBuffer;
        }

        ID3D11Buffer* IB = Cmd.MeshBuffer->GetIndexBuffer().GetBuffer();
        if (IB)
        {
            Context->DrawIndexed(Cmd.SectionIndexCount, Cmd.SectionIndexStart, 0);
        }
        else
        {
            Context->Draw(Cmd.MeshBuffer->GetVertexBuffer().GetVertexCount(), 0);
        }
    }
}

void FRenderer::ExecuteHiZDownsample(ID3D11DeviceContext* Context)
{
    const uint32 MipCount = Device.GetHiZMipCount();
    if (MipCount <= 1)
    {
        return;
    }

    // Ensure OM is unbound before running compute.
    Context->OMSetRenderTargets(0, nullptr, nullptr);

    Resources.HiZDownsampleCS.Bind(Context);

    ID3D11Buffer* cb0 = Resources.HiZConstantBuffer.GetBuffer();
    Context->CSSetConstantBuffers(0, 1, &cb0);

    const uint32 W = static_cast<uint32>(Device.GetRenderTargetWidth());
    const uint32 H = static_cast<uint32>(Device.GetRenderTargetHeight());

    // Build mip chain from mip1. mip0 is populated separately by ExecuteHiZCopyMip0.
    for (uint32 Mip = 1; Mip < MipCount; ++Mip)
    {
        const uint32 SrcW = std::max(1u, W >> (Mip - 1));
        const uint32 SrcH = std::max(1u, H >> (Mip - 1));
        const uint32 DstW = std::max(1u, W >> Mip);
        const uint32 DstH = std::max(1u, H >> Mip);

        FHiZConstants HiZConsts = {SrcW, SrcH, DstW, DstH};
        Resources.HiZConstantBuffer.Update(Context, &HiZConsts, sizeof(FHiZConstants));

        ID3D11ShaderResourceView* SRV = Device.GetHiZMipSRV(Mip - 1);
        ID3D11UnorderedAccessView* UAV = Device.GetHiZMipUAV(Mip);
        Context->CSSetShaderResources(0, 1, &SRV);
        Context->CSSetUnorderedAccessViews(0, 1, &UAV, nullptr);

        Context->Dispatch((DstW + 7) / 8, (DstH + 7) / 8, 1);
    }

    ID3D11ShaderResourceView* NullSRV = nullptr;
    ID3D11UnorderedAccessView* NullUAV = nullptr;
    Context->CSSetShaderResources(0, 1, &NullSRV);
    Context->CSSetUnorderedAccessViews(0, 1, &NullUAV, nullptr);
    Context->CSSetShader(nullptr, nullptr, 0);

    // Restore scene render target bindings.
    ID3D11RenderTargetView* RTV = CurrentRenderTargets.SceneColorRTV;
    Context->OMSetRenderTargets(1, &RTV, CurrentRenderTargets.DepthStencilView);
}

void FRenderer::ExecuteHiZCopyMip0(ID3D11DeviceContext* Context)
{
    const uint32 W = static_cast<uint32>(Device.GetRenderTargetWidth());
    const uint32 H = static_cast<uint32>(Device.GetRenderTargetHeight());
    if (W == 0 || H == 0)
    {
        return;
    }

    Context->OMSetRenderTargets(0, nullptr, nullptr);

    Resources.HiZCopyMip0CS.Bind(Context);

    FHiZConstants HiZConsts = {W, H, W, H};
    Resources.HiZConstantBuffer.Update(Context, &HiZConsts, sizeof(FHiZConstants));

    ID3D11Buffer* cb0 = Resources.HiZConstantBuffer.GetBuffer();
    Context->CSSetConstantBuffers(0, 1, &cb0);

    ID3D11ShaderResourceView* SRV = Device.GetDepthPrepassSRV();
    ID3D11UnorderedAccessView* UAV = Device.GetHiZMipUAV(0);
    Context->CSSetShaderResources(0, 1, &SRV);
    Context->CSSetUnorderedAccessViews(0, 1, &UAV, nullptr);

    Context->Dispatch((W + 7) / 8, (H + 7) / 8, 1);

    ID3D11ShaderResourceView* NullSRV = nullptr;
    ID3D11UnorderedAccessView* NullUAV = nullptr;
    Context->CSSetShaderResources(0, 1, &NullSRV);
    Context->CSSetUnorderedAccessViews(0, 1, &NullUAV, nullptr);
    Context->CSSetShader(nullptr, nullptr, 0);
}

void FRenderer::ExecuteOcclusionCull(ID3D11DeviceContext*  Context,
                                     const TArray<uint32>& VisibleIndices,
                                     const TArray<FAABB>&  AABBBoundsArray)
{
    const uint32 Count = static_cast<uint32>(VisibleIndices.size());
    if (Count == 0)
    {
        return;
    }

    const uint32 SafeCount = std::min(Count, MaxOcclusionObjects);

    // 1. Upload AABB data.
    D3D11_MAPPED_SUBRESOURCE Mapped = {};
    Context->Map(OcclusionAABBBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped);
    FGPUOcclusionAABB* Dst = static_cast<FGPUOcclusionAABB*>(Mapped.pData);
    for (uint32 i = 0; i < SafeCount; ++i)
    {
        const FAABB& Box = AABBBoundsArray[VisibleIndices[i]];
        Dst[i].Min = Box.Min;
        Dst[i].Pad0 = 0.0f;
        Dst[i].Max = Box.Max;
        Dst[i].Pad1 = 0.0f;
    }
    Context->Unmap(OcclusionAABBBuffer.Get(), 0);

    // 2. Update cbuffer.
    FOcclusionCullConstants OcConsts = {};
    OcConsts.ViewProjection = CachedViewProjection;
    OcConsts.CameraForward = CachedOcclusionCameraForward;
    OcConsts.CameraRight = CachedOcclusionCameraRight;
    OcConsts.CameraUp = CachedOcclusionCameraUp;
    OcConsts.ViewportWidth = Device.GetViewportWidth();
    OcConsts.ViewportHeight = Device.GetViewportHeight();
    OcConsts.ViewportMinX = Device.GetViewportX();
    OcConsts.CameraPosition = CachedOcclusionCameraPosition;
    OcConsts.ViewportMinY = Device.GetViewportY();
    OcConsts.TargetWidth = Device.GetRenderTargetWidth();
    OcConsts.TargetHeight = Device.GetRenderTargetHeight();
    OcConsts.HiZMipCount = static_cast<float>(Device.GetHiZMipCount());
    OcConsts.ObjectCount = SafeCount;
    Resources.OcclusionCullConstantBuffer.Update(Context, &OcConsts, sizeof(OcConsts));

    // 3. Dispatch occlusion CS.
    Resources.OcclusionCullCS.Bind(Context);
    ID3D11Buffer* cb0 = Resources.OcclusionCullConstantBuffer.GetBuffer();
    Context->CSSetConstantBuffers(0, 1, &cb0);

    ID3D11ShaderResourceView* SRVs[2] = {OcclusionAABBSRV.Get(), Device.GetHiZFullSRV()};
    Context->CSSetShaderResources(0, 2, SRVs);

    ID3D11UnorderedAccessView* UAV = OcclusionResultUAV.Get();
    Context->CSSetUnorderedAccessViews(0, 1, &UAV, nullptr);

    Context->Dispatch((SafeCount + 63) / 64, 1, 1);

    // 4. Unbind UAV/SRV before CopyResource.
    ID3D11ShaderResourceView* NullSRVs[2] = {nullptr, nullptr};
    ID3D11UnorderedAccessView* NullUAV = nullptr;
    Context->CSSetShaderResources(0, 2, NullSRVs);
    Context->CSSetUnorderedAccessViews(0, 1, &NullUAV, nullptr);
    Context->CSSetShader(nullptr, nullptr, 0);

    // 5. Copy result to staging and submit completion query.
    const uint32 writeIdx = StagingWriteIdx;
    OcclusionSubmittedVisibleIndices[writeIdx].assign(VisibleIndices.begin(),
                                                      VisibleIndices.begin() + SafeCount);
    OcclusionSubmittedViewProjection[writeIdx] = CachedViewProjection;

    Context->CopyResource(OcclusionResultStaging[writeIdx].Get(), OcclusionResultBuffer.Get());
    if (OcclusionResultQueries[writeIdx])
    {
        Context->End(OcclusionResultQueries[writeIdx].Get());
    }
    OcclusionPendingSlot = writeIdx;
    StagingWriteIdx = 1 - writeIdx;
    bStagingValid = true;
    bOcclusionReadbackPending = true;
}

bool FRenderer::TryConsumeOcclusionCullResult(ID3D11DeviceContext* Context,
                                              uint32               TotalObjectCount,
                                              bool                 bApplyHideHysteresis)
{
    if (!bOcclusionReadbackPending || !bStagingValid)
    {
        return false;
    }

    const uint32 readIdx = OcclusionPendingSlot;
    if (!OcclusionResultQueries[readIdx] ||
        Context->GetData(OcclusionResultQueries[readIdx].Get(), nullptr, 0,
                         D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK)
    {
        return false;
    }

    D3D11_MAPPED_SUBRESOURCE ReadMapped = {};
    if (FAILED(Context->Map(OcclusionResultStaging[readIdx].Get(), 0, D3D11_MAP_READ, 0,
                            &ReadMapped)))
    {
        return false;
    }

    const TArray<uint32>& SubmittedIndices = OcclusionSubmittedVisibleIndices[readIdx];
    const uint32* Results = static_cast<const uint32*>(ReadMapped.pData);

    OcclusionConsecutiveOccludedCounts.resize(TotalObjectCount, 0);

    TArray<uint8> SubmittedThisResult(TotalObjectCount, 0);
    TArray<uint32> ProposedSurvivors;
    TArray<uint32> HysteresisSurvivors;
    ProposedSurvivors.reserve(SubmittedIndices.size());
    HysteresisSurvivors.reserve(SubmittedIndices.size());
    for (uint32 i = 0; i < static_cast<uint32>(SubmittedIndices.size()); ++i)
    {
        const uint32 RenderCommandIndex = SubmittedIndices[i];
        if (RenderCommandIndex >= TotalObjectCount)
        {
            continue;
        }

        SubmittedThisResult[RenderCommandIndex] = 1;

        if (Results[i])
        {
            OcclusionConsecutiveOccludedCounts[RenderCommandIndex] = 0;
            ProposedSurvivors.push_back(RenderCommandIndex);
            HysteresisSurvivors.push_back(RenderCommandIndex);
            continue;
        }

        uint8& ConsecutiveOccludedCount = OcclusionConsecutiveOccludedCounts[RenderCommandIndex];
        if (bApplyHideHysteresis)
        {
            ConsecutiveOccludedCount = static_cast<uint8>(
                std::min<uint32>(static_cast<uint32>(ConsecutiveOccludedCount) + 1u, 255u));
        }
        else
        {
            ConsecutiveOccludedCount = OcclusionHideHysteresisFrames;
        }

        if (ConsecutiveOccludedCount < OcclusionHideHysteresisFrames)
        {
            HysteresisSurvivors.push_back(RenderCommandIndex);
        }
    }

    for (uint32 ObjectIndex = 0; ObjectIndex < TotalObjectCount; ++ObjectIndex)
    {
        if (!SubmittedThisResult[ObjectIndex])
        {
            OcclusionConsecutiveOccludedCounts[ObjectIndex] = 0;
        }
    }

    Context->Unmap(OcclusionResultStaging[readIdx].Get(), 0);

    const uint32 SubmittedCount = static_cast<uint32>(SubmittedIndices.size());
    const uint32 SurvivorCount = static_cast<uint32>(ProposedSurvivors.size());
    const uint32 CullSavings = SubmittedCount - SurvivorCount;
    const uint32 RequiredSavings = std::max(MinOcclusionCullSavings, SubmittedCount / 8u);
    const bool bAcceptCullResult = SurvivorCount > 0 && CullSavings >= RequiredSavings;

    OcclusionDebugState.ProposedSurvivorCount = SurvivorCount;
    OcclusionDebugState.CullSavings = CullSavings;
    OcclusionDebugState.RequiredSavings = RequiredSavings;
    OcclusionDebugState.bHasReadbackResult = true;
    OcclusionDebugState.bAcceptedCullResult = bAcceptCullResult;

    if (bAcceptCullResult)
    {
        CachedOcclusionVisibleIndices = SubmittedIndices;
        CachedOcclusionSurvivorIndices = std::move(HysteresisSurvivors);
        CachedOcclusionResultViewProjection = OcclusionSubmittedViewProjection[readIdx];
    }
    else
    {
        CachedOcclusionVisibleIndices.clear();
        CachedOcclusionSurvivorIndices.clear();
        std::fill(OcclusionConsecutiveOccludedCounts.begin(),
                  OcclusionConsecutiveOccludedCounts.end(), 0);
    }

    bHasCachedOcclusionResults = bAcceptCullResult;
    bHasAcceptedOcclusionResults = bAcceptCullResult;
    bOcclusionReadbackPending = false;
    return true;
}
