#include "Render/Proxy/ParticleSystemSceneProxy.h"

#include "Component/Particle/ParticleSystemComponent.h"
#include "Render/Command/DrawCommand.h"
#include "Render/Shader/ShaderManager.h"
#include "Render/Types/FrameContext.h"
#include "Runtime/Engine.h"
#include "Materials/Material.h"
#include "Materials/MaterialManager.h"
#include "Mesh/MeshManager.h"
#include "Particles/Runtime/ParticleEmitterInstance.h"
#include "Particles/Module/ParticleModule.h"
#include "Particles/Module/ParticleModuleTypeDataBase.h"

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

	ClearDrawBatches();

	RebuildSpriteParticleGeometry(Frame);
	RebuildMeshParticleGeometry();
}

bool FParticleSystemSceneProxy::PrepareDrawBuffer(ID3D11Device* Device, ID3D11DeviceContext* Context,
	FDrawCommandBuffer& OutBuffer) const
{
	if (MeshIndexCount > 0)
	{
		return PrepareParticleDrawBuffer(EParticleRenderType::Mesh, Device, Context, OutBuffer);
	}

	if (SpriteIndexCount > 0)
	{
		return PrepareParticleDrawBuffer(EParticleRenderType::Sprite, Device, Context, OutBuffer);
	}

	return false;
}

bool FParticleSystemSceneProxy::PrepareParticleDrawBuffer(EParticleRenderType Type, ID3D11Device* Device, ID3D11DeviceContext* Context,
	FDrawCommandBuffer& OutBuffer) const
{
	if (!Device || !Context) return false;

	OutBuffer = {};
	
	switch (Type)
	{
	case EParticleRenderType::Sprite:
		if (SpriteIndexCount == 0) return false;

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

	case EParticleRenderType::Mesh:
		if (MeshIndexCount == 0) return false;

		if (!bMeshGeometryCreated)
		{
			MeshGeometry.Create(Device);
			bMeshGeometryCreated = true;
		}

		if (!MeshGeometry.Upload(Device, Context)) return false;

		OutBuffer.VB = MeshGeometry.GetVertexBuffer();
		OutBuffer.VBStride = MeshGeometry.GetVertexStride();
		OutBuffer.IB = MeshGeometry.GetIndexBuffer();
		return OutBuffer.VB != nullptr && OutBuffer.IB != nullptr;

	default:
		return false;
	}
}

void FParticleSystemSceneProxy::RebuildSpriteParticleGeometry(const FFrameContext& Frame)
{
	SpriteGeometry.Clear();
	SpriteIndexCount = 0;

	FParticleDrawBatch& Batch = FindOrAddDrawBatch(EParticleRenderType::Sprite);
	Batch.Sections.clear();

	UParticleSystemComponent* Component = GetParticleSystemComponent();
	if (!Component) return;

	const TArray<FParticleEmitterInstance*>& Instances = Component->GetEmitterInstances();

	for (FParticleEmitterInstance* Instance : Instances)
	{
		if (!Instance) continue;
		if (dynamic_cast<FParticleMeshEmitterInstance*>(Instance)) continue;

		const uint32 FirstIndex = SpriteGeometry.GetIndexCount();

		const int32 ActiveCount = Instance->GetActiveParticleCount();
		const FParticleDataContainer& Data = Instance->GetParticleDataContainer();
		const uint16* ParticleIndices = Instance->ParticleIndices;

		for (int32 Index = 0; Index < ActiveCount; ++Index)
		{
			const FBaseParticle& Particle = Data.GetParticle(ParticleIndices[Index]);
			if (!Particle.bAlive) continue;

			SpriteGeometry.AddParticleQuad(Particle, Frame.CameraRight, Frame.CameraUp);
		}

		const uint32 IndexCount = SpriteGeometry.GetIndexCount() - FirstIndex;
		if (IndexCount > 0)
		{
			Batch.Sections.push_back({ ResolveEmitterMaterial(Instance), FirstIndex, IndexCount });
		}
	}

	SpriteIndexCount = SpriteGeometry.GetIndexCount();

	if (SpriteIndexCount == 0)
	{
		Batch.Sections.clear();
	}
}

void FParticleSystemSceneProxy::RebuildMeshParticleGeometry()
{
	MeshGeometry.Clear();
	MeshIndexCount = 0;

	FParticleDrawBatch& Batch = FindOrAddDrawBatch(EParticleRenderType::Mesh);
	Batch.Sections.clear();
	
	UParticleSystemComponent* Component = GetParticleSystemComponent();
	if (!Component) return;

	const TArray<FParticleEmitterInstance*>& Instances = Component->GetEmitterInstances();

	for (FParticleEmitterInstance* Instance : Instances)
	{
		if (!Instance) continue;

		FParticleMeshEmitterInstance* MeshInstance = dynamic_cast<FParticleMeshEmitterInstance*>(Instance);
		if (!MeshInstance || !MeshInstance->TypeDataModule) continue;

		UParticleModuleTypeDataMesh* TypeData = MeshInstance->TypeDataModule;

		UStaticMesh* Mesh = ResolveTypeDataMesh(TypeData);
		FStaticMesh* MeshAsset = Mesh ? Mesh->GetStaticMeshAsset() : nullptr;
		if (!MeshAsset || MeshAsset->Vertices.empty() || MeshAsset->Indices.empty()) continue;

		const TArray<FStaticMaterial>& StaticMaterials = Mesh->GetStaticMaterials();

		const int32 ActiveCount = Instance->GetActiveParticleCount();
		const FParticleDataContainer& Data = Instance->GetParticleDataContainer();

		for (int32 ParticleIndex = 0; ParticleIndex < ActiveCount; ++ParticleIndex)
		{
			const FBaseParticle& Particle = Data.GetParticle(Instance->ParticleIndices[ParticleIndex]);
			if (!Particle.bAlive) continue;

			for (const FStaticMeshSection& MeshSection : MeshAsset->Sections)
			{
				const uint32 FirstIndex = MeshGeometry.GetIndexCount();

				FMeshParticleTransform Transform;
				Transform.Position = Particle.Position;
				Transform.Scale = Particle.Size;
				Transform.RotationEuler = FVector(0, 0, Particle.Rotation);

				if (const FParticleMeshPayload* Payload = MeshInstance->GetParticlePayload<FParticleMeshPayload>(ParticleIndex))
				{
					Transform.Scale = FVector(
						Transform.Scale.X * Payload->MeshScale.X,
						Transform.Scale.Y * Payload->MeshScale.Y,
						Transform.Scale.Z * Payload->MeshScale.Z);
					Transform.RotationEuler += Payload->MeshRotation;
				}

				MeshGeometry.AddMeshParticle(Particle, Transform, MeshSection, MeshAsset->Vertices, MeshAsset->Indices);

				const int32 IndexCount = MeshGeometry.GetIndexCount() - FirstIndex;
				if (IndexCount == 0) continue;

				UMaterialInterface* Material = nullptr;
				if (MeshSection.MaterialIndex >= 0 && MeshSection.MaterialIndex < static_cast<int32>(StaticMaterials.size()))
				{
					Material = StaticMaterials[MeshSection.MaterialIndex].MaterialInterface;
				}

				if (!Material)
				{
					Material = ResolveEmitterMaterial(Instance);
				}

				Batch.Sections.push_back({ Material, FirstIndex, static_cast<uint32>(IndexCount) });
			}
		}
	}

	MeshIndexCount = MeshGeometry.GetIndexCount();

	if (MeshIndexCount == 0)
	{
		Batch.Sections.clear();
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

UParticleSystemComponent* FParticleSystemSceneProxy::GetParticleSystemComponent() const
{
	return static_cast<UParticleSystemComponent*>(GetOwner());
}
