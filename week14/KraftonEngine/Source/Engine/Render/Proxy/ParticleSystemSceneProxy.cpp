#include "Render/Proxy/ParticleSystemSceneProxy.h"

#include "Component/Particle/ParticleSystemComponent.h"
#include "Render/Command/DrawCommand.h"
#include "Render/Shader/ShaderManager.h"
#include "Render/Types/FrameContext.h"
#include "Materials/Material.h"
#include "Materials/MaterialManager.h"
#include "Particles/Runtime/ParticleEmitterInstance.h"
#include "Particles/Module/ParticleModule.h"

FParticleSystemSceneProxy::FParticleSystemSceneProxy(UParticleSystemComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent)
{
	ProxyFlags |= EPrimitiveProxyFlags::PerViewportUpdate;
	ProxyFlags |= EPrimitiveProxyFlags::NeverCull; // 테스트 (Particle Bound 도입 시 삭제)
	ProxyFlags &= ~EPrimitiveProxyFlags::SupportsOutline;
	ProxyFlags &= ~EPrimitiveProxyFlags::ShowAABB;

	DefaultMaterial = UMaterial::CreateTransient(ERenderPass::Opaque, EBlendState::Opaque, EDepthStencilState::Default,
		ERasterizerState::SolidBackCull, FShaderManager::Get().GetOrCreate(EShaderPath::Particle));
}

FParticleSystemSceneProxy::~FParticleSystemSceneProxy()
{
	SpriteGeometry.Release();
}

void FParticleSystemSceneProxy::UpdateMesh()
{
	MeshBuffer = nullptr;
	SectionDraws.clear();
}

void FParticleSystemSceneProxy::UpdateMaterial()
{
	RebuildSectionDraws();
}

void FParticleSystemSceneProxy::UpdatePerViewport(const FFrameContext& Frame)
{
	if (!bVisible) return;

	RebuildSpriteParticleGeometry(Frame);
	RebuildSectionDraws();
}

bool FParticleSystemSceneProxy::PrepareDrawBuffer(ID3D11Device* Device, ID3D11DeviceContext* Context,
	FDrawCommandBuffer& OutBuffer) const
{
	if (SpriteIndexCount == 0 || !Device || !Context) return false;

	if (!bSpriteGeometryCreated)
	{
		SpriteGeometry.Create(Device);
		bSpriteGeometryCreated = true;
	}

	if (!SpriteGeometry.Upload(Context)) return false;

	OutBuffer = {};
	OutBuffer.VB = SpriteGeometry.GetVertexBuffer();
	OutBuffer.VBStride = SpriteGeometry.GetVertexStride();
	OutBuffer.IB = SpriteGeometry.GetIndexBuffer();

	return OutBuffer.VB != nullptr && OutBuffer.IB != nullptr;
}

void FParticleSystemSceneProxy::RebuildSpriteParticleGeometry(const FFrameContext& Frame)
{
	SpriteGeometry.Clear();
	SpriteSections.clear();
	SpriteIndexCount = 0;

	UParticleSystemComponent* Component = GetParticleSystemComponent();
	if (!Component) return;

	const TArray<FParticleEmitterInstance*>& Instances = Component->GetEmitterInstances();

	for (FParticleEmitterInstance* Instance : Instances)
	{
		if (!Instance) continue;

		const uint32 FirstIndex = SpriteGeometry.GetIndexCount();

		const int32 ActiveCount = Instance->GetActiveParticleCount();
		const FParticleDataContainer& Data = Instance->GetParticleDataContainer();

		for (int32 Index = 0; Index < ActiveCount; ++Index)
		{
			const FBaseParticle& Particle = Data.GetParticle(Index);
			if (!Particle.bAlive) continue;

			SpriteGeometry.AddParticleQuad(Particle, Frame.CameraRight, Frame.CameraUp);
		}

		const uint32 IndexCount = SpriteGeometry.GetIndexCount() - FirstIndex;
		if (IndexCount > 0)
		{
			SpriteSections.push_back({ ResolveEmitterMaterial(Instance), FirstIndex, IndexCount });
		}
	}

	SpriteIndexCount = SpriteGeometry.GetIndexCount();
}

void FParticleSystemSceneProxy::RebuildSectionDraws()
{
	SectionDraws.clear();

	if (SpriteIndexCount == 0) return;

	for (const FParticleSpriteSection& Section : SpriteSections)
	{
		UMaterialInterface* Material = Section.Material ? Section.Material : DefaultMaterial;
		if (!Material || Section.IndexCount == 0) continue;

		SectionDraws.push_back({ Material, Section.FirstIndex, Section.IndexCount });
	}
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

UParticleSystemComponent* FParticleSystemSceneProxy::GetParticleSystemComponent() const
{
	return static_cast<UParticleSystemComponent*>(GetOwner());
}
