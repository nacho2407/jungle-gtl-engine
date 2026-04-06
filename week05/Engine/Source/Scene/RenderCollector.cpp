#include "RenderCollector.h"

#include "Actor/Actor.h"
#include "Component/PrimitiveComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Core/Engine.h"
#include "Renderer/RenderCommand.h"
#include "Scene/Scene.h"
#include <chrono>
#include <utility>

namespace
{
	bool TryBuildStaticMeshOcclusionCandidate(
		const UStaticMeshComponent& StaticMeshComponent,
		const FPrimitiveSceneProxy* SceneProxy,
		const FVector& CameraPosition,
		FStaticMeshOcclusionCandidate& OutCandidate)
	{
		const FBoxSphereBounds Bounds = StaticMeshComponent.GetWorldBounds();
		const float DistanceToCamera = (Bounds.Center - CameraPosition).Size();

		OutCandidate = {};
		OutCandidate.CandidateId = StaticMeshComponent.UUID;
		OutCandidate.Component = &StaticMeshComponent;
		OutCandidate.SceneProxy = SceneProxy;
		OutCandidate.StaticMesh = StaticMeshComponent.GetStaticMesh();
		OutCandidate.RenderMesh = StaticMeshComponent.GetRenderMesh(DistanceToCamera);
		OutCandidate.BoundsCenter = Bounds.Center;
		OutCandidate.BoundsRadius = Bounds.Radius;
		OutCandidate.BoundsExtent = Bounds.BoxExtent;
		OutCandidate.WorldMatrix = StaticMeshComponent.GetWorldTransform();
		return OutCandidate.Component && OutCandidate.SceneProxy && OutCandidate.StaticMesh && OutCandidate.RenderMesh;
	}
}

void FSceneRenderCollector::CollectRenderCommands(
	UScene* Scene,
	const FFrustum& Frustum,
	const FShowFlags& ShowFlags,
	const FVector& CameraPosition,
	FRenderCommandQueue& OutQueue)
{
	if (!Scene)
	{
		return;
	}

	const auto CollectStartTime = std::chrono::high_resolution_clock::now();

	UPrimitiveComponent::FlushPendingRenderStateUpdates();

	VisiblePrimitivesScratch.clear();
	FrustrumCull(Scene, Frustum, ShowFlags, VisiblePrimitivesScratch);
	OutQueue.Commands.reserve(OutQueue.Commands.size() + VisiblePrimitivesScratch.size());
	uint32 AddedStaticMeshCandidateCount = 0;

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
		Command.bStaticMesh = PrimitiveComponent->IsA(UStaticMeshComponent::StaticClass());

		if (Command.bStaticMesh)
		{
			const UStaticMeshComponent* StaticMeshComponent = static_cast<const UStaticMeshComponent*>(PrimitiveComponent);
			FStaticMeshOcclusionCandidate Candidate = {};
			if (TryBuildStaticMeshOcclusionCandidate(*StaticMeshComponent, SceneProxy, CameraPosition, Candidate))
			{
				Command.StaticMeshOcclusionCandidateIndex = static_cast<uint32>(OutQueue.StaticMeshOcclusionCandidates.size());
				OutQueue.StaticMeshOcclusionCandidates.push_back(std::move(Candidate));
				++AddedStaticMeshCandidateCount;
			}
		}

		OutQueue.AddCommand(std::move(Command));
	}

	if (GEngine)
	{
		FRenderInstrumentationStats& Stats = GEngine->GetMutableRenderInstrumentationStats();
		Stats.StaticMeshCandidateCount += AddedStaticMeshCandidateCount;
		const auto CollectEndTime = std::chrono::high_resolution_clock::now();
		Stats.CollectRenderCommandsCpuMs += std::chrono::duration<double, std::milli>(CollectEndTime - CollectStartTime).count();
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
	uint32 FrustumPassedStaticMeshCount = 0;

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

		if (PrimitiveComponent->IsA(UStaticMeshComponent::StaticClass()))
		{
			++FrustumPassedStaticMeshCount;
		}

		OutVisible.push_back(PrimitiveComponent);
	}

	if (GEngine)
	{
		GEngine->GetMutableRenderInstrumentationStats().FrustumPassedStaticMeshCount += FrustumPassedStaticMeshCount;
	}
}
