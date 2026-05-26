#include "ParticleSystemSceneProxy.h"
#include "Component/Primitive/ParticleSystemComponent.h"
#include "Render/Command/DrawCommandList.h"
#include "Render/Command/DrawCommand.h"
#include "Render/Shader/ShaderManager.h"
#include "Render/Types/FrameContext.h"
#include "Render/Types/VertexTypes.h"
#include "Materials/Material.h"
#include "Materials/MaterialManager.h"
#include "Core/Logging/Log.h"
#include "Particles/ParticleHelper.h"
#include "Engine/Profiling/Stats/ParticleStats.h"
#include "Object/Object.h"
#include <cstring>


struct FParticleFrameConstants
{
	FVector CameraRight; float _pad0;
	FVector CameraUp;    float _pad1;
};

// EParticleBlendMode → Pass / BlendState / DepthStencil 결정
struct FParticleRenderState
{
	ERenderPass         Pass;
	EBlendState         Blend;
	EDepthStencilState  DepthStencil;
};

static FParticleRenderState ResolveParticleRenderState(EParticleBlendMode BlendMode)
{
	switch (BlendMode)
	{
	case EParticleBlendMode::Additive:
		// 가산 합성 — 뒤 색상에 더해지므로 소팅 불필요, 뎁스 쓰기 금지
		return { ERenderPass::AlphaBlend, EBlendState::Additive, EDepthStencilState::DepthReadOnly };

	case EParticleBlendMode::AlphaBlend:
	case EParticleBlendMode::Translucent:
	default:
		// 반투명 — 뎁스 쓰기 금지, back-to-front 소팅 필요
		return { ERenderPass::AlphaBlend, EBlendState::AlphaBlend, EDepthStencilState::DepthReadOnly };
	}
}


FParticleSystemSceneProxy::FParticleSystemSceneProxy(UParticleSystemComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent)
{
	ProxyFlags |= EPrimitiveProxyFlags::PerViewportUpdate
	            | EPrimitiveProxyFlags::Particle
	            | EPrimitiveProxyFlags::ShowAABB; // bBoundingVolume 토글 시 AABB 표시 자격.
	ProxyFlags &= ~EPrimitiveProxyFlags::SupportsOutline;
}


FParticleSystemSceneProxy::~FParticleSystemSceneProxy()
{
	CachedEmitterData.clear();
	CachedEmitterCount = 0;
	EmitterBuffers.clear();
}


void FParticleSystemSceneProxy::UpdateLOD(uint32 LODLevel)
{
	// 엔진이 계산한 LOD를 저장만 함
	CurrentLOD = LODLevel;
}


void FParticleSystemSceneProxy::UpdatePerViewport(const FFrameContext& Frame)
{
	if (!bVisible)
	{
		CachedEmitterData.clear();
		CachedEmitterCount = 0;
		return;
	}

	UParticleSystemComponent* Comp = static_cast<UParticleSystemComponent*>(GetOwner());
	if (!IsValid(Comp))
	{
		CachedEmitterData.clear();
		CachedEmitterCount = 0;
		bVisible = false;
		return;
	}

	float DistToCamera = FVector::Distance(Frame.CameraPosition, Comp->GetWorldLocation());
	Comp->SetCachedDistanceToCamera(DistToCamera);

	const TArray<FDynamicEmitterDataBase*>& EmitterList = Comp->GetEmitterRenderData();
	CachedEmitterData.clear();
	CachedEmitterData.reserve(EmitterList.size());

	for (FDynamicEmitterDataBase* EmitterData : EmitterList)
	{
		if (EmitterData)
		{
			CachedEmitterData.push_back(EmitterData);
		}
	}

	CachedEmitterCount = static_cast<int32>(CachedEmitterData.size());

	const FParticleSortContext SortCtx { Frame.CameraPosition, Frame.CameraForward };
	for (FDynamicEmitterDataBase* EmitterData : CachedEmitterData)
	{
		if (!EmitterData)
		{
			continue;
		}

		const FDynamicEmitterReplayDataBase& Source = EmitterData->GetSource();

		if (Source.eEmitterType == EDynamicEmitterType::Sprite ||
			Source.eEmitterType == EDynamicEmitterType::Mesh)
		{
			static_cast<FDynamicSpriteEmitterDataBase*>(EmitterData)->SortSpriteParticles(SortCtx);
		}
	}
}


void FParticleSystemSceneProxy::BuildParticleCommands(
	ID3D11Device* Device, ID3D11DeviceContext* Context,
	const FFrameContext& Frame, FDrawCommandList& OutCmdList)
{
	if (CachedEmitterCount <= 0) return;

	PARTICLE_STATS_RESET();

	// QuadVB/IB 최초 1회 생성
	if (!QuadVB.GetBuffer())
		BuildQuadGeometry(Device);

	// 에미터 수에 맞게 GPU 버퍼 확보
	EnsureEmitterBuffers(Device, CachedEmitterCount);

	// CPU 스테이징 채우기 — 매 프레임 여기서만 1회 실행
	{
		SCOPE_STAT_CAT("ParticleStagingFill", "Particle");
		for (int32 i = 0; i < CachedEmitterCount; ++i)
		{
			if (CachedEmitterData[i] && EmitterBuffers[i])
				FillStagingBuffer(*CachedEmitterData[i], *EmitterBuffers[i], Frame);
		}
	}

	// GPU 업로드 + 드로우 커맨드 생성
	int32 SubmittedCount = 0;
	for (auto& BufferPtr : EmitterBuffers)
	{
		if (!BufferPtr || BufferPtr->ActiveParticleCount <= 0) continue;
		SubmitEmitter(*BufferPtr, Device, Context, Frame, OutCmdList);
		++SubmittedCount;
	}

	if (SubmittedCount == 0)
	{
		UE_LOG("[ParticleProxy] BuildParticleCommands: %d emitter(s) cached but none had active particles", CachedEmitterCount);
	}
}


void FParticleSystemSceneProxy::BuildQuadGeometry(ID3D11Device* Device)
{
	FParticleQuadVertex Verts[4] = {
		{ FVector2(-0.5f, -0.5f) },
		{ FVector2( 0.5f, -0.5f) },
		{ FVector2(-0.5f,  0.5f) },
		{ FVector2( 0.5f,  0.5f) },
	};
	QuadVB.Create(Device, Verts, 4, sizeof(Verts), sizeof(FParticleQuadVertex));

	uint32 Indices[6] = { 0, 1, 2, 2, 1, 3 };
	QuadIB.Create(Device, Indices, 6, sizeof(Indices));

	if (!QuadVB.GetBuffer() || !QuadIB.GetBuffer())
		UE_LOG("[ParticleProxy] BuildQuadGeometry: Failed to create quad VB or IB");
}


void FParticleSystemSceneProxy::EnsureEmitterBuffers(ID3D11Device* Device, int32 EmitterCount)
{
	for (int32 i = 0; i < EmitterCount && i < static_cast<int32>(EmitterBuffers.size()); ++i)
	{
		if (!EmitterBuffers[i] || i >= static_cast<int32>(CachedEmitterData.size()) || !CachedEmitterData[i])
		{
			continue;
		}

		const uint32 RequiredStride = static_cast<uint32>(CachedEmitterData[i]->GetDynamicVertexStride());
		if (EmitterBuffers[i]->InstanceVB.GetStride() != RequiredStride)
		{
			EmitterBuffers[i]->InstanceVB.Create(Device, 64, RequiredStride);
		}
	}

	const int32 Current = static_cast<int32>(EmitterBuffers.size());
	if (Current >= EmitterCount) return;

	for (int32 i = Current; i < EmitterCount; ++i)
	{
		const uint32 InstanceStride =
			(i < static_cast<int32>(CachedEmitterData.size()) && CachedEmitterData[i])
			? static_cast<uint32>(CachedEmitterData[i]->GetDynamicVertexStride())
			: sizeof(FParticleSpriteInstance);

		auto Buf = std::make_unique<FEmitterRenderBuffer>();
		Buf->InstanceVB.Create(Device, 64, InstanceStride);
		Buf->ParticleFrameCB.Create(Device, sizeof(FParticleFrameConstants), "ParticleFrameCB");
		EmitterBuffers.push_back(std::move(Buf));
	}
}


void FParticleSystemSceneProxy::FillStagingBuffer(
	FDynamicEmitterDataBase& EmitterData, FEmitterRenderBuffer& OutBuffer, const FFrameContext& Frame)
{
	const FDynamicEmitterReplayDataBase& Source = EmitterData.GetSource();
	const int32 Stride = EmitterData.GetDynamicVertexStride();
	int32 Count = Source.ActiveParticleCount;
	if (Source.MaxDrawCount >= 0 && Source.MaxDrawCount < Count)
	{
		Count = Source.MaxDrawCount;
	}

	OutBuffer.ActiveParticleCount = Count;
	OutBuffer.DynamicVertexCount  = 0;
	OutBuffer.DynamicIndexCount   = 0;
	OutBuffer.EmitterType         = Source.eEmitterType;
	OutBuffer.BlendMode           = Source.BlendMode;
	OutBuffer.Material            = nullptr;
	OutBuffer.EmitterMeshBuffer   = nullptr;
	OutBuffer.MeshSectionMaterials.clear();
	OutBuffer.MeshSectionFirstIndices.clear();
	OutBuffer.MeshSectionIndexCounts.clear();
	OutBuffer.StagingIndices.clear();
	OutBuffer.StagingBuffer.resize(Count * Stride);

	if (Source.eEmitterType == EDynamicEmitterType::Sprite
	 || Source.eEmitterType == EDynamicEmitterType::Mesh
	 || Source.eEmitterType == EDynamicEmitterType::Beam
	 || Source.eEmitterType == EDynamicEmitterType::Ribbon)
	{
		const auto& SpriteSource =
			static_cast<const FDynamicSpriteEmitterReplayDataBase&>(Source);
		OutBuffer.Material = SpriteSource.Material;

		if (!OutBuffer.Material)
			UE_LOG("[ParticleProxy] FillStagingBuffer: Material is null (emitter type=%d)", (int)Source.eEmitterType);

		if (Source.eEmitterType == EDynamicEmitterType::Mesh)
		{
			const FDynamicMeshEmitterData& MeshEmitterData = static_cast<const FDynamicMeshEmitterData&>(EmitterData);
			const FDynamicMeshEmitterReplayData& MeshSource = MeshEmitterData.Source;
			OutBuffer.EmitterMeshBuffer = MeshEmitterData.MeshBuffer;
			OutBuffer.MeshSectionMaterials = MeshSource.SectionMaterials;
			OutBuffer.MeshSectionFirstIndices = MeshSource.SectionFirstIndices;
			OutBuffer.MeshSectionIndexCounts = MeshSource.SectionIndexCounts;

			if (!OutBuffer.EmitterMeshBuffer)
				UE_LOG("[ParticleProxy] FillStagingBuffer: MeshBuffer is null on Mesh emitter");
		}
	}

	if (Source.eEmitterType == EDynamicEmitterType::Beam
	 || Source.eEmitterType == EDynamicEmitterType::Ribbon)
	{
		const TArray<FParticleBeamTrailVertex>* BuiltVertices = nullptr;
		const TArray<uint32>* BuiltIndices = nullptr;

		if (Source.eEmitterType == EDynamicEmitterType::Beam)
		{
			auto& BeamData = static_cast<FDynamicBeam2EmitterData&>(EmitterData);
			BeamData.BuildMeshData(Frame);
			BuiltVertices = &BeamData.GetBuiltVertices();
			BuiltIndices = &BeamData.GetBuiltIndices();
		}
		else
		{
			const auto& RibbonData = static_cast<const FDynamicRibbonEmitterData&>(EmitterData);
			BuiltVertices = &RibbonData.GetBuiltVertices();
			BuiltIndices = &RibbonData.GetBuiltIndices();
		}

		const int32 VertexCount = BuiltVertices ? static_cast<int32>(BuiltVertices->size()) : 0;
		const int32 IndexCount = BuiltIndices ? static_cast<int32>(BuiltIndices->size()) : 0;
		OutBuffer.ActiveParticleCount = Source.ActiveParticleCount;
		OutBuffer.DynamicVertexCount = VertexCount;
		OutBuffer.DynamicIndexCount = IndexCount;
		OutBuffer.StagingBuffer.resize(VertexCount * Stride);
		OutBuffer.StagingIndices = BuiltIndices ? *BuiltIndices : TArray<uint32>();

		if (VertexCount > 0)
		{
			memcpy(OutBuffer.StagingBuffer.data(), BuiltVertices->data(), VertexCount * Stride);
		}
		return;
	}

	if (Count == 0) return;

	if (Source.eEmitterType == EDynamicEmitterType::Sprite)
		PARTICLE_STATS_ADD_SPRITE_PARTICLES(static_cast<uint32>(Count));
	else if (Source.eEmitterType == EDynamicEmitterType::Mesh)
		PARTICLE_STATS_ADD_MESH_PARTICLES(static_cast<uint32>(Count));

	if (!Source.DataContainer.ParticleData)
	{
		UE_LOG("[ParticleProxy] FillStagingBuffer: ParticleData is null but ActiveParticleCount=%d", Count);
		return;
	}

	if (Source.eEmitterType == EDynamicEmitterType::Sprite)
	{
		for (int32 i = 0; i < Count; ++i)
		{
			const uint32 Idx = Source.DataContainer.ParticleIndices
				? Source.DataContainer.ParticleIndices[i]
				: static_cast<uint32>(i);

			const FBaseParticle* P = reinterpret_cast<const FBaseParticle*>(
			    Source.DataContainer.ParticleData + Idx * Source.ParticleStride);
			FParticleSpriteInstance* Inst = reinterpret_cast<FParticleSpriteInstance*>(
			    OutBuffer.StagingBuffer.data() + i * Stride);
			Inst->Position = P->Location;
			Inst->Size     = P->Size.X * Source.Scale.X;
			Inst->Color    = P->Color.ToVector4();
			Inst->Rotation = P->Rotation;
			// 라이프타임 진행도를 그대로 흘려보냄. 머티리얼 그래프의 ParticleSubUV가 Rows/Cols로 정수 프레임 변환.
			Inst->SubImageIndex = P->RelativeTime;
			// 모듈이 아직 없으므로 기본값. 0이어야 `pow(x, DP.r + 1)` 같은 패턴에서 자연스러움.
			Inst->DynamicParam = FVector4(0.0f, 0.0f, 0.0f, 0.0f);
		}
	}
	else if (Source.eEmitterType == EDynamicEmitterType::Mesh)
	{
		// FDynamicMeshEmitterReplayData에 있는 MeshRotationOffset을 꺼낸다.
		const int32 MeshRotOffset =
			static_cast<const FDynamicMeshEmitterReplayData&>(Source).MeshRotationOffset;

		for (int32 i = 0; i < Count; ++i)
		{
			const uint32 Idx = Source.DataContainer.ParticleIndices
				? Source.DataContainer.ParticleIndices[i]
				: static_cast<uint32>(i);

			const FBaseParticle* P = reinterpret_cast<const FBaseParticle*>(
				Source.DataContainer.ParticleData + Idx * Source.ParticleStride);
			FMeshParticleInstanceVertex* Inst = reinterpret_cast<FMeshParticleInstanceVertex*>(
				OutBuffer.StagingBuffer.data() + i * Stride);

			// 회전: MeshRotationOffset > 0이면 FMeshRotationPayloadData에서 읽고,
			// 미설정(0)이면 회전 없이 스케일+위치만 적용.
			FVector Euler = FVector::ZeroVector;
			if (MeshRotOffset > 0)
			{
				const FMeshRotationPayloadData* RotPayload =
					reinterpret_cast<const FMeshRotationPayloadData*>(
						reinterpret_cast<const uint8*>(P) + MeshRotOffset);
				Euler = RotPayload->Rotation;
			}

			// 스케일: 파티클 크기 × 에미터 스케일
			const FVector Scale(
				P->Size.X * Source.Scale.X,
				P->Size.Y * Source.Scale.Y,
				P->Size.Z * Source.Scale.Z);

			// SRT 순서로 월드 트랜스폼 구성
			FMatrix WorldTM = FMatrix::MakeScaleMatrix(Scale) * FMatrix::MakeRotationEuler(Euler);
			WorldTM.SetLocation(P->Location);

			Inst->Transform = WorldTM;
			Inst->Color     = P->Color.ToVector4();
			Inst->SubImageIndex = P->RelativeTime;
			Inst->DynamicParam  = FVector4(0.0f, 0.0f, 0.0f, 0.0f);
		}
	}
}


void FParticleSystemSceneProxy::SubmitEmitter(
	FEmitterRenderBuffer& Buffer,
	ID3D11Device* Device, ID3D11DeviceContext* Context,
	const FFrameContext& Frame, FDrawCommandList& OutCmdList)
{
	switch (Buffer.EmitterType)
	{
	case EDynamicEmitterType::Sprite:
		SubmitSpriteEmitter(Buffer, Device, Context, Frame, OutCmdList);
		break;
	case EDynamicEmitterType::Mesh:
		SubmitMeshEmitter(Buffer, Device, Context, Frame, OutCmdList);
		break;
	case EDynamicEmitterType::Ribbon:
	case EDynamicEmitterType::Beam:
		SubmitBeamTrailEmitter(Buffer, Device, Context, Frame, OutCmdList);
		break;
	}
}


void FParticleSystemSceneProxy::SubmitSpriteEmitter(
	FEmitterRenderBuffer& Buffer,
	ID3D11Device* Device, ID3D11DeviceContext* Context,
	const FFrameContext& Frame, FDrawCommandList& OutCmdList)
{
	if (!QuadVB.GetBuffer() || !QuadIB.GetBuffer())
	{
		UE_LOG("[ParticleProxy] SubmitSpriteEmitter: QuadVB or QuadIB is null");
		return;
	}

	Buffer.InstanceVB.EnsureCapacity(Device, static_cast<uint32>(Buffer.ActiveParticleCount));

	if (!Buffer.InstanceVB.Update(Context, Buffer.StagingBuffer.data(),
		static_cast<uint32>(Buffer.ActiveParticleCount)))
	{
		UE_LOG("[ParticleProxy] SubmitSpriteEmitter: InstanceVB upload failed (count=%d)", Buffer.ActiveParticleCount);
		return;
	}

	FParticleFrameConstants FrameCB;
	FrameCB.CameraRight = Frame.CameraRight; FrameCB._pad0 = 0.0f;
	FrameCB.CameraUp    = Frame.CameraUp;    FrameCB._pad1 = 0.0f;
	Buffer.ParticleFrameCB.Update(Context, &FrameCB, sizeof(FParticleFrameConstants));

    FShader* Shader = Buffer.Material && Buffer.Material->GetShader() ? Buffer.Material->GetShader()
    : FShaderManager::Get().GetOrCreate(EShaderPath::ParticleSprite);
	if (!Shader)
	{
		UE_LOG("[ParticleProxy] SubmitSpriteEmitter: ParticleSprite shader not found (%s)", EShaderPath::ParticleSprite);
		return;
	}

	const FParticleRenderState RS = ResolveParticleRenderState(Buffer.BlendMode);

	FDrawCommand& Cmd                  = OutCmdList.AddCommand();
	Cmd.Shader                         = Shader;
    if (Buffer.Material)
    {
        Cmd.Pass                     = Buffer.Material->GetRenderPass();
        Cmd.RenderState.Blend        = Buffer.Material->GetBlendState();
        Cmd.RenderState.DepthStencil = Buffer.Material->GetDepthStencilState();
        Cmd.RenderState.Rasterizer   = Buffer.Material->GetRasterizerState();
    }
    else
    {
        Cmd.Pass                     = RS.Pass;
        Cmd.RenderState.Blend        = RS.Blend;
        Cmd.RenderState.DepthStencil = RS.DepthStencil;
        Cmd.RenderState.Rasterizer   = ERasterizerState::SolidNoCull; // 빌보드는 항상 양면
    }

	Cmd.Buffer.VB             = QuadVB.GetBuffer();
	Cmd.Buffer.VBStride       = sizeof(FParticleQuadVertex);
	Cmd.Buffer.IB             = QuadIB.GetBuffer();
	Cmd.Buffer.IndexCount     = 6;
	Cmd.Buffer.InstanceVB     = Buffer.InstanceVB.GetBuffer();
	Cmd.Buffer.InstanceStride = sizeof(FParticleSpriteInstance);
	Cmd.Buffer.InstanceCount  = static_cast<uint32>(Buffer.ActiveParticleCount);

	if (Buffer.Material)
    {
        Buffer.Material->FlushDirtyBuffers(Device, Context);

        // ParticleSprite 빌보드 확장은 카메라 축이 필요. ParticleFrameCB(b2)는 반드시 묶고,
        // 머티리얼 파라미터 PerMaterial은 b3로 분리한다(generator도 ParticleSprite 도메인에서 PerMaterial을 b3로 emit).
        Cmd.Bindings.PerShaderCB[0] = &Buffer.ParticleFrameCB;
        Cmd.Bindings.PerShaderCB[1] = Buffer.Material->GetGPUBufferBySlot(ECBSlot::PerShader1);

        const ID3D11ShaderResourceView* const* MatSRVs       = Buffer.Material->GetCachedSRVs();
        ID3D11ShaderResourceView*              FallbackWhite = FMaterialManager::Get().GetFallbackWhiteSRV();

        for (int32 Slot = 0; Slot < static_cast<int32>(EMaterialTextureSlot::Max); ++Slot)
        {
            // null이면 1x1 흰색 → 셰이더가 sample 시 (1,1,1,1) 받아 alpha-clip 회피.
            Cmd.Bindings.SRVs[Slot] = MatSRVs[Slot] ? const_cast<ID3D11ShaderResourceView*>(MatSRVs[Slot])
            : FallbackWhite;
        }
    }
    else
    {
        Cmd.Bindings.PerShaderCB[0] = &Buffer.ParticleFrameCB;
    }

	Cmd.BuildSortKey();
	PARTICLE_STATS_ADD_DRAW_CALL();
}


void FParticleSystemSceneProxy::SubmitBeamTrailEmitter(
	FEmitterRenderBuffer& Buffer,
	ID3D11Device* Device, ID3D11DeviceContext* Context,
	const FFrameContext& Frame, FDrawCommandList& OutCmdList)
{
	if (Buffer.DynamicVertexCount <= 0 || Buffer.DynamicIndexCount <= 0)
	{
		return;
	}

	Buffer.InstanceVB.EnsureCapacity(Device, static_cast<uint32>(Buffer.DynamicVertexCount));
	Buffer.DynamicIB.EnsureCapacity(Device, static_cast<uint32>(Buffer.DynamicIndexCount));

	if (!Buffer.InstanceVB.Update(Context, Buffer.StagingBuffer.data(),
		static_cast<uint32>(Buffer.DynamicVertexCount)))
	{
		UE_LOG("[ParticleProxy] SubmitBeamTrailEmitter: vertex upload failed (vertices=%d)", Buffer.DynamicVertexCount);
		return;
	}

	if (!Buffer.DynamicIB.Update(Context, Buffer.StagingIndices.data(),
		static_cast<uint32>(Buffer.DynamicIndexCount)))
	{
		UE_LOG("[ParticleProxy] SubmitBeamTrailEmitter: index upload failed (indices=%d)", Buffer.DynamicIndexCount);
		return;
	}

	FShader* Shader = FShaderManager::Get().GetOrCreate(EShaderPath::ParticleBeamTrail);
	if (!Shader)
	{
		UE_LOG("[ParticleProxy] SubmitBeamTrailEmitter: ParticleBeamTrail shader not found (%s)", EShaderPath::ParticleBeamTrail);
		return;
	}

	const FParticleRenderState RS = ResolveParticleRenderState(Buffer.BlendMode);

	FDrawCommand& Cmd = OutCmdList.AddCommand();
	Cmd.Shader = Shader;
	if (Buffer.Material)
	{
		Cmd.Pass                     = Buffer.Material->GetRenderPass();
		Cmd.RenderState.Blend        = Buffer.Material->GetBlendState();
		Cmd.RenderState.DepthStencil = Buffer.Material->GetDepthStencilState();
		Cmd.RenderState.Rasterizer   = Buffer.Material->GetRasterizerState();
	}
	else
	{
		Cmd.Pass                     = RS.Pass;
		Cmd.RenderState.Blend        = RS.Blend;
		Cmd.RenderState.DepthStencil = RS.DepthStencil;
		Cmd.RenderState.Rasterizer   = ERasterizerState::SolidNoCull;
	}

	Cmd.Buffer.VB         = Buffer.InstanceVB.GetBuffer();
	Cmd.Buffer.VBStride   = sizeof(FParticleBeamTrailVertex);
	Cmd.Buffer.IB         = Buffer.DynamicIB.GetBuffer();
	Cmd.Buffer.IndexCount = static_cast<uint32>(Buffer.DynamicIndexCount);

	if (Buffer.Material)
	{
		Buffer.Material->FlushDirtyBuffers(Device, Context);

		Cmd.Bindings.PerShaderCB[0] = Buffer.Material->GetGPUBufferBySlot(ECBSlot::PerShader0);
		Cmd.Bindings.PerShaderCB[1] = Buffer.Material->GetGPUBufferBySlot(ECBSlot::PerShader1);

		const ID3D11ShaderResourceView* const* MatSRVs = Buffer.Material->GetCachedSRVs();
		ID3D11ShaderResourceView* FallbackWhite = FMaterialManager::Get().GetFallbackWhiteSRV();

		for (int32 Slot = 0; Slot < static_cast<int32>(EMaterialTextureSlot::Max); ++Slot)
		{
			Cmd.Bindings.SRVs[Slot] = MatSRVs[Slot] ? const_cast<ID3D11ShaderResourceView*>(MatSRVs[Slot])
				: FallbackWhite;
		}
	}

	Cmd.BuildSortKey();
	PARTICLE_STATS_ADD_DRAW_CALL();
}


void FParticleSystemSceneProxy::SubmitMeshEmitter(
	FEmitterRenderBuffer& Buffer,
	ID3D11Device* Device, ID3D11DeviceContext* Context,
	const FFrameContext& Frame, FDrawCommandList& OutCmdList)
{
	if (!Buffer.EmitterMeshBuffer)
	{
		UE_LOG("[ParticleProxy] SubmitMeshEmitter: EmitterMeshBuffer is null");
		return;
	}
	if (!Buffer.EmitterMeshBuffer->IsValid())
	{
		UE_LOG("[ParticleProxy] SubmitMeshEmitter: EmitterMeshBuffer is invalid (VB may not be created)");
		return;
	}

	Buffer.InstanceVB.EnsureCapacity(Device, static_cast<uint32>(Buffer.ActiveParticleCount));

	if (!Buffer.InstanceVB.Update(Context, Buffer.StagingBuffer.data(),
		static_cast<uint32>(Buffer.ActiveParticleCount)))
	{
		UE_LOG("[ParticleProxy] SubmitMeshEmitter: InstanceVB upload failed (count=%d)", Buffer.ActiveParticleCount);
		return;
	}

	const FParticleRenderState RS = ResolveParticleRenderState(Buffer.BlendMode);

	const uint32 TotalIndexCount = Buffer.EmitterMeshBuffer->GetIndexBuffer().GetIndexCount();
	const int32 SectionCount = std::min(
		static_cast<int32>(Buffer.MeshSectionFirstIndices.size()),
		static_cast<int32>(Buffer.MeshSectionIndexCounts.size()));
	const int32 DrawCount = SectionCount > 0 ? SectionCount : 1;

	for (int32 DrawIdx = 0; DrawIdx < DrawCount; ++DrawIdx)
	{
		UMaterial* SectionMaterial = Buffer.Material;
		if (DrawIdx < static_cast<int32>(Buffer.MeshSectionMaterials.size()) && Buffer.MeshSectionMaterials[DrawIdx])
		{
			SectionMaterial = Buffer.MeshSectionMaterials[DrawIdx];
		}

		FShader* Shader = SectionMaterial && SectionMaterial->GetShader()
			? SectionMaterial->GetShader()
			: FShaderManager::Get().GetOrCreate(EShaderPath::ParticleMesh);
		if (!Shader)
		{
			continue;
		}

		FDrawCommand& Cmd = OutCmdList.AddCommand();
		Cmd.Shader = Shader;
		if (SectionMaterial)
		{
			Cmd.Pass                     = SectionMaterial->GetRenderPass();
			Cmd.RenderState.Blend        = SectionMaterial->GetBlendState();
			Cmd.RenderState.DepthStencil = SectionMaterial->GetDepthStencilState();
			Cmd.RenderState.Rasterizer   = SectionMaterial->GetRasterizerState();
		}
		else
		{
			Cmd.Pass                     = RS.Pass;
			Cmd.RenderState.Blend        = RS.Blend;
			Cmd.RenderState.DepthStencil = RS.DepthStencil;
			Cmd.RenderState.Rasterizer   = ERasterizerState::SolidNoCull;
		}

		Cmd.Buffer.VB             = Buffer.EmitterMeshBuffer->GetVertexBuffer().GetBuffer();
		Cmd.Buffer.VBStride       = Buffer.EmitterMeshBuffer->GetVertexBuffer().GetStride();
		Cmd.Buffer.IB             = Buffer.EmitterMeshBuffer->GetIndexBuffer().GetBuffer();
		Cmd.Buffer.FirstIndex     = (SectionCount > 0) ? Buffer.MeshSectionFirstIndices[DrawIdx] : 0;
		Cmd.Buffer.IndexCount     = (SectionCount > 0) ? Buffer.MeshSectionIndexCounts[DrawIdx] : TotalIndexCount;
		Cmd.Buffer.InstanceVB     = Buffer.InstanceVB.GetBuffer();
		Cmd.Buffer.InstanceStride = sizeof(FMeshParticleInstanceVertex);
		Cmd.Buffer.InstanceCount  = static_cast<uint32>(Buffer.ActiveParticleCount);

		if (SectionMaterial)
		{
			SectionMaterial->FlushDirtyBuffers(Device, Context);
			Cmd.Bindings.PerShaderCB[0] = SectionMaterial->GetGPUBufferBySlot(ECBSlot::PerShader0);
			Cmd.Bindings.PerShaderCB[1] = SectionMaterial->GetGPUBufferBySlot(ECBSlot::PerShader1);

			const ID3D11ShaderResourceView* const* MatSRVs = SectionMaterial->GetCachedSRVs();
			ID3D11ShaderResourceView* FallbackWhite = FMaterialManager::Get().GetFallbackWhiteSRV();
			for (int32 Slot = 0; Slot < static_cast<int32>(EMaterialTextureSlot::Max); ++Slot)
			{
				Cmd.Bindings.SRVs[Slot] = MatSRVs[Slot]
					? const_cast<ID3D11ShaderResourceView*>(MatSRVs[Slot])
					: FallbackWhite;
			}
		}

		Cmd.BuildSortKey();
		PARTICLE_STATS_ADD_DRAW_CALL();
	}
}
