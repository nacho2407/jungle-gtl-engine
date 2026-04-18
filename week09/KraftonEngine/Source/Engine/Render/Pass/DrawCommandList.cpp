#include "DrawCommandList.h"

#include <algorithm>
#include <cstring>
#include "Render/Resource/Shader.h"
#include "Render/Pipeline/RenderConstants.h"
#include "Profiling/Stats.h"

// ============================================================
// FStateCache
// ============================================================

void FStateCache::Reset()
{
	bForceAll = true;

	Shader       = nullptr;
	DepthStencil = {};
	Blend        = {};
	Rasterizer   = {};
	Topology     = {};
	StencilRef   = 0;
	MeshBuffer   = nullptr;
	RawVB        = nullptr;
	RawIB        = nullptr;
	PerObjectCB     = nullptr;
	PerShaderCB[0]  = nullptr;
	PerShaderCB[1]  = nullptr;
	DiffuseSRV   = nullptr;

	RTV         = nullptr;
	DSV         = nullptr;
}

void FStateCache::Cleanup(ID3D11DeviceContext* Ctx)
{
	ID3D11ShaderResourceView* NullSRVs[6] = {};
	Ctx->PSSetShaderResources(0, ARRAYSIZE(NullSRVs), NullSRVs);
	DiffuseSRV = nullptr;
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
	OutEnd   = PassOffsets[(uint32)Pass + 1];
}

void FDrawCommandList::Submit(FD3DDevice& Device, ID3D11DeviceContext* Ctx)
{
	if (Commands.empty()) return;

	SCOPE_STAT_CAT("DrawCommandList::Submit", "4_ExecutePass");

	FStateCache Cache;
	Cache.Reset();

	for (const FDrawCommand& Cmd : Commands)
	{
		SubmitCommand(Cmd, Device, Ctx, Cache);
	}

	Cache.Cleanup(Ctx);
}

void FDrawCommandList::SubmitRange(uint32 StartIdx, uint32 EndIdx, FD3DDevice& Device,
	ID3D11DeviceContext* Ctx)
{
	if (StartIdx >= EndIdx) return;
	if (EndIdx > Commands.size()) EndIdx = static_cast<uint32>(Commands.size());

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
	if (StartIdx >= EndIdx) return;
	if (EndIdx > Commands.size()) EndIdx = static_cast<uint32>(Commands.size());

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

	// --- PSO 상태 ---
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



	// --- Shader ---
	if (Cmd.Shader && (bForce || Cmd.Shader != Cache.Shader))
	{
		Cmd.Shader->Bind(Ctx);
		Cache.Shader = Cmd.Shader;
	}

	// --- Geometry (VB + IB) ---
	if (Cmd.MeshBuffer)
	{
		// Static MeshBuffer 경로
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
					Ctx->IASetIndexBuffer(IB, DXGI_FORMAT_R32_UINT, 0);
			}

			Cache.MeshBuffer = Cmd.MeshBuffer;
			Cache.RawVB = nullptr;
			Cache.RawIB = nullptr;
		}
	}
	else if (Cmd.RawVB)
	{
		// Dynamic Buffer 경로 (LineGeometry, FontGeometry 등)
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
		// SV_VertexID 기반 드로우 — InputLayout + VB 해제
		Ctx->IASetInputLayout(nullptr);
		Ctx->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
		Cache.MeshBuffer = nullptr;
		Cache.RawVB = nullptr;
		Cache.RawIB = nullptr;
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
		if (Cmd.PerShaderCB[i] && (bForce || Cmd.PerShaderCB[i] != Cache.PerShaderCB[i]))
		{
			ID3D11Buffer* RawCB = Cmd.PerShaderCB[i]->GetBuffer();
			if (RawCB)
			{
				uint32 Slot = ECBSlot::PerShader0 + i;
				Ctx->VSSetConstantBuffers(Slot, 1, &RawCB);
				Ctx->PSSetConstantBuffers(Slot, 1, &RawCB);
			}
			Cache.PerShaderCB[i] = Cmd.PerShaderCB[i];
		}
	}

	// --- Diffuse SRV (t0) ---
	if (bForce || Cmd.DiffuseSRV != Cache.DiffuseSRV)
	{
		if (Cmd.DiffuseSRV) {
			ID3D11ShaderResourceView* SRV = Cmd.DiffuseSRV;
			Ctx->PSSetShaderResources(0, 1, &SRV);
			Cache.DiffuseSRV = Cmd.DiffuseSRV;
		}
	}

	Cache.bForceAll = false;

	// --- Draw ---
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
		// IndexCount/VertexCount 모두 0이면 MeshBuffer 전체 드로우
		uint32 IdxCount = Cmd.MeshBuffer->GetIndexBuffer().GetIndexCount();
		if (IdxCount > 0)
			Ctx->DrawIndexed(IdxCount, 0, 0);
		else
			Ctx->Draw(Cmd.MeshBuffer->GetVertexBuffer().GetVertexCount(), 0);
	}

	FDrawCallStats::Increment();
}
