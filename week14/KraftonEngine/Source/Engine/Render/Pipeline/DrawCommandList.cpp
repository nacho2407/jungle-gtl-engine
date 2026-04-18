#include "DrawCommandList.h"

#include <algorithm>
#include <cstring>
#include "Render/Resource/Shader.h"
#include "Render/Resource/RenderResources.h"
#include "Render/Device/D3DDevice.h"
#include "Render/Pipeline/RenderConstants.h"
#include "Profiling/Stats.h"

// ============================================================
// FStateCache
// ============================================================

void FStateCache::Reset()
{
	bForceAll = true;

	Shader      = nullptr;
	RenderState = {};
	Buffer      = {};
	PerObjectCB = nullptr;
	Bindings    = {};

	RTV = nullptr;
	DSV = nullptr;
}

void FStateCache::Cleanup(ID3D11DeviceContext* Ctx)
{
	// t0 ~ t7 SRV 언바인딩
	for (int i = 0; i < (int)EMaterialTextureSlot::Max; i++)
	{
		if (Bindings.SRVs[i])
		{
			ID3D11ShaderResourceView* nullSRV = nullptr;
			Ctx->PSSetShaderResources(i, 1, &nullSRV);
			Bindings.SRVs[i] = nullptr;
		}
	}
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

	// 패스별 오프셋 빌드 — 정렬 후 1회 선형 스캔
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

void FDrawCommandList::Submit(FD3DDevice& Device, FSystemResources& Resources)
{
	if (Commands.empty()) return;

	SCOPE_STAT_CAT("DrawCommandList::Submit", "4_ExecutePass");

	ID3D11DeviceContext* Ctx = Device.GetDeviceContext();
	FStateCache Cache;
	Cache.Reset();

	for (const FDrawCommand& Cmd : Commands)
	{
		SubmitCommand(Cmd, Device, Resources, Ctx, Cache);
	}

	Cache.Cleanup(Ctx);
}

void FDrawCommandList::SubmitRange(uint32 StartIdx, uint32 EndIdx,
	FD3DDevice& Device, FSystemResources& Resources, FStateCache& Cache)
{
	if (StartIdx >= EndIdx) return;
	if (EndIdx > Commands.size()) EndIdx = static_cast<uint32>(Commands.size());

	ID3D11DeviceContext* Ctx = Device.GetDeviceContext();
	for (uint32 i = StartIdx; i < EndIdx; ++i)
	{
		SubmitCommand(Commands[i], Device, Resources, Ctx, Cache);
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

void FDrawCommandList::SubmitCommand(const FDrawCommand& Cmd,
	FD3DDevice& Device, FSystemResources& Resources,
	ID3D11DeviceContext* Ctx, FStateCache& Cache)
{
	const bool bForce = Cache.bForceAll;

	// --- 렌더 상태 ---
	if (bForce || Cmd.RenderState.DepthStencil != Cache.RenderState.DepthStencil)
	{
		Resources.SetDepthStencilState(Device, Cmd.RenderState.DepthStencil);
		Cache.RenderState.DepthStencil = Cmd.RenderState.DepthStencil;
	}

	if (bForce || Cmd.RenderState.Blend != Cache.RenderState.Blend)
	{
		Resources.SetBlendState(Device, Cmd.RenderState.Blend);
		Cache.RenderState.Blend = Cmd.RenderState.Blend;
	}

	if (bForce || Cmd.RenderState.Rasterizer != Cache.RenderState.Rasterizer)
	{
		Resources.SetRasterizerState(Device, Cmd.RenderState.Rasterizer);
		Cache.RenderState.Rasterizer = Cmd.RenderState.Rasterizer;
	}

	if (bForce || Cmd.RenderState.Topology != Cache.RenderState.Topology)
	{
		Ctx->IASetPrimitiveTopology(Cmd.RenderState.Topology);
		Cache.RenderState.Topology = Cmd.RenderState.Topology;
	}

	// --- Shader ---
	if (Cmd.Shader && (bForce || Cmd.Shader != Cache.Shader))
	{
		Cmd.Shader->Bind(Ctx);
		Cache.Shader = Cmd.Shader;
	}

	// PreDepth: PS 언바인딩 — depth만 기록, 셰이딩 스킵
	if (Cmd.Pass == ERenderPass::PreDepth)
	{
		Ctx->PSSetShader(nullptr, nullptr, 0);
		Cache.Shader = nullptr;  // 다음 커맨드에서 PS 재바인딩 보장
	}

	// --- Geometry (VB + IB) ---
	if (Cmd.Buffer.HasBuffers())
	{
		if (bForce || Cmd.Buffer.VB != Cache.Buffer.VB || Cmd.Buffer.VBStride != Cache.Buffer.VBStride)
		{
			uint32 Offset = 0;
			Ctx->IASetVertexBuffers(0, 1, &Cmd.Buffer.VB, &Cmd.Buffer.VBStride, &Offset);
		}
		if (bForce || Cmd.Buffer.IB != Cache.Buffer.IB)
		{
			if (Cmd.Buffer.IB)
				Ctx->IASetIndexBuffer(Cmd.Buffer.IB, DXGI_FORMAT_R32_UINT, 0);
		}
		Cache.Buffer = Cmd.Buffer;
	}
	else if (Cache.Buffer.HasBuffers())
	{
		// SV_VertexID 기반 드로우 — InputLayout + VB 해제
		Ctx->IASetInputLayout(nullptr);
		Ctx->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
		Cache.Buffer = {};
	}

	// --- PerObject CB (b1) ---
	if (Cmd.PerObjectCB && (bForce || Cmd.PerObjectCB != Cache.PerObjectCB))
	{
		ID3D11Buffer* RawCB = Cmd.PerObjectCB->GetBuffer();
		if (RawCB)
		{
			Ctx->VSSetConstantBuffers(ECBSlot::PerObject, 1, &RawCB);
		}
		Cache.PerObjectCB = Cmd.PerObjectCB;
	}

	// --- PerShader CBs (b2, b3) ---
	for (uint32 i = 0; i < 2; ++i)
	{
		if (bForce || Cmd.Bindings.PerShaderCB[i] != Cache.Bindings.PerShaderCB[i])
		{
			uint32 Slot = ECBSlot::PerShader0 + i;
			ID3D11Buffer* RawCB = Cmd.Bindings.PerShaderCB[i] ? Cmd.Bindings.PerShaderCB[i]->GetBuffer() : nullptr;
			Ctx->VSSetConstantBuffers(Slot, 1, &RawCB);
			Ctx->PSSetConstantBuffers(Slot, 1, &RawCB);
			Cache.Bindings.PerShaderCB[i] = Cmd.Bindings.PerShaderCB[i];
		}
	}

	// --- SRV (t0 ~ t7) 바인딩 ---
	for (int i = 0; i < (int)EMaterialTextureSlot::Max; i++)
	{
		if (bForce || Cmd.Bindings.SRVs[i] != Cache.Bindings.SRVs[i])
		{
			ID3D11ShaderResourceView* SRV = Cmd.Bindings.SRVs[i];
			Ctx->VSSetShaderResources(i, 1, &SRV);
			Ctx->PSSetShaderResources(i, 1, &SRV);
			Cache.Bindings.SRVs[i] = Cmd.Bindings.SRVs[i];
		}
	}

	Cache.bForceAll = false;

	// --- Draw ---
	if (Cmd.Buffer.IndexCount > 0)
	{
		Ctx->DrawIndexed(Cmd.Buffer.IndexCount, Cmd.Buffer.FirstIndex, Cmd.Buffer.BaseVertex);
	}
	else if (Cmd.Buffer.VertexCount > 0)
	{
		Ctx->Draw(Cmd.Buffer.VertexCount, 0);
	}

	FDrawCallStats::Increment();
}
