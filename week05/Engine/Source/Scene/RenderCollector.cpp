#include "RenderCollector.h"

#include "Actor/Actor.h"
#include "Component/PrimitiveComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Core/ConsoleVariableManager.h"
#include "Core/Engine.h"
#include "Renderer/RenderCommand.h"
#include "Renderer/SceneProxy.h"
#include "Scene/Scene.h"
#include <chrono>
#include <cmath>
#include <utility>

namespace
{
	struct FStaticMeshCullSettings
	{
		float MaxDistance = 0.0f;
		float MinProjectedRadius = 0.0f;
		float ProjectionScaleY = 0.0f;
		bool bPerspectiveProjection = false;
	};

	float ReadConsoleFloat(const char* Name, float DefaultValue)
	{
		if (FConsoleVariable* Var = FConsoleVariableManager::Get().Find(Name))
		{
			return Var->GetFloat();
		}

		return DefaultValue;
	}

	FStaticMeshCullSettings BuildStaticMeshCullSettings(const FMatrix& ProjectionMatrix)
	{
		FStaticMeshCullSettings Settings;
		Settings.MaxDistance = (std::max)(ReadConsoleFloat("r.StaticMeshCullMaxDistance", 0.0f), 0.0f);
		Settings.MinProjectedRadius = (std::max)(ReadConsoleFloat("r.StaticMeshCullMinProjectedRadius", 0.0f), 0.0f);
		Settings.ProjectionScaleY = ProjectionMatrix.M[2][1];
		Settings.bPerspectiveProjection = std::abs(ProjectionMatrix.M[0][3]) > 0.5f;
		return Settings;
	}

	bool ShouldCullStaticMeshBySettings(
		const FBoxSphereBounds& Bounds,
		const FVector& CameraPosition,
		const FStaticMeshCullSettings& Settings,
		bool& bOutDistanceCulled,
		bool& bOutSizeCulled)
	{
		bOutDistanceCulled = false;
		bOutSizeCulled = false;

		const FVector CameraDelta = Bounds.Center - CameraPosition;
		const float DistanceSquared = CameraDelta.SizeSquared();

		if (Settings.MaxDistance > 0.0f)
		{
			const float MaxDistanceSquared = Settings.MaxDistance * Settings.MaxDistance;
			if (DistanceSquared > MaxDistanceSquared)
			{
				bOutDistanceCulled = true;
				return true;
			}
		}

		if (Settings.bPerspectiveProjection && Settings.MinProjectedRadius > 0.0f && Settings.ProjectionScaleY > 0.0f && Bounds.Radius > 0.0f)
		{
			const float DistanceToCamera = std::sqrt((std::max)(DistanceSquared, 1.0e-6f));
			const float ProjectedRadius = (Bounds.Radius * Settings.ProjectionScaleY) / DistanceToCamera;
			if (ProjectedRadius < Settings.MinProjectedRadius)
			{
				bOutSizeCulled = true;
				return true;
			}
		}

		return false;
	}
}

void FSceneRenderCollector::CollectRenderCommands(
	UScene* Scene,
	const FFrustum& Frustum,
	const FShowFlags& ShowFlags,
	const FVector& CameraPosition,
	const FMatrix& ProjectionMatrix,
	FRenderCommandQueue& OutQueue)
{
	if (!Scene)
	{
		return;
	}

	const auto CollectStartTime = std::chrono::high_resolution_clock::now();

	UPrimitiveComponent::FlushPendingRenderStateUpdates();

	VisiblePrimitivesScratch.clear();
	FrustrumCull(Scene, Frustum, ShowFlags, CameraPosition, ProjectionMatrix, VisiblePrimitivesScratch);
	OutQueue.Commands.reserve(OutQueue.Commands.size() + VisiblePrimitivesScratch.size());
	uint32 AddedStaticMeshCandidateCount = 0;

	for (const FVisiblePrimitiveEntry& VisiblePrimitive : VisiblePrimitivesScratch)
	{
		UPrimitiveComponent* PrimitiveComponent = VisiblePrimitive.PrimitiveComponent;
		if (!PrimitiveComponent)
		{
			continue;
		}

		FPrimitiveSceneProxy* SceneProxy = VisiblePrimitive.SceneProxy;
		if (!SceneProxy)
		{
			continue;
		}

		FRenderCommand Command = {};
		Command.SceneProxy = SceneProxy;
		Command.bStaticMesh = VisiblePrimitive.bStaticMesh;

		if (Command.bStaticMesh)
		{
			FStaticMeshOcclusionCandidate Candidate = {};
			if (SceneProxy->TryBuildStaticMeshOcclusionCandidate(CameraPosition, Candidate))
			{
				Command.RenderMesh = Candidate.RenderMesh;
				Command.WorldMatrix = Candidate.WorldMatrix;
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
	const FVector& CameraPosition,
	const FMatrix& ProjectionMatrix,
	TArray<FVisiblePrimitiveEntry>& OutVisible)
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

	const FStaticMeshCullSettings StaticMeshCullSettings = BuildStaticMeshCullSettings(ProjectionMatrix);
	CandidatePrimitivesScratch.clear();
	Scene->QueryPrimitivesByFrustum(Frustum, CandidatePrimitivesScratch);
	OutVisible.reserve(OutVisible.size() + CandidatePrimitivesScratch.size());
	uint32 FrustumPassedStaticMeshCount = 0;
	uint32 DistanceCulledStaticMeshCount = 0;
	uint32 SizeCulledStaticMeshCount = 0;

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

		FPrimitiveSceneProxy* SceneProxy = PrimitiveComponent->GetSceneProxy();
		if (!SceneProxy)
		{
			continue;
		}

		const bool bStaticMesh = PrimitiveComponent->IsA(UStaticMeshComponent::StaticClass());

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

			if (!bStaticMesh && !PrimitiveComponent->GetRenderMesh())
			{
				continue;
			}
			break;
		}

		if (bStaticMesh)
		{
			bool bDistanceCulled = false;
			bool bSizeCulled = false;
			if (ShouldCullStaticMeshBySettings(SceneProxy->GetBounds(), CameraPosition, StaticMeshCullSettings, bDistanceCulled, bSizeCulled))
			{
				DistanceCulledStaticMeshCount += bDistanceCulled ? 1u : 0u;
				SizeCulledStaticMeshCount += bSizeCulled ? 1u : 0u;
				continue;
			}

			++FrustumPassedStaticMeshCount;
		}

		OutVisible.push_back({ PrimitiveComponent, SceneProxy, bStaticMesh });
	}

	if (GEngine)
	{
		FRenderInstrumentationStats& Stats = GEngine->GetMutableRenderInstrumentationStats();
		Stats.FrustumPassedStaticMeshCount += FrustumPassedStaticMeshCount;
		Stats.StaticMeshDistanceCulledCount += DistanceCulledStaticMeshCount;
		Stats.StaticMeshSizeCulledCount += SizeCulledStaticMeshCount;
	}
}
