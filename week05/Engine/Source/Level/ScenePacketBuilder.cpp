#include "Level/ScenePacketBuilder.h"

#include "Component/BillboardComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/SubUVComponent.h"
#include "Component/TextComponent.h"
#include "Component/UUIDBillboardComponent.h"

bool FScenePacketBuilder::ShouldIncludePrimitive(UPrimitiveComponent* Primitive, const FShowFlags& ShowFlags) const
{
	if (!Primitive || Primitive->IsPendingKill())
	{
		return false;
	}

	const bool bIsUUID = Primitive->IsA(UUUIDBillboardComponent::StaticClass());
	const bool bIsSubUV = Primitive->IsA(USubUVComponent::StaticClass());
	const bool bIsText = Primitive->IsA(UTextRenderComponent::StaticClass());
	const bool bIsBillboard = Primitive->IsA(UBillboardComponent::StaticClass());
	if (bIsUUID)
	{
		return ShowFlags.HasFlag(EEngineShowFlags::SF_UUID);
	}

	if (bIsSubUV || bIsBillboard)
	{
		return ShowFlags.HasFlag(EEngineShowFlags::SF_Billboard);
	}

	if (bIsText)
	{
		return ShowFlags.HasFlag(EEngineShowFlags::SF_Text);
	}

	if (!ShowFlags.HasFlag(EEngineShowFlags::SF_Primitives))
	{
		return false;
	}

	return Primitive->GetRenderMesh() != nullptr;
}

void FScenePacketBuilder::BuildScenePacket(
	const TArray<UPrimitiveComponent*>& VisiblePrimitives,
	const FShowFlags& ShowFlags,
	FSceneRenderPacket& OutPacket)
{
	OutPacket.Clear();
	OutPacket.Reserve(VisiblePrimitives.size());

	for (UPrimitiveComponent* Primitive : VisiblePrimitives)
	{
		if (!ShouldIncludePrimitive(Primitive, ShowFlags))
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
