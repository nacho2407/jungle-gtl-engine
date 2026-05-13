#include "Editor/Viewport/EditorBoxSelectionService.h"

#include "Camera/ViewportCamera.h"
#include "Component/PrimitiveComponent.h"
#include "Editor/Selection/SelectionManager.h"
#include "Engine/Input/InputSystem.h"
#include "GameFramework/World.h"
#include "Math/Vector4.h"

#include <algorithm>
#include <unordered_set>

void FEditorBoxSelectionService::SelectActorsInBox(
	UWorld* World,
	FSelectionManager* SelectionManager,
	const FViewportCamera* Camera,
	const POINT& BoxSelectStart,
	const POINT& BoxSelectEnd,
	float ViewportWidth,
	float ViewportHeight,
	FWorldSpatialIndex::FPrimitiveFrustumQueryScratch& FrustumQueryScratch)
{
	if (!SelectionManager || !World || !Camera)
	{
		return;
	}

	const int32 MinX = std::min(BoxSelectStart.x, BoxSelectEnd.x);
	const int32 MinY = std::min(BoxSelectStart.y, BoxSelectEnd.y);
	const int32 MaxX = std::max(BoxSelectStart.x, BoxSelectEnd.x);
	const int32 MaxY = std::max(BoxSelectStart.y, BoxSelectEnd.y);
	const int32 Width = MaxX - MinX;
	const int32 Height = MaxY - MinY;

	if (Width < 2 || Height < 2)
	{
		return;
	}

	if (!InputSystem::Get().GetKey(VK_SHIFT))
	{
		SelectionManager->ClearSelection();
	}

	TArray<UPrimitiveComponent*> CandidatePrimitives;
	World->GetSpatialIndex().FrustumQueryPrimitives(Camera->GetFrustum(), CandidatePrimitives, FrustumQueryScratch);

	std::unordered_set<AActor*> SeenActors;
	SeenActors.reserve(CandidatePrimitives.size());

	for (UPrimitiveComponent* Primitive : CandidatePrimitives)
	{
		AActor* Actor = (Primitive != nullptr) ? Primitive->GetOwner() : nullptr;
		if (!Actor || !Actor->GetRootComponent())
		{
			continue;
		}

		if (!SeenActors.insert(Actor).second)
		{
			continue;
		}

		float ViewportX = 0.f;
		float ViewportY = 0.f;
		float Depth = 0.f;
		if (!TryProjectWorldToViewport(*Camera, Actor->GetActorLocation(), ViewportWidth, ViewportHeight, ViewportX, ViewportY, Depth))
		{
			continue;
		}

		if (Depth < 0.f || Depth > 1.f)
		{
			continue;
		}

		const int32 Px = static_cast<int32>(ViewportX);
		const int32 Py = static_cast<int32>(ViewportY);
		if (Px >= MinX && Px <= MaxX && Py >= MinY && Py <= MaxY)
		{
			SelectionManager->AddSelect(Actor);
		}
	}
}

bool FEditorBoxSelectionService::TryProjectWorldToViewport(
	const FViewportCamera& Camera,
	const FVector& WorldPos,
	float ViewportWidth,
	float ViewportHeight,
	float& OutViewportX,
	float& OutViewportY,
	float& OutDepth)
{
	const FVector4 Clip = FMatrix::Identity.TransformVector4(FVector4(WorldPos, 1.0f), Camera.GetViewProjectionMatrix());
	if (MathUtil::IsNearlyZero(Clip.W))
	{
		return false;
	}

	const float InvW = 1.0f / Clip.W;
	const float NdcX = Clip.X * InvW;
	const float NdcY = Clip.Y * InvW;
	const float NdcZ = Clip.Z * InvW;
	if (NdcX < -1.0f || NdcX > 1.0f || NdcY < -1.0f || NdcY > 1.0f)
	{
		return false;
	}

	OutViewportX = (NdcX * 0.5f + 0.5f) * ViewportWidth;
	OutViewportY = (1.0f - (NdcY * 0.5f + 0.5f)) * ViewportHeight;
	OutDepth = NdcZ;
	return true;
}
