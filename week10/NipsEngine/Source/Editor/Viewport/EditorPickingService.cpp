#include "Editor/Viewport/EditorPickingService.h"

#include "Camera/ViewportCamera.h"
#include "Component/PrimitiveComponent.h"
#include "Core/CollisionTypes.h"
#include "Editor/Settings/EditorSettings.h"
#include "Editor/Viewport/FSceneViewport.h"
#include "EditorEngine.h"
#include "GameFramework/World.h"
#include "Render/Renderer/Renderer.h"
#include "Spatial/WorldSpatialIndex.h"

#include <algorithm>
#include <cfloat>
#include <cmath>

bool FEditorPickingService::ResolveActorForSelection(
	UWorld* World,
	const FViewportCamera* Camera,
	FSceneViewport* Viewport,
	UEditorEngine* Editor,
	float LocalX,
	float LocalY,
	AActor*& OutActor)
{
	OutActor = nullptr;
	const FEditorSettings& EditorSettings = FEditorSettings::Get();
	if (EditorSettings.PickingMode == EEditorPickingMode::IdBuffer && PickActorByIdAtViewportLocalPoint(Viewport, Editor, LocalX, LocalY, OutActor))
	{
		return OutActor != nullptr;
	}

	return PickActorByRayAtViewportLocalPoint(World, Camera, Viewport, LocalX, LocalY, OutActor);
}

bool FEditorPickingService::PickActorByIdAtViewportLocalPoint(
	FSceneViewport* Viewport,
	UEditorEngine* Editor,
	float LocalX,
	float LocalY,
	AActor*& OutActor)
{
	OutActor = nullptr;
	if (!Viewport || !Editor)
	{
		return false;
	}

	ID3D11DeviceContext* Context = Editor->GetRenderer().GetFD3DDevice().GetDeviceContext();
	if (!Context)
	{
		return false;
	}

	const int32 BaseX = static_cast<int32>(std::round(LocalX));
	const int32 BaseY = static_cast<int32>(std::round(LocalY));
	static constexpr POINT SampleOffsets[] =
	{
		{ 0, 0 },
		{ 1, 0 },
		{ -1, 0 },
		{ 0, 1 },
		{ 0, -1 }
	};

	for (const POINT& Offset : SampleOffsets)
	{
		uint32 PickId = 0;
		const int32 SampleX = BaseX + static_cast<int32>(Offset.x);
		const int32 SampleY = BaseY + static_cast<int32>(Offset.y);
		const uint32 X = static_cast<uint32>(std::max<int32>(0, SampleX));
		const uint32 Y = static_cast<uint32>(std::max<int32>(0, SampleY));
		if (!Viewport->ReadEditorIdPickAt(X, Y, Context, PickId) || PickId == 0)
		{
			continue;
		}

		OutActor = Viewport->GetEditorIdPickActor(PickId);
		return OutActor != nullptr;
	}

	return true;
}

bool FEditorPickingService::PickActorByRayAtViewportLocalPoint(
	UWorld* World,
	const FViewportCamera* Camera,
	FSceneViewport* Viewport,
	float LocalX,
	float LocalY,
	AActor*& OutActor)
{
	OutActor = nullptr;
	if (!World || !Camera || !Viewport)
	{
		return false;
	}

	const FViewportRect& Rect = Viewport->GetRect();
	if (Rect.Width <= 0 || Rect.Height <= 0)
	{
		return false;
	}

	const FRay Ray = Camera->DeprojectScreenToWorld(LocalX, LocalY, static_cast<float>(Rect.Width), static_cast<float>(Rect.Height));
	float ClosestDist = FLT_MAX;

	FWorldSpatialIndex::FPrimitiveRayQueryScratch RayQueryScratch;
	TArray<UPrimitiveComponent*> CandidatePrimitives;
	TArray<float> CandidateTs;
	World->GetSpatialIndex().RayQueryPrimitives(Ray, CandidatePrimitives, CandidateTs, RayQueryScratch);

	for (int32 CandidateIndex = 0; CandidateIndex < static_cast<int32>(CandidatePrimitives.size()); ++CandidateIndex)
	{
		if (CandidateTs[CandidateIndex] > ClosestDist)
		{
			break;
		}

		UPrimitiveComponent* PrimitiveComp = CandidatePrimitives[CandidateIndex];
		AActor* Actor = PrimitiveComp ? PrimitiveComp->GetOwner() : nullptr;
		if (!Actor || !Actor->GetRootComponent())
		{
			continue;
		}

		FHitResult HitResult{};
		if (PrimitiveComp->Raycast(Ray, HitResult) && HitResult.Distance < ClosestDist)
		{
			ClosestDist = HitResult.Distance;
			OutActor = Actor;
		}
	}

	return true;
}
