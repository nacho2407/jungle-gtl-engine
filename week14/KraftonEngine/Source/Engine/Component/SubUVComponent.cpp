#include "SubUVComponent.h"
#include "Object/ObjectFactory.h"

#include <cstring>
#include "Render/Resource/MeshBufferManager.h"
#include "Render/Shader/ShaderManager.h"
#include "Resource/ResourceManager.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Component/CameraComponent.h"
#include "Render/Proxy/SubUVSceneProxy.h"
#include "Serialization/Archive.h"
#include "Materials/Material.h"

FPrimitiveSceneProxy* USubUVComponent::CreateSceneProxy()
{
	return new FSubUVSceneProxy(this);
}

void USubUVComponent::PostDuplicate()
{
	UBillboardComponent::PostDuplicate();
	// 파티클 리소스 재바인딩
	SetParticle(ParticleName);
}

USubUVComponent::USubUVComponent()
{
	SetVisibility(false);
}

USubUVComponent::~USubUVComponent()
{
	if (SubUVMaterial)
	{
		UObjectManager::Get().DestroyObject(SubUVMaterial);
		SubUVMaterial = nullptr;
	}
}

void USubUVComponent::SetParticle(const FName& InParticleName)
{
	ParticleName = InParticleName;
	CachedParticle = FResourceManager::Get().FindParticle(InParticleName);
	RebuildSubUVMaterial();
}

void USubUVComponent::RebuildSubUVMaterial()
{
	if (!SubUVMaterial)
	{
		SubUVMaterial = UMaterial::CreateTransient(
			ERenderPass::AlphaBlend, EBlendState::AlphaBlend,
			EDepthStencilState::Default, ERasterizerState::SolidBackCull,
			FShaderManager::Get().GetOrCreate(EShaderPath::SubUV));
	}

	if (CachedParticle && CachedParticle->IsLoaded())
		SubUVMaterial->SetCachedSRV(EMaterialTextureSlot::Diffuse, CachedParticle->SRV);
	else
		SubUVMaterial->SetCachedSRV(EMaterialTextureSlot::Diffuse, nullptr);
}

bool USubUVComponent::ShouldExposeProperty(const FProperty& Property) const
{
	if (Property.OwnerClassName && strcmp(Property.OwnerClassName, "UBillboardComponent") == 0)
	{
		return false;
	}
	return UPrimitiveComponent::ShouldExposeProperty(Property);
}

void USubUVComponent::PostEditProperty(const char* PropertyName)
{
	// SubUV는 Billboard property를 숨기므로 PostEditProperty도 Primitive로 직접 올라간다.
	UPrimitiveComponent::PostEditProperty(PropertyName);

	if (strcmp(PropertyName, "ParticleName") == 0 || strcmp(PropertyName, "Particle") == 0)
	{
		SetParticle(ParticleName);
		// 파티클 교체 시 UV 그리드/텍스처가 바뀌므로 Mesh 단계까지 dirty.
		MarkProxyDirty(EDirtyFlag::Mesh);
	}
}

void USubUVComponent::UpdateWorldAABB() const
{
	FVector LExt = { 0.01f, 0.5f, 0.5f };

	float NewEx = std::abs(CachedWorldMatrix.M[0][0]) * LExt.X +
		std::abs(CachedWorldMatrix.M[1][0]) * LExt.Y +
		std::abs(CachedWorldMatrix.M[2][0]) * LExt.Z;

	float NewEy = std::abs(CachedWorldMatrix.M[0][1]) * LExt.X +
		std::abs(CachedWorldMatrix.M[1][1]) * LExt.Y +
		std::abs(CachedWorldMatrix.M[2][1]) * LExt.Z;

	float NewEz = std::abs(CachedWorldMatrix.M[0][2]) * LExt.X +
		std::abs(CachedWorldMatrix.M[1][2]) * LExt.Y +
		std::abs(CachedWorldMatrix.M[2][2]) * LExt.Z;

	FVector WorldCenter = GetWorldLocation();

	WorldAABBMinLocation = WorldCenter - FVector(NewEx, NewEy, NewEz);
	WorldAABBMaxLocation = WorldCenter + FVector(NewEx, NewEy, NewEz);
}

void USubUVComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	UBillboardComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!CachedParticle) return;
	if (!bLoop && bIsExecute) return; // 단발 재생 완료 후 정지

	const uint32 TotalFrames = CachedParticle->Columns * CachedParticle->Rows;
	if (TotalFrames == 0) return;

	const int32 PrevFrameIndex = FrameIndex;
	TimeAccumulator += DeltaTime;
	const float FrameDuration = 1.0f / PlayRate;
	while (TimeAccumulator >= FrameDuration)
	{
		TimeAccumulator -= FrameDuration;

		if (bLoop)
		{
			bIsExecute = false;
			FrameIndex = static_cast<int32>((FrameIndex + 1) % static_cast<int32>(TotalFrames)); // 무한 반복
		}
		else
		{
			if (FrameIndex < static_cast<int32>(TotalFrames - 1))
			{
				FrameIndex++;
			}
			else
			{
				bIsExecute = true;    // 마지막 프레임 도달 → 완료
				TimeAccumulator = 0.0f;
				break;
			}
		}
	}

	// FrameIndex 변경 시 프록시에 경량 dirty 마킹 (Octree/BVH 갱신 없이)
	if (FrameIndex != PrevFrameIndex)
	{
		MarkProxyDirty(EDirtyFlag::Material);
	}
}

