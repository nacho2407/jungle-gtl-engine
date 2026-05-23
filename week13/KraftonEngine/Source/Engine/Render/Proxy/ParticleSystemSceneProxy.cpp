#include "ParticleSystemSceneProxy.h"
#include "Component/Primitive/ParticleSystemComponent.h"
#include "Render/Command/DrawCommandList.h"
#include "Render/Command/DrawCommand.h"
#include "Render/Shader/ShaderManager.h"
#include "Render/Types/FrameContext.h"
#include "Render/Types/VertexTypes.h"
#include "Materials/Material.h"
#include "Core/Logging/Log.h"
#include "Particles/ParticleHelper.h"


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
	            | EPrimitiveProxyFlags::Particle;
	ProxyFlags &= ~(EPrimitiveProxyFlags::SupportsOutline
	              | EPrimitiveProxyFlags::ShowAABB);
}


void FParticleSystemSceneProxy::UpdateLOD(uint32 LODLevel)
{
	// 엔진이 계산한 LOD를 저장 — UpdatePerViewport에서 최종 결정에 사용
	CurrentLOD = LODLevel;

	// TODO: 파티클 전용 LOD 거리(UParticleSystem::LODDistances)가 없을 때
	//       여기서 컴포넌트에 직접 전달하는 경로 추가
	// UParticleSystemComponent* Comp = static_cast<UParticleSystemComponent*>(GetOwner());
	// if (Comp) Comp->SetCurrentLODLevel(static_cast<int32>(LODLevel));
}


void FParticleSystemSceneProxy::UpdatePerViewport(const FFrameContext& Frame)
{
	if (!bVisible) return;

	UParticleSystemComponent* Comp =
		static_cast<UParticleSystemComponent*>(GetOwner());
	if (!Comp)
	{
		UE_LOG("[ParticleProxy] UpdatePerViewport: Owner component is null");
		return;
	}

	// TODO: 파티클 전용 LOD 최종 결정 (카메라 거리 기반)
	// UParticleSystem* Template = Comp->GetTemplate();
	// if (Template && !Template->LODDistances.empty())
	// {
	//     float DistSq = (CachedWorldPos - Frame.CameraPosition).LengthSquared();
	//     int32 LODIndex = ComputeParticleLOD(DistSq, Template->LODDistances);
	//     CurrentLOD = LODIndex;
	//     Comp->SetCurrentLODLevel(LODIndex);
	// }
	// else
	// {
	//     Comp->SetCurrentLODLevel(static_cast<int32>(CurrentLOD));
	// }

	// TODO: 반투명 스프라이트 back-to-front 정렬 (카메라 위치 기준)
	// for (FDynamicEmitterDataBase* EmitterData : CachedEmitterData)
	// {
	//     if (EmitterData) EmitterData->SortSpriteParticles(Frame.CameraPosition);
	// }

	// GetEmitterRenderData(): 컴포넌트가 매 틱 갱신한 동적 에미터 데이터
	const TArray<FDynamicEmitterDataBase*>& EmitterList = Comp->GetEmitterRenderData();

	CachedEmitterCount = static_cast<int32>(EmitterList.size());
	CachedEmitterData  = EmitterList;  // 포인터 배열만 복사 (소유권은 컴포넌트)

	for (int32 i = 0; i < CachedEmitterCount; ++i)
	{
		if (!CachedEmitterData[i])
		{
			UE_LOG("[ParticleProxy] UpdatePerViewport: EmitterData[%d] is null, skip", i);
			continue;
		}
		if (i < static_cast<int32>(EmitterBuffers.size()) && EmitterBuffers[i])
			FillStagingBuffer(*CachedEmitterData[i], *EmitterBuffers[i]);
	}
}


void FParticleSystemSceneProxy::BuildParticleCommands(
	ID3D11Device* Device, ID3D11DeviceContext* Context,
	const FFrameContext& Frame, FDrawCommandList& OutCmdList)
{
	if (CachedEmitterCount <= 0) return;

	// QuadVB/IB 최초 1회 생성
	if (!QuadVB.GetBuffer())
		BuildQuadGeometry(Device);

	// 에미터 수에 맞게 GPU 버퍼 확보
	EnsureEmitterBuffers(Device, CachedEmitterCount);

	// UpdatePerViewport 미실행 시 방어적으로 스테이징 채움
	for (int32 i = 0; i < CachedEmitterCount; ++i)
	{
		if (CachedEmitterData[i] && EmitterBuffers[i])
			FillStagingBuffer(*CachedEmitterData[i], *EmitterBuffers[i]);
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
	const int32 Current = static_cast<int32>(EmitterBuffers.size());
	if (Current >= EmitterCount) return;

	for (int32 i = Current; i < EmitterCount; ++i)
	{
		auto Buf = std::make_unique<FEmitterRenderBuffer>();
		Buf->InstanceVB.Create(Device, 64, sizeof(FParticleSpriteInstance));
		Buf->ParticleFrameCB.Create(Device, sizeof(FParticleFrameConstants), "ParticleFrameCB");
		EmitterBuffers.push_back(std::move(Buf));
	}
}


void FParticleSystemSceneProxy::FillStagingBuffer(
	const FDynamicEmitterDataBase& EmitterData, FEmitterRenderBuffer& OutBuffer)
{
	const FDynamicEmitterReplayDataBase& Source = EmitterData.GetSource();
	const int32 Stride = EmitterData.GetDynamicVertexStride();
	const int32 Count  = Source.ActiveParticleCount;

	OutBuffer.ActiveParticleCount = Count;
	OutBuffer.EmitterType         = Source.eEmitterType;
	OutBuffer.BlendMode           = Source.BlendMode;
	OutBuffer.StagingBuffer.resize(Count * Stride);

	// 메타 캐싱 (Material, MeshBuffer)
	if (Source.eEmitterType == EDynamicEmitterType::Sprite
	 || Source.eEmitterType == EDynamicEmitterType::Mesh)
	{
		const auto& SpriteSource =
			static_cast<const FDynamicSpriteEmitterReplayDataBase&>(Source);
		OutBuffer.Material = SpriteSource.Material;

		if (!OutBuffer.Material)
			UE_LOG("[ParticleProxy] FillStagingBuffer: Material is null (emitter type=%d)", (int)Source.eEmitterType);

		if (Source.eEmitterType == EDynamicEmitterType::Mesh)
		{
			OutBuffer.EmitterMeshBuffer =
				static_cast<const FDynamicMeshEmitterData&>(EmitterData).MeshBuffer;

			if (!OutBuffer.EmitterMeshBuffer)
				UE_LOG("[ParticleProxy] FillStagingBuffer: MeshBuffer is null on Mesh emitter");
		}
	}

	if (Count == 0) return;

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
		}
	}
	else if (Source.eEmitterType == EDynamicEmitterType::Mesh)
	{
		for (int32 i = 0; i < Count; ++i)
		{
			const uint32 Idx = Source.DataContainer.ParticleIndices
				? Source.DataContainer.ParticleIndices[i]
				: static_cast<uint32>(i);

			// TODO: FBaseParticle 레이아웃 확정 후 주석 해제
			// const FBaseParticle* P = reinterpret_cast<const FBaseParticle*>(
			//     Source.DataContainer.ParticleData + Idx * Source.ParticleStride);
			// FMeshParticleInstanceVertex* Inst = reinterpret_cast<FMeshParticleInstanceVertex*>(
			//     OutBuffer.StagingBuffer.data() + i * Stride);
			// Inst->Transform = ...;
			// Inst->Color = P->Color;
			(void)Idx;
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
		// TODO: 구현 예정
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

	FShader* Shader = FShaderManager::Get().GetOrCreate(EShaderPath::ParticleSprite);
	if (!Shader)
	{
		UE_LOG("[ParticleProxy] SubmitSpriteEmitter: ParticleSprite shader not found (%s)", EShaderPath::ParticleSprite);
		return;
	}

	const FParticleRenderState RS = ResolveParticleRenderState(Buffer.BlendMode);

	FDrawCommand& Cmd                  = OutCmdList.AddCommand();
	Cmd.Shader                         = Shader;
	Cmd.Pass                           = RS.Pass;
	Cmd.RenderState.Blend              = RS.Blend;
	Cmd.RenderState.DepthStencil       = RS.DepthStencil;
	Cmd.RenderState.Rasterizer         = ERasterizerState::SolidNoCull; // 빌보드는 항상 양면

	Cmd.Buffer.VB             = QuadVB.GetBuffer();
	Cmd.Buffer.VBStride       = sizeof(FParticleQuadVertex);
	Cmd.Buffer.IB             = QuadIB.GetBuffer();
	Cmd.Buffer.IndexCount     = 6;
	Cmd.Buffer.InstanceVB     = Buffer.InstanceVB.GetBuffer();
	Cmd.Buffer.InstanceStride = sizeof(FParticleSpriteInstance);
	Cmd.Buffer.InstanceCount  = static_cast<uint32>(Buffer.ActiveParticleCount);

	Cmd.Bindings.PerShaderCB[0] = &Buffer.ParticleFrameCB;

	if (Buffer.Material)
		Cmd.Bindings.SRVs[(int)EMaterialTextureSlot::Diffuse] =
			const_cast<ID3D11ShaderResourceView*>(
				Buffer.Material->GetCachedSRVs()[(int)EMaterialTextureSlot::Diffuse]);

	Cmd.BuildSortKey();
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

	FShader* Shader = FShaderManager::Get().GetOrCreate(EShaderPath::ParticleMesh);
	if (!Shader)
	{
		UE_LOG("[ParticleProxy] SubmitMeshEmitter: ParticleMesh shader not found (%s)", EShaderPath::ParticleMesh);
		return;
	}

	const FParticleRenderState RS = ResolveParticleRenderState(Buffer.BlendMode);

	FDrawCommand& Cmd                  = OutCmdList.AddCommand();
	Cmd.Shader                         = Shader;
	Cmd.Pass                           = RS.Pass;
	Cmd.RenderState.Blend              = RS.Blend;
	Cmd.RenderState.DepthStencil       = RS.DepthStencil;
	Cmd.RenderState.Rasterizer         = ERasterizerState::SolidNoCull; // 메시 파티클도 양면

	Cmd.Buffer.VB             = Buffer.EmitterMeshBuffer->GetVertexBuffer().GetBuffer();
	Cmd.Buffer.VBStride       = Buffer.EmitterMeshBuffer->GetVertexBuffer().GetStride();
	Cmd.Buffer.IB             = Buffer.EmitterMeshBuffer->GetIndexBuffer().GetBuffer();
	Cmd.Buffer.IndexCount     = Buffer.EmitterMeshBuffer->GetIndexBuffer().GetIndexCount();
	Cmd.Buffer.InstanceVB     = Buffer.InstanceVB.GetBuffer();
	Cmd.Buffer.InstanceStride = sizeof(FMeshParticleInstanceVertex);
	Cmd.Buffer.InstanceCount  = static_cast<uint32>(Buffer.ActiveParticleCount);

	if (Buffer.Material)
		Cmd.Bindings.SRVs[(int)EMaterialTextureSlot::Diffuse] =
			const_cast<ID3D11ShaderResourceView*>(
				Buffer.Material->GetCachedSRVs()[(int)EMaterialTextureSlot::Diffuse]);

	Cmd.BuildSortKey();
}
