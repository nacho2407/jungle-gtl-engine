#include "Render/Proxy/ParticleSystemSceneProxy.h"

#include "Component/Particle/ParticleSystemComponent.h"
#include "Render/Command/DrawCommand.h"
#include "Render/Shader/ShaderManager.h"
#include "Render/Types/FrameContext.h"
#include "Runtime/Engine.h"
#include "Materials/Material.h"
#include "Materials/MaterialManager.h"
#include "Mesh/MeshManager.h"
#include "Particles/ParticleSystem.h"
#include "Particles/Runtime/ParticleEmitterInstance.h"
#include "Particles/Module/ParticleModule.h"
#include "Particles/Module/ParticleModuleTypeDataBase.h"
#include "Render/Proxy/ParticleDynamicEmitterData.h"

FParticleSystemSceneProxy::FParticleSystemSceneProxy(UParticleSystemComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent)
{
	ProxyFlags |= EPrimitiveProxyFlags::ParticleSystem;
	ProxyFlags |= EPrimitiveProxyFlags::PerViewportUpdate;
	ProxyFlags |= EPrimitiveProxyFlags::NeverCull; // 테스트 (Particle Bound 도입 시 삭제)

	DefaultMaterial = UMaterial::CreateTransient(ERenderPass::Opaque, EBlendState::Opaque, EDepthStencilState::Default,
		ERasterizerState::SolidBackCull, FShaderManager::Get().GetOrCreate(EShaderPath::Particle));
}

FParticleSystemSceneProxy::~FParticleSystemSceneProxy()
{
	SpriteGeometry.Release();
	MeshGeometry.Release();
	MeshInstanceBuffer.Release();
}

void FParticleSystemSceneProxy::UpdateMesh()
{
	MeshBuffer = nullptr;
	SectionDraws.clear();
}

void FParticleSystemSceneProxy::UpdateMaterial()
{
	SectionDraws.clear();
}

void FParticleSystemSceneProxy::UpdatePerViewport(const FFrameContext& Frame)
{
	if (!bVisible) return;

	ResetDynamicGeometry();

	UParticleSystemComponent* Component = GetParticleSystemComponent();
	if (!Component) return;

	BuildDynamicEmitters(Frame, Component->GetEmitterInstances());
	FinalizeDynamicGeometry();
}

void FParticleSystemSceneProxy::ResetDynamicGeometry()
{
	ClearDrawBatches();
	SpriteGeometry.Clear();
	SpriteIndexCount = 0;
	MeshGeometry.Clear();
	MeshInstances.clear();
	MeshIndexCount = 0;
}

void FParticleSystemSceneProxy::BuildDynamicEmitters(const FFrameContext& Frame, const TArray<FParticleEmitterInstance*>& Instances)
{
	for (int32 EmitterIndex = 0; EmitterIndex < static_cast<int32>(Instances.size()); ++EmitterIndex)
	{
		FParticleEmitterInstance* Instance = Instances[EmitterIndex];
		if (!Instance) continue;

		const UParticleLODLevel* CurrentLODLevel = Instance->GetCurrentLODLevel();
		if (!CurrentLODLevel || !CurrentLODLevel->IsEnabled()) continue;

		AppendEmitter(Frame, EmitterIndex, BuildEmitterSource(Instance));
	}
}

FDynamicEmitterReplayDataBase FParticleSystemSceneProxy::BuildEmitterSource(FParticleEmitterInstance* Instance) const
{
	FDynamicEmitterReplayDataBase Source;
	Source.Instance = Instance;
	Source.Material = ResolveEmitterMaterial(Instance);
	Source.RenderType = GetEmitterRenderType(Instance);
	return Source;
}

void FParticleSystemSceneProxy::AppendEmitter(const FFrameContext& Frame, int32 EmitterIndex, const FDynamicEmitterReplayDataBase& Source)
{
	switch (Source.RenderType)
	{
		case EParticleRenderType::Sprite:
			AppendSpriteEmitter(Frame, EmitterIndex, Source);
			break;

		case EParticleRenderType::Ribbon:
			AppendRibbonEmitter(Frame, EmitterIndex, Source);
			break;

		case EParticleRenderType::Beam:
			AppendBeamEmitter(Frame, EmitterIndex, Source);
			break;

		case EParticleRenderType::Mesh:
			AppendMeshEmitter(Frame, EmitterIndex, Source);
			break;

		default:
			break;
	}
}

void FParticleSystemSceneProxy::FinalizeDynamicGeometry()
{
	SpriteIndexCount = SpriteGeometry.GetIndexCount();

	if (MeshInstances.empty())
	{
		MeshIndexCount = 0;

		for (FParticleDrawBatch& Batch : DrawBatches)
		{
			if (Batch.Type == EParticleRenderType::Mesh)
			{
				Batch.Sections.clear();
				Batch.InstanceCount = 0;
			}
		}
	}
}

void FParticleSystemSceneProxy::AppendSpriteEmitter(const FFrameContext& Frame, int32 EmitterIndex, const FDynamicEmitterReplayDataBase& Source)
{
	FParticleDrawBatch& Batch = FindOrAddDrawBatch(EParticleRenderType::Sprite);
	const uint32 FirstIndex = SpriteGeometry.GetIndexCount();

	FDynamicSpriteEmitterData DynamicData(EmitterIndex, Source, true);
	const uint32 IndexCount = DynamicData.BuildDynamicVertexData(Frame, SpriteGeometry);
	if (IndexCount > 0)
	{
		Batch.Sections.push_back({ Source.Material, FirstIndex, IndexCount });
	}
}

void FParticleSystemSceneProxy::AppendRibbonEmitter(const FFrameContext& Frame, int32 EmitterIndex, const FDynamicEmitterReplayDataBase& Source)
{
	FParticleDrawBatch& Batch = FindOrAddDrawBatch(EParticleRenderType::Ribbon);
	const uint32 FirstIndex = SpriteGeometry.GetIndexCount();

	FDynamicRibbonEmitterData DynamicData(EmitterIndex, Source);
	const uint32 IndexCount = DynamicData.BuildDynamicVertexData(Frame, SpriteGeometry);
	if (IndexCount > 0)
	{
		Batch.Sections.push_back({ Source.Material, FirstIndex, IndexCount });
	}
}

void FParticleSystemSceneProxy::AppendBeamEmitter(const FFrameContext& Frame, int32 EmitterIndex, const FDynamicEmitterReplayDataBase& Source)
{
	FParticleDrawBatch& Batch = FindOrAddDrawBatch(EParticleRenderType::Beam);
	const uint32 FirstIndex = SpriteGeometry.GetIndexCount();

	FDynamicBeamEmitterData DynamicData(EmitterIndex, Source);
	const uint32 IndexCount = DynamicData.BuildDynamicVertexData(Frame, SpriteGeometry);
	if (IndexCount > 0)
	{
		Batch.Sections.push_back({ Source.Material, FirstIndex, IndexCount });
	}
}

void FParticleSystemSceneProxy::AppendMeshEmitter(const FFrameContext& Frame, int32 EmitterIndex, const FDynamicEmitterReplayDataBase& Source)
{
	FParticleMeshEmitterInstance* MeshInstance = dynamic_cast<FParticleMeshEmitterInstance*>(Source.Instance);
	if (!MeshInstance || !MeshInstance->TypeDataModule) return;

	UParticleModuleTypeDataMesh* TypeData = MeshInstance->TypeDataModule;

	UStaticMesh* Mesh = ResolveTypeDataMesh(TypeData);
	FStaticMesh* MeshAsset = Mesh ? Mesh->GetStaticMeshAsset() : nullptr;
	if (!MeshAsset || MeshAsset->Vertices.empty() || MeshAsset->Indices.empty()) return;

	FParticleDrawBatch& Batch = FindOrAddMeshDrawBatch(Mesh);

	if (Batch.Sections.empty())
	{
		const TArray<FStaticMaterial>& StaticMaterials = Mesh->GetStaticMaterials();

		for (const FStaticMeshSection& MeshSection : MeshAsset->Sections)
		{
			const uint32 IndexCount = MeshSection.NumTriangles * 3;
			if (IndexCount == 0) continue;

			UMaterialInterface* Material = nullptr;
			const int32 MaterialIndex = MeshSection.MaterialIndex;
			if (MaterialIndex >= 0 && MaterialIndex < static_cast<int32>(StaticMaterials.size()))
			{
				Material = StaticMaterials[MaterialIndex].MaterialInterface;
			}

			if (!Material)
			{
				Material = Source.Material;
			}

			Batch.Sections.push_back({
				Material,
				MeshSection.FirstIndex,
				IndexCount
			});

			MeshIndexCount += IndexCount;
		}
	}

	FDynamicMeshEmitterData DynamicData(EmitterIndex, Source, MeshInstance);
	Batch.InstanceCount += DynamicData.BuildDynamicVertexData(Frame, MeshInstances, ShouldSortMeshParticles(Batch, Source.Material));
}

bool FParticleSystemSceneProxy::PrepareDrawBuffer(ID3D11Device* Device, ID3D11DeviceContext* Context,
	FDrawCommandBuffer& OutBuffer) const
{
	for (const FParticleDrawBatch& Batch : DrawBatches)
	{
		if (!Batch.Sections.empty())
		{
			return PrepareParticleDrawBuffer(Batch, Device, Context, OutBuffer);
		}
	}

	return false;
}

bool FParticleSystemSceneProxy::PrepareParticleDrawBuffer(const FParticleDrawBatch& Batch, ID3D11Device* Device, ID3D11DeviceContext* Context,
	FDrawCommandBuffer& OutBuffer) const
{
	if (!Device || !Context) return false;

	OutBuffer = {};

	switch (Batch.Type)
	{
	case EParticleRenderType::Sprite:
	case EParticleRenderType::Ribbon:
	case EParticleRenderType::Beam:
	{
		if (SpriteIndexCount == 0 || Batch.Sections.empty()) return false;

		if (!bSpriteGeometryCreated)
		{
			SpriteGeometry.Create(Device);
			bSpriteGeometryCreated = true;
		}

		if (!SpriteGeometry.Upload(Context)) return false;

		OutBuffer.VB = SpriteGeometry.GetVertexBuffer();
		OutBuffer.VBStride = SpriteGeometry.GetVertexStride();
		OutBuffer.IB = SpriteGeometry.GetIndexBuffer();
		return OutBuffer.VB != nullptr && OutBuffer.IB != nullptr;
	}

	case EParticleRenderType::Mesh:
	{
		if (!Batch.Mesh || Batch.InstanceCount == 0 || Batch.Sections.empty()) return false;

		FStaticMesh* MeshAsset = Batch.Mesh->GetStaticMeshAsset();
		if (!MeshAsset || !MeshAsset->RenderBuffer) return false;

		if (!bMeshInstanceBufferCreated)
		{
			MeshInstanceBuffer.Create(Device, 1024, sizeof(FMeshParticleInstanceData));
			bMeshInstanceBufferCreated = true;
		}

		if (MeshInstances.empty()) return false;

		MeshInstanceBuffer.EnsureCapacity(Device, static_cast<uint32>(MeshInstances.size()));
		if (!MeshInstanceBuffer.Update(Context, MeshInstances.data(), static_cast<uint32>(MeshInstances.size())))
		{
			return false;
		}

		OutBuffer.VB = MeshAsset->RenderBuffer->GetVertexBuffer().GetBuffer();
		OutBuffer.VBStride = MeshAsset->RenderBuffer->GetVertexBuffer().GetStride();
		OutBuffer.IB = MeshAsset->RenderBuffer->GetIndexBuffer().GetBuffer();

		OutBuffer.InstanceVB = MeshInstanceBuffer.GetBuffer();
		OutBuffer.InstanceStride = sizeof(FMeshParticleInstanceData);
		OutBuffer.FirstInstance = Batch.FirstInstance;
		OutBuffer.InstanceCount = Batch.InstanceCount;

		return OutBuffer.VB != nullptr && OutBuffer.IB != nullptr && OutBuffer.InstanceVB != nullptr;
	}

	default:
		return false;
	}
}

FParticleDrawBatch& FParticleSystemSceneProxy::FindOrAddDrawBatch(EParticleRenderType Type)
{
	for (FParticleDrawBatch& Batch : DrawBatches)
	{
		if (Batch.Type == Type)
		{
			return Batch;
		}
	}

	FParticleDrawBatch NewBatch;
	NewBatch.Type = Type;
	DrawBatches.push_back(NewBatch);
	return DrawBatches.back();
}

FParticleDrawBatch& FParticleSystemSceneProxy::FindOrAddMeshDrawBatch(UStaticMesh* Mesh)
{
	for (FParticleDrawBatch& Batch : DrawBatches)
	{
		if (Batch.Type == EParticleRenderType::Mesh && Batch.Mesh == Mesh)
		{
			return Batch;
		}
	}

	FParticleDrawBatch NewBatch;
	NewBatch.Type = EParticleRenderType::Mesh;
	NewBatch.Mesh = Mesh;
	NewBatch.FirstInstance = static_cast<uint32>(MeshInstances.size());
	NewBatch.InstanceCount = 0;

	DrawBatches.push_back(NewBatch);
	return DrawBatches.back();
}

UMaterialInterface* FParticleSystemSceneProxy::ResolveEmitterMaterial(const FParticleEmitterInstance* Instance) const
{
	if (!Instance) return DefaultMaterial;
	
	if (UParticleModuleRequired* Required = Instance->GetRequiredModule())
	{
		if (!Required->Material && !Required->MaterialPath.IsNull())
		{
			Required->Material = FMaterialManager::Get().GetOrCreateMaterialInterface(Required->MaterialPath.ToString());
		}

		if (Required->Material)
		{
			return Required->Material;
		}
	}

	return DefaultMaterial;
}

UStaticMesh* FParticleSystemSceneProxy::ResolveTypeDataMesh(UParticleModuleTypeDataMesh* TypeData) const
{
	if (!TypeData)
	{
		return nullptr;
	}

	if (TypeData->Mesh)
	{
		return TypeData->Mesh;
	}

	if (TypeData->MeshPath.IsNull())
	{
		return nullptr;
	}

	ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
	if (!Device)
	{
		return nullptr;
	}

	UStaticMesh* LoadedMesh = FMeshManager::LoadStaticMesh(TypeData->MeshPath.ToString(), Device);
	if (LoadedMesh)
	{
		TypeData->Mesh = LoadedMesh;
		TypeData->MeshPath.SetCachedObject(LoadedMesh);
	}

	return LoadedMesh;
}

bool FParticleSystemSceneProxy::ShouldSortMeshParticles(const FParticleDrawBatch& Batch, UMaterialInterface* FallbackMaterial) const
{
	for (const FMeshSectionDraw& Section : Batch.Sections)
	{
		if (Section.Material && Section.Material->GetBlendState() != EBlendState::Opaque)
		{
			return true;
		}
	}

	return FallbackMaterial && FallbackMaterial->GetBlendState() != EBlendState::Opaque;
}

UParticleSystemComponent* FParticleSystemSceneProxy::GetParticleSystemComponent() const
{
	return static_cast<UParticleSystemComponent*>(GetOwner());
}
