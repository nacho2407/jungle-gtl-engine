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
	Shader       = nullptr;
	DepthStencil = static_cast<EDepthStencilState>(~0u);
	Blend        = static_cast<EBlendState>(~0u);
	Rasterizer   = static_cast<ERasterizerState>(~0u);
	Topology     = static_cast<D3D11_PRIMITIVE_TOPOLOGY>(~0u);
	StencilRef   = 0xFF;
	MeshBuffer   = nullptr;
	RawVB        = nullptr;
	RawIB        = nullptr;
	PerObjectCB  = nullptr;
	ExtraCB      = nullptr;
	MaterialCB   = nullptr;
	DiffuseSRV   = reinterpret_cast<ID3D11ShaderResourceView*>(~0ull);
	Sampler      = reinterpret_cast<ID3D11SamplerState*>(~0ull);

	LastUVScroll     = -1;
	LastSectionColor = { -1.0f, -1.0f, -1.0f, -1.0f };

	bReadOnlyDSV = false;
	RTV         = nullptr;
	DSV         = nullptr;
	DSVReadOnly = nullptr;
}

void FStateCache::Cleanup(ID3D11DeviceContext* Ctx)
{
	// DSV 복원 (ReadOnly → 쓰기 가능)
	if (bReadOnlyDSV && RTV)
	{
		Ctx->OMSetRenderTargets(1, &RTV, DSV);
		bReadOnlyDSV = false;
	}

	// SRV 언바인딩
	if (DiffuseSRV != reinterpret_cast<ID3D11ShaderResourceView*>(~0ull)
		&& DiffuseSRV != nullptr)
	{
		ID3D11ShaderResourceView* nullSRV = nullptr;
		Ctx->PSSetShaderResources(0, 1, &nullSRV);
		DiffuseSRV = nullptr;
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
	OutEnd   = PassOffsets[(uint32)Pass + 1];
}

void FDrawCommandList::Submit(FD3DDevice& Device, ID3D11DeviceContext* Ctx,
	ID3D11SamplerState* DefaultSampler)
{
	if (Commands.empty()) return;

	SCOPE_STAT_CAT("DrawCommandList::Submit", "4_ExecutePass");

	FStateCache Cache;
	Cache.Reset();

	for (const FDrawCommand& Cmd : Commands)
	{
		SubmitCommand(Cmd, Device, Ctx, Cache, DefaultSampler);
	}

	Cache.Cleanup(Ctx);
}

void FDrawCommandList::SubmitRange(uint32 StartIdx, uint32 EndIdx, FD3DDevice& Device,
	ID3D11DeviceContext* Ctx, ID3D11SamplerState* DefaultSampler)
{
	if (StartIdx >= EndIdx) return;
	if (EndIdx > Commands.size()) EndIdx = static_cast<uint32>(Commands.size());

	FStateCache Cache;
	Cache.Reset();

	for (uint32 i = StartIdx; i < EndIdx; ++i)
	{
		SubmitCommand(Commands[i], Device, Ctx, Cache, DefaultSampler);
	}

	Cache.Cleanup(Ctx);
}

void FDrawCommandList::SubmitRange(uint32 StartIdx, uint32 EndIdx, FD3DDevice& Device,
	ID3D11DeviceContext* Ctx, FStateCache& Cache, ID3D11SamplerState* DefaultSampler)
{
	if (StartIdx >= EndIdx) return;
	if (EndIdx > Commands.size()) EndIdx = static_cast<uint32>(Commands.size());

	for (uint32 i = StartIdx; i < EndIdx; ++i)
	{
		SubmitCommand(Commands[i], Device, Ctx, Cache, DefaultSampler);
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
	ID3D11DeviceContext* Ctx, FStateCache& Cache,
	ID3D11SamplerState* DefaultSampler)
{
	// --- PSO 상태 ---
	if (Cmd.DepthStencil != Cache.DepthStencil)
	{
		Device.SetDepthStencilState(Cmd.DepthStencil);
		Cache.DepthStencil = Cmd.DepthStencil;
	}

	if (Cmd.Blend != Cache.Blend)
	{
		Device.SetBlendState(Cmd.Blend);
		Cache.Blend = Cmd.Blend;
	}

	if (Cmd.Rasterizer != Cache.Rasterizer)
	{
		Device.SetRasterizerState(Cmd.Rasterizer);
		Cache.Rasterizer = Cmd.Rasterizer;
	}

	if (Cmd.Topology != Cache.Topology)
	{
		Ctx->IASetPrimitiveTopology(Cmd.Topology);
		Cache.Topology = Cmd.Topology;
	}

	// --- DSV Read-Only 전환 (PostProcess에서 SRV + DSV 동시 바인딩) ---
	if (Cmd.bReadOnlyDSV != Cache.bReadOnlyDSV && Cache.RTV)
	{
		ID3D11DepthStencilView* TargetDSV = Cmd.bReadOnlyDSV ? Cache.DSVReadOnly : Cache.DSV;
		Ctx->OMSetRenderTargets(1, &Cache.RTV, TargetDSV);
		Cache.bReadOnlyDSV = Cmd.bReadOnlyDSV;
	}

	// --- Shader ---
	if (Cmd.Shader && Cmd.Shader != Cache.Shader)
	{
		Cmd.Shader->Bind(Ctx);
		Cache.Shader = Cmd.Shader;
	}

	// --- Geometry (VB + IB) ---
	if (Cmd.MeshBuffer)
	{
		// Static MeshBuffer 경로
		if (Cmd.MeshBuffer != Cache.MeshBuffer)
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
		if (Cmd.RawVB != Cache.RawVB)
		{
			uint32 Offset = 0;
			Ctx->IASetVertexBuffers(0, 1, &Cmd.RawVB, &Cmd.RawVBStride, &Offset);
			Cache.RawVB = Cmd.RawVB;
			Cache.MeshBuffer = nullptr;
		}
		if (Cmd.RawIB != Cache.RawIB)
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

	// --- Sampler (s0) ---
	{
		// 커맨드에 명시적 Sampler가 있으면 그것을 사용, 없으면 DefaultSampler
		ID3D11SamplerState* TargetSampler = Cmd.Sampler ? Cmd.Sampler : DefaultSampler;
		if (TargetSampler && TargetSampler != Cache.Sampler)
		{
			Ctx->PSSetSamplers(0, 1, &TargetSampler);
			Cache.Sampler = TargetSampler;
		}
	}

	// --- PerObject CB (b1) ---
	if (Cmd.PerObjectCB && Cmd.PerObjectCB != Cache.PerObjectCB)
	{
		ID3D11Buffer* RawCB = Cmd.PerObjectCB->GetBuffer();
		if (RawCB)
		{
			Ctx->VSSetConstantBuffers(ECBSlot::PerObject, 1, &RawCB);
		}
		Cache.PerObjectCB = Cmd.PerObjectCB;
	}

	// --- Extra CB (b2 등) ---
	if (Cmd.ExtraCB && Cmd.ExtraCB != Cache.ExtraCB)
	{
		ID3D11Buffer* RawCB = Cmd.ExtraCB->GetBuffer();
		if (RawCB)
		{
			Ctx->VSSetConstantBuffers(Cmd.ExtraCBSlot, 1, &RawCB);
			Ctx->PSSetConstantBuffers(Cmd.ExtraCBSlot, 1, &RawCB);
		}
		Cache.ExtraCB = Cmd.ExtraCB;
	}

	// --- Material CB (b4) — 인라인 데이터 기반 업데이트 ---
	if (Cmd.MaterialCB)
	{
		// MaterialCB 슬롯 바인딩 (최초 1회)
		if (Cmd.MaterialCB != Cache.MaterialCB)
		{
			ID3D11Buffer* RawCB = Cmd.MaterialCB->GetBuffer();
			if (RawCB)
			{
				Ctx->VSSetConstantBuffers(ECBSlot::Material, 1, &RawCB);
			}
			Cache.MaterialCB = Cmd.MaterialCB;
		}

		// 데이터 변경 시만 업데이트
		int32 CurUVScroll = static_cast<int32>(Cmd.bIsUVScroll);
		if (CurUVScroll != Cache.LastUVScroll
			|| memcmp(&Cmd.SectionColor, &Cache.LastSectionColor, sizeof(FVector4)) != 0)
		{
			FMaterialConstants MatConstants = {};
			MatConstants.bIsUVScroll = Cmd.bIsUVScroll;
			MatConstants.SectionColor = Cmd.SectionColor;
			Cmd.MaterialCB->Update(Ctx, &MatConstants, sizeof(MatConstants));
			Cache.LastUVScroll = CurUVScroll;
			Cache.LastSectionColor = Cmd.SectionColor;
		}
	}

	// --- Diffuse SRV (t0) ---
	if (Cmd.DiffuseSRV != Cache.DiffuseSRV)
	{
		ID3D11ShaderResourceView* SRV = Cmd.DiffuseSRV;
		Ctx->PSSetShaderResources(0, 1, &SRV);
		Cache.DiffuseSRV = Cmd.DiffuseSRV;
	}

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
