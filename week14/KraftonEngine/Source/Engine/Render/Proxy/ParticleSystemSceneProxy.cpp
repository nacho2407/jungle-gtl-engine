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

#include <algorithm>

namespace
{
	EParticleRenderType GetEmitterRenderType(const FParticleEmitterInstance* Instance)
	{
		const UParticleLODLevel* LODLevel = Instance ? Instance->GetCurrentLODLevel() : nullptr;
		const UParticleModuleTypeDataBase* TypeData = LODLevel ? LODLevel->GetTypeDataModule() : nullptr;
		return TypeData ? TypeData->GetRenderType() : EParticleRenderType::Sprite;
	}
}

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

	ClearDrawBatches();
	MeshInstances.clear();

	RebuildSpriteParticleGeometry(Frame);
	RebuildRibbonParticleGeometry(Frame);
	RebuildBeamParticleGeometry(Frame);
	RebuildMeshParticleGeometry();
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

void FParticleSystemSceneProxy::RebuildSpriteParticleGeometry(const FFrameContext& Frame)
{
	SpriteGeometry.Clear();
	SpriteIndexCount = 0;

	RebuildSpriteLikeParticleGeometry(Frame, EParticleRenderType::Sprite, true);
}

void FParticleSystemSceneProxy::RebuildRibbonParticleGeometry(const FFrameContext& Frame)
{
	RebuildSpriteLikeParticleGeometry(Frame, EParticleRenderType::Ribbon, false);
}

void FParticleSystemSceneProxy::RebuildBeamParticleGeometry(const FFrameContext& Frame)
{
	RebuildSpriteLikeParticleGeometry(Frame, EParticleRenderType::Beam, false);
}

void FParticleSystemSceneProxy::RebuildSpriteLikeParticleGeometry(const FFrameContext& Frame, EParticleRenderType RenderType, bool bDepthSort)
{
	FParticleDrawBatch& Batch = FindOrAddDrawBatch(RenderType);
	Batch.Sections.clear();

	UParticleSystemComponent* Component = GetParticleSystemComponent();
	if (!Component) return;

	const TArray<FParticleEmitterInstance*>& Instances = Component->GetEmitterInstances();

	for (FParticleEmitterInstance* Instance : Instances)
	{
		if (!Instance) continue;
		if (GetEmitterRenderType(Instance) != RenderType) continue;

		const uint32 FirstIndex = SpriteGeometry.GetIndexCount();

		const int32 ActiveCount = Instance->GetActiveParticleCount();
		const FParticleDataContainer& Data = Instance->GetParticleDataContainer();
		const uint16* ParticleIndices = Instance->ParticleIndices;
		if (!ParticleIndices) continue;

		if (!bDepthSort)
		{
			for (int32 Index = 0; Index < ActiveCount; ++Index)
			{
				const FBaseParticle& Particle = Data.GetParticle(ParticleIndices[Index]);
				if (!Particle.bAlive) continue;

				SpriteGeometry.AddParticleQuad(Particle, Frame.CameraRight, Frame.CameraUp);
			}
		}
		else
		{
			TArray<uint16> RenderOrder;
			RenderOrder.reserve(ActiveCount);
			for (int32 Index = 0; Index < ActiveCount; ++Index)
			{
				const uint16 ParticleSlot = ParticleIndices[Index];
				const FBaseParticle& Particle = Data.GetParticle(ParticleSlot);
				if (!Particle.bAlive) continue;

				RenderOrder.push_back(ParticleSlot);
			}

			std::sort(RenderOrder.begin(), RenderOrder.end(),
				[&Data, &Frame](uint16 A, uint16 B)
				{
					const FBaseParticle& ParticleA = Data.GetParticle(A);
					const FBaseParticle& ParticleB = Data.GetParticle(B);

					const float DepthA = (ParticleA.Position - Frame.CameraPosition).Dot(Frame.CameraForward);
					const float DepthB = (ParticleB.Position - Frame.CameraPosition).Dot(Frame.CameraForward);
					return DepthA > DepthB;
				});

			for (uint16 ParticleSlot : RenderOrder)
			{
				SpriteGeometry.AddParticleQuad(Data.GetParticle(ParticleSlot), Frame.CameraRight, Frame.CameraUp);
			}
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
	MeshInstances.clear();
	MeshIndexCount = 0;

	UParticleSystemComponent* Component = GetParticleSystemComponent();
	if (!Component) return;

	const TArray<FParticleEmitterInstance*>& Instances = Component->GetEmitterInstances();

	for (FParticleEmitterInstance* Instance : Instances)
	{
		if (!Instance) continue;
		if (GetEmitterRenderType(Instance) != EParticleRenderType::Mesh) continue;

		FParticleMeshEmitterInstance* MeshInstance = dynamic_cast<FParticleMeshEmitterInstance*>(Instance);

		if (!MeshInstance || !MeshInstance->TypeDataModule) continue;

		UParticleModuleTypeDataMesh* TypeData = MeshInstance->TypeDataModule;

		UStaticMesh* Mesh = ResolveTypeDataMesh(TypeData);
		FStaticMesh* MeshAsset = Mesh ? Mesh->GetStaticMeshAsset() : nullptr;
		if (!MeshAsset || MeshAsset->Vertices.empty() || MeshAsset->Indices.empty()) continue;

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
					Material = ResolveEmitterMaterial(Instance);
				}

				Batch.Sections.push_back({
					Material,
					MeshSection.FirstIndex,
					IndexCount
				});

				MeshIndexCount += IndexCount;
			}
		}

		const int32 ActiveCount = Instance->GetActiveParticleCount();
		const FParticleDataContainer& Data = Instance->GetParticleDataContainer();

		for (int32 ParticleIndex = 0; ParticleIndex < ActiveCount; ++ParticleIndex)
		{
			const FBaseParticle& Particle = Data.GetParticle(Instance->ParticleIndices[ParticleIndex]);
			if (!Particle.bAlive) continue;

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

			MeshInstances.push_back(MakeMeshParticleInstanceData(Transform, Particle.Color));
			++Batch.InstanceCount;
		}
	}

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

UParticleSystemComponent* FParticleSystemSceneProxy::GetParticleSystemComponent() const
{
	return static_cast<UParticleSystemComponent*>(GetOwner());
}
