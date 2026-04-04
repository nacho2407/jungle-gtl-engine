#include "RenderCollector.h"

#include "Actor/Actor.h"
#include "Component/PrimitiveComponent.h"
#include "Renderer/RenderCommand.h"
#include "Scene/Scene.h"
#include <utility>

void FSceneRenderCollector::CollectRenderCommands(
	UScene* Scene,
	const FFrustum& Frustum,
	const FShowFlags& ShowFlags,
	const FVector& CameraPosition,
	FRenderCommandQueue& OutQueue)
{
	(void)CameraPosition;

	if (!Scene)
	{
		return;
	}

	UPrimitiveComponent::FlushPendingRenderStateUpdates();

	VisiblePrimitivesScratch.clear();
	FrustrumCull(Scene, Frustum, ShowFlags, VisiblePrimitivesScratch);
	OutQueue.Commands.reserve(OutQueue.Commands.size() + VisiblePrimitivesScratch.size());

	for (UPrimitiveComponent* PrimitiveComponent : VisiblePrimitivesScratch)
	{
		if (!PrimitiveComponent)
		{
			continue;
		}

		FPrimitiveSceneProxy* SceneProxy = PrimitiveComponent->GetSceneProxy();
		if (!SceneProxy)
		{
			continue;
		}

		FRenderCommand Command = {};
		Command.SceneProxy = SceneProxy;
		OutQueue.AddCommand(std::move(Command));
	}
}

void FSceneRenderCollector::FrustrumCull(
	UScene* Scene,
	const FFrustum& Frustum,
	const FShowFlags& ShowFlags,
	TArray<UPrimitiveComponent*>& OutVisible)
{
	if (!Scene)
	{
		return;
	}

	const bool bShowUUID = ShowFlags.HasFlag(EEngineShowFlags::SF_UUID);
	const bool bShowBillboard = ShowFlags.HasFlag(EEngineShowFlags::SF_Billboard);
	const bool bShowText = ShowFlags.HasFlag(EEngineShowFlags::SF_Text);
	const bool bShowPrimitives = ShowFlags.HasFlag(EEngineShowFlags::SF_Primitives);
	if (!bShowUUID && !bShowBillboard && !bShowText && !bShowPrimitives)
	{
		return;
	}

	CandidatePrimitivesScratch.clear();
	Scene->QueryPrimitivesByFrustum(Frustum, CandidatePrimitivesScratch);
	OutVisible.reserve(OutVisible.size() + CandidatePrimitivesScratch.size());

	for (UPrimitiveComponent* PrimitiveComponent : CandidatePrimitivesScratch)
	{
		if (!PrimitiveComponent || PrimitiveComponent->IsPendingKill())
		{
			continue;
		}

		AActor* OwnerActor = PrimitiveComponent->GetOwnerFast();
		if (!OwnerActor || OwnerActor->IsPendingDestroy() || !OwnerActor->IsVisible())
		{
			continue;
		}

		switch (PrimitiveComponent->GetRenderCategory())
		{
		case EPrimitiveRenderCategory::UUIDBillboard:
			if (!bShowUUID)
			{
				continue;
			}
			break;
		case EPrimitiveRenderCategory::SubUV:
			if (!bShowBillboard)
			{
				continue;
			}
			break;
		case EPrimitiveRenderCategory::Text:
			if (!bShowText)
			{
				continue;
			}
			break;
		case EPrimitiveRenderCategory::Primitive:
		default:
			if (!bShowPrimitives)
			{
				continue;
			}

			if (!PrimitiveComponent->GetRenderMesh())
			{
				continue;
			}
			break;
		}

		OutVisible.push_back(PrimitiveComponent);
	}
}
