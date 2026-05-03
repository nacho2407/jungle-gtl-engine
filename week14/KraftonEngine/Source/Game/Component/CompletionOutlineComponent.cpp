#include "Game/Component/CompletionOutlineComponent.h"

#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Materials/Material.h"
#include "Render/Proxy/PrimitiveSceneProxy.h"
#include "Render/Resource/MeshBufferManager.h"
#include "Render/Scene/FScene.h"
#include "Render/Shader/ShaderManager.h"

IMPLEMENT_CLASS(UCompletionOutlineComponent, UPrimitiveComponent)
HIDE_FROM_COMPONENT_LIST(UCompletionOutlineComponent)

class FCompletionOutlineSceneProxy : public FPrimitiveSceneProxy
{
public:
	explicit FCompletionOutlineSceneProxy(UPrimitiveComponent* InComponent)
		: FPrimitiveSceneProxy(InComponent)
	{
		bCastShadow = false;
		bCastShadowAsTwoSided = false;
	}

	void UpdateMesh() override
	{
		MeshBuffer = GetOwner()->GetMeshBuffer();

		if (!DefaultMaterial)
		{
			DefaultMaterial = UMaterial::CreateTransient(
				ERenderPass::AlphaBlend,
				EBlendState::NoColor,
				EDepthStencilState::NoDepth,
				ERasterizerState::SolidNoCull,
				FShaderManager::Get().GetOrCreate(EShaderPath::Primitive));
		}

		SectionDraws.clear();
		if (MeshBuffer && DefaultMaterial)
		{
			const uint32 IndexCount = MeshBuffer->GetIndexBuffer().GetIndexCount();
			SectionDraws.push_back({ DefaultMaterial, 0, IndexCount });
		}
	}
};

void UCompletionOutlineComponent::CreateRenderState()
{
	UPrimitiveComponent::CreateRenderState();

	if (AActor* OwnerActor = GetOwner())
	{
		if (UWorld* World = OwnerActor->GetWorld())
		{
			World->GetScene().SetProxySelected(GetSceneProxy(), true);
		}
	}
}

void UCompletionOutlineComponent::DestroyRenderState()
{
	if (AActor* OwnerActor = GetOwner())
	{
		if (UWorld* World = OwnerActor->GetWorld())
		{
			World->GetScene().SetProxySelected(GetSceneProxy(), false);
		}
	}

	UPrimitiveComponent::DestroyRenderState();
}

FMeshBuffer* UCompletionOutlineComponent::GetMeshBuffer() const
{
	return &FMeshBufferManager::Get().GetMeshBuffer(EMeshShape::Quad);
}

FPrimitiveSceneProxy* UCompletionOutlineComponent::CreateSceneProxy()
{
	return new FCompletionOutlineSceneProxy(this);
}

void UCompletionOutlineComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	UPrimitiveComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (TickType != ELevelTick::LEVELTICK_All)
	{
		return;
	}

	RemainingTime -= DeltaTime;
	if (RemainingTime <= 0.0f)
	{
		if (AActor* OwnerActor = GetOwner())
		{
			OwnerActor->RemoveComponent(this);
		}
	}
}
