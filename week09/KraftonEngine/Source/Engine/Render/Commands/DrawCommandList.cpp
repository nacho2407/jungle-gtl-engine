#include "DrawCommandList.h"

#include <algorithm>
#include <cstring>

#include "Profiling/Stats.h"
#include "Render/Core/RenderConstants.h"
#include "Render/Hardware/Resources/Shader.h"

// ============================================================
// FStateCache
// ============================================================

void FStateCache::Reset()
{
    bForceAll = true;

    Shader = nullptr;
    DepthStencil = {};
    Blend = {};
    Rasterizer = {};
    Topology = {};
    StencilRef = 0;
    MeshBuffer = nullptr;
    RawVB = nullptr;
    RawIB = nullptr;
    PerObjectCB = nullptr;
    PerShaderCB[0] = nullptr;
    PerShaderCB[1] = nullptr;
    LightCB = nullptr;
    DiffuseSRV = nullptr;
    NormalSRV = nullptr;
    LocalLightSRV = nullptr;

    RTV = nullptr;
    DSV = nullptr;
}

void FStateCache::Cleanup(ID3D11DeviceContext* Ctx)
{
    ID3D11ShaderResourceView* NullSRVs[8] = {};
    Ctx->PSSetShaderResources(0, ARRAYSIZE(NullSRVs), NullSRVs);
    DiffuseSRV = nullptr;
    NormalSRV = nullptr;

    ID3D11ShaderResourceView* NullLocalLightSRV = nullptr;
    Ctx->PSSetShaderResources(ESystemTexSlot::LocalLights, 1, &NullLocalLightSRV);
    LocalLightSRV = nullptr;
}

// ============================================================
// FDrawCommandList
// ============================================================

FDrawCommand& FDrawCommandList::AddCommand()
{
    Commands.emplace_back();
    return Commands.back();
}

void FDrawCommandList::Sort()
{
    if (Commands.size() > 1)
    {
        std::sort(Commands.begin(), Commands.end(),
                  [](const FDrawCommand& A, const FDrawCommand& B)
                  {
                      return A.SortKey < B.SortKey;
                  });
    }

    std::memset(PassOffsets, 0, sizeof(PassOffsets));
    const uint32 Total = static_cast<uint32>(Commands.size());
    uint32 Idx = 0;
    for (uint32 P = 0; P < (uint32)ERenderPass::MAX; ++P)
    {
        PassOffsets[P] = Idx;
        while (Idx < Total && (uint32)Commands[Idx].Pass == P)
            ++Idx;
    }
    PassOffsets[(uint32)ERenderPass::MAX] = Total;
}

void FDrawCommandList::GetPassRange(ERenderPass Pass, uint32& OutStart, uint32& OutEnd) const
{
    OutStart = PassOffsets[(uint32)Pass];
    OutEnd = PassOffsets[(uint32)Pass + 1];
}

void FDrawCommandList::Submit(FD3DDevice& Device, ID3D11DeviceContext* Ctx)
{
    if (Commands.empty())
        return;

    SCOPE_STAT_CAT("DrawCommandList::Submit", "4_ExecutePass");

    FStateCache Cache;
    Cache.Reset();

    for (const FDrawCommand& Cmd : Commands)
    {
        SubmitCommand(Cmd, Device, Ctx, Cache);
    }

    Cache.Cleanup(Ctx);
}

void FDrawCommandList::SubmitRange(uint32 StartIdx, uint32 EndIdx, FD3DDevice& Device, ID3D11DeviceContext* Ctx)
{
    if (StartIdx >= EndIdx)
        return;
    if (EndIdx > Commands.size())
        EndIdx = static_cast<uint32>(Commands.size());

    FStateCache Cache;
    Cache.Reset();

    for (uint32 i = StartIdx; i < EndIdx; ++i)
    {
        SubmitCommand(Commands[i], Device, Ctx, Cache);
    }

    Cache.Cleanup(Ctx);
}

void FDrawCommandList::SubmitRange(uint32 StartIdx, uint32 EndIdx, FD3DDevice& Device,
                                   ID3D11DeviceContext* Ctx, FStateCache& Cache)
{
    if (StartIdx >= EndIdx)
        return;
    if (EndIdx > Commands.size())
        EndIdx = static_cast<uint32>(Commands.size());

    for (uint32 i = StartIdx; i < EndIdx; ++i)
    {
        SubmitCommand(Commands[i], Device, Ctx, Cache);
    }
}

void FDrawCommandList::Reset()
{
    Commands.clear();
    std::memset(PassOffsets, 0, sizeof(PassOffsets));
}

uint32 FDrawCommandList::GetCommandCount(ERenderPass Pass) const
{
    return PassOffsets[(uint32)Pass + 1] - PassOffsets[(uint32)Pass];
}

// ============================================================
// 단일 커맨드 GPU 제출 — StateCache 비교 후 변경분만 바인딩
// ============================================================

void FDrawCommandList::SubmitCommand(const FDrawCommand& Cmd, FD3DDevice& Device,
                                     ID3D11DeviceContext* Ctx, FStateCache& Cache)
{
    const bool bForce = Cache.bForceAll;

    if (bForce || Cmd.DepthStencil != Cache.DepthStencil)
    {
        Device.SetDepthStencilState(Cmd.DepthStencil);
        Cache.DepthStencil = Cmd.DepthStencil;
    }

    if (bForce || Cmd.Blend != Cache.Blend)
    {
        Device.SetBlendState(Cmd.Blend);
        Cache.Blend = Cmd.Blend;
    }

    if (bForce || Cmd.Rasterizer != Cache.Rasterizer)
    {
        Device.SetRasterizerState(Cmd.Rasterizer);
        Cache.Rasterizer = Cmd.Rasterizer;
    }

    if (bForce || Cmd.Topology != Cache.Topology)
    {
        Ctx->IASetPrimitiveTopology(Cmd.Topology);
        Cache.Topology = Cmd.Topology;
    }

    if (Cmd.Shader && (bForce || Cmd.Shader != Cache.Shader))
    {
        Cmd.Shader->Bind(Ctx);

        if (Cmd.Pass == ERenderPass::DepthPre)
        {
            Ctx->PSSetShader(nullptr, nullptr, 0);
        }

        Cache.Shader = Cmd.Shader;
    }

    if (Cmd.MeshBuffer)
    {
        if (bForce || Cmd.MeshBuffer != Cache.MeshBuffer)
        {
            uint32 Offset = 0;
            uint32 Stride = Cmd.MeshBuffer->GetVertexBuffer().GetStride();
            ID3D11Buffer* VB = Cmd.MeshBuffer->GetVertexBuffer().GetBuffer();

            if (VB && Stride > 0)
            {
                Ctx->IASetVertexBuffers(0, 1, &VB, &Stride, &Offset);

                ID3D11Buffer* IB = Cmd.MeshBuffer->GetIndexBuffer().GetBuffer();
                if (IB)
                {
                    Ctx->IASetIndexBuffer(IB, DXGI_FORMAT_R32_UINT, 0);
                }
            }

            Cache.MeshBuffer = Cmd.MeshBuffer;
            Cache.RawVB = nullptr;
            Cache.RawIB = nullptr;
        }
    }
    else if (Cmd.RawVB)
    {
        if (bForce || Cmd.RawVB != Cache.RawVB)
        {
            uint32 Offset = 0;
            Ctx->IASetVertexBuffers(0, 1, &Cmd.RawVB, &Cmd.RawVBStride, &Offset);
            Cache.RawVB = Cmd.RawVB;
            Cache.MeshBuffer = nullptr;
        }

        if (bForce || Cmd.RawIB != Cache.RawIB)
        {
            Ctx->IASetIndexBuffer(Cmd.RawIB, DXGI_FORMAT_R32_UINT, 0);
            Cache.RawIB = Cmd.RawIB;
        }
    }
    else if (Cache.MeshBuffer || Cache.RawVB)
    {
        Ctx->IASetInputLayout(nullptr);
        Ctx->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
        Cache.MeshBuffer = nullptr;
        Cache.RawVB = nullptr;
        Cache.RawIB = nullptr;
    }

    if (Cmd.PerObjectCB && (bForce || Cmd.PerObjectCB != Cache.PerObjectCB))
    {
        ID3D11Buffer* RawCB = Cmd.PerObjectCB->GetBuffer();
        if (RawCB)
        {
            Ctx->VSSetConstantBuffers(ECBSlot::PerObject, 1, &RawCB);
        }
        Cache.PerObjectCB = Cmd.PerObjectCB;
    }

    for (uint32 i = 0; i < 2; ++i)
    {
        if (bForce || Cmd.PerShaderCB[i] != Cache.PerShaderCB[i])
        {
            const uint32 Slot = ECBSlot::PerShader0 + i;
            ID3D11Buffer* RawCB = Cmd.PerShaderCB[i] ? Cmd.PerShaderCB[i]->GetBuffer() : nullptr;
            Ctx->VSSetConstantBuffers(Slot, 1, &RawCB);
            Ctx->PSSetConstantBuffers(Slot, 1, &RawCB);
            Cache.PerShaderCB[i] = Cmd.PerShaderCB[i];
        }
    }

    if (Cmd.LightCB && (bForce || Cmd.LightCB != Cache.LightCB))
    {
        ID3D11Buffer* RawCB = Cmd.LightCB->GetBuffer();
        if (RawCB)
        {
            Ctx->PSSetConstantBuffers(ECBSlot::Light, 1, &RawCB);
        }
        Cache.LightCB = Cmd.LightCB;
    }

    if (bForce || Cmd.LocalLightSRV != Cache.LocalLightSRV)
    {
        ID3D11ShaderResourceView* SRV = Cmd.LocalLightSRV;
        Ctx->PSSetShaderResources(ESystemTexSlot::LocalLights, 1, &SRV);
        Cache.LocalLightSRV = Cmd.LocalLightSRV;
    }

    const bool bUsesPreBoundLightingInputs = (Cmd.Pass == ERenderPass::Lighting);
    const bool bUsesPreBoundDecalInputs = (Cmd.Pass == ERenderPass::Decal && Cmd.MeshBuffer == nullptr);

    if (bUsesPreBoundDecalInputs)
    {
        if (bForce || Cmd.DiffuseSRV != Cache.DiffuseSRV)
        {
            ID3D11ShaderResourceView* DecalSRV = Cmd.DiffuseSRV;
            Ctx->PSSetShaderResources(0, 1, &DecalSRV);
            Cache.DiffuseSRV = Cmd.DiffuseSRV;
        }
    }
    else if (!bUsesPreBoundLightingInputs &&
             (bForce || Cmd.DiffuseSRV != Cache.DiffuseSRV || Cmd.NormalSRV != Cache.NormalSRV))
    {
        ID3D11ShaderResourceView* SRVs[2] = { Cmd.DiffuseSRV, Cmd.NormalSRV };
        Ctx->PSSetShaderResources(0, 2, SRVs);
        Cache.DiffuseSRV = Cmd.DiffuseSRV;
        Cache.NormalSRV = Cmd.NormalSRV;
    }
    else if (bUsesPreBoundLightingInputs)
    {
        Cache.DiffuseSRV = Cmd.DiffuseSRV;
        Cache.NormalSRV = Cmd.NormalSRV;
    }

    Cache.bForceAll = false;

    if (Cmd.IndexCount > 0)
    {
        Ctx->DrawIndexed(Cmd.IndexCount, Cmd.FirstIndex, Cmd.BaseVertex);
    }
    else if (Cmd.VertexCount > 0)
    {
        Ctx->Draw(Cmd.VertexCount, 0);
    }
    else if (Cmd.MeshBuffer)
    {
        const uint32 IdxCount = Cmd.MeshBuffer->GetIndexBuffer().GetIndexCount();
        if (IdxCount > 0)
            Ctx->DrawIndexed(IdxCount, 0, 0);
        else
            Ctx->Draw(Cmd.MeshBuffer->GetVertexBuffer().GetVertexCount(), 0);
    }

    FDrawCallStats::Increment();
}
