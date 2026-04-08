#include "Level/ScenePacketBuilder.h"

#include "Actor/Actor.h"
#include "Component/BillboardComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/SubUVComponent.h"
#include "Component/TextComponent.h"
#include "Component/UUIDBillboardComponent.h"

void FScenePacketBuilder::BuildScenePacket(
	const TArray<AActor*>& Actors,
	const FFrustum& Frustum,
	const FShowFlags& ShowFlags,
	FSceneRenderPacket& OutPacket)
{
	TArray<UPrimitiveComponent*> VisiblePrimitives;
	FrustumCull(Actors, Frustum, ShowFlags, VisiblePrimitives);

	OutPacket.Clear();
	OutPacket.Reserve(VisiblePrimitives.size());

	for (UPrimitiveComponent* Primitive : VisiblePrimitives)
	{
		if (!Primitive)
		{
			continue;
		}

		if (Primitive->IsA(UStaticMeshComponent::StaticClass()))
		{
			OutPacket.MeshPrimitives.push_back({ static_cast<UStaticMeshComponent*>(Primitive) });
			continue;
		}

		if (Primitive->IsA(UTextRenderComponent::StaticClass()))
		{
			OutPacket.TextPrimitives.push_back({ static_cast<UTextRenderComponent*>(Primitive) });
			continue;
		}

		if (Primitive->IsA(USubUVComponent::StaticClass()))
		{
			OutPacket.SubUVPrimitives.push_back({ static_cast<USubUVComponent*>(Primitive) });
			continue;
		}

		if (Primitive->IsA(UBillboardComponent::StaticClass()))
		{
			OutPacket.BillboardPrimitives.push_back({ static_cast<UBillboardComponent*>(Primitive) });
		}
	}
}

void FScenePacketBuilder::FrustumCull(
	const TArray<AActor*>& Actors,
	const FFrustum& Frustum,
	const FShowFlags& ShowFlags,
	TArray<UPrimitiveComponent*>& OutVisible)
{
	for (AActor* Actor : Actors)
	{
		if (!Actor || Actor->IsPendingDestroy() || !Actor->IsVisible())
		{
			continue;
		}

		for (UActorComponent* Component : Actor->GetComponents())
		{
			if (!Component->IsA(UPrimitiveComponent::StaticClass()))
			{
				continue;
			}

			UPrimitiveComponent* PrimitiveComponent = static_cast<UPrimitiveComponent*>(Component);

			const bool bIsUUID = PrimitiveComponent->IsA(UUUIDBillboardComponent::StaticClass());
			const bool bIsSubUV = PrimitiveComponent->IsA(USubUVComponent::StaticClass());
			const bool bIsText = PrimitiveComponent->IsA(UTextRenderComponent::StaticClass());
			const bool bIsBillboard = PrimitiveComponent->IsA(UBillboardComponent::StaticClass());
			if (bIsUUID)
			{
				if (!ShowFlags.HasFlag(EEngineShowFlags::SF_UUID))
				{
					continue;
				}
			}
			else if (bIsSubUV)
			{
				if (!ShowFlags.HasFlag(EEngineShowFlags::SF_Billboard))
				{
					continue;
				}
			}
			else if (bIsBillboard)
			{
				if (!ShowFlags.HasFlag(EEngineShowFlags::SF_Billboard))
				{
					continue;
				}
			}
			else if (bIsText)
			{
				if (!ShowFlags.HasFlag(EEngineShowFlags::SF_Text))
				{
					continue;
				}
			}
			else
			{
				if (!ShowFlags.HasFlag(EEngineShowFlags::SF_Primitives))
				{
					continue;
				}
				if (!PrimitiveComponent->GetRenderMesh())
				{
					continue;
				}
			}

			if (Frustum.IsVisible(PrimitiveComponent->GetWorldBounds()))
			{
				OutVisible.push_back(PrimitiveComponent);
			}
		}
	}
}

