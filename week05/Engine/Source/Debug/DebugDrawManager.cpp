#include "DebugDrawManager.h"
#include "Renderer/Renderer.h"
#include "Core/ShowFlags.h"
#include "World/World.h"
#include "Actor/Actor.h"
#include "Component/PrimitiveComponent.h"
#include "Component/UUIDBillboardComponent.h"
#include "Component/SubUVComponent.h"
#include "Component/SkyComponent.h"
#include "Object/Class.h"
void FDebugDrawManager::DrawLine(const FVector& Start, const FVector& End, const FVector4& Color)
{
	Lines.push_back({ Start, End, Color });
}

void FDebugDrawManager::DrawCube(const FVector& Center, const FVector& Extent, const FVector4& Color)
{
	Cubes.push_back({ Center, Extent, Color });
}

void FDebugDrawManager::DrawWorldAxis(float Length)
{
	bDrawWorldAxis = true;
}

void FDebugDrawManager::Flush(CRenderer* Renderer, const FShowFlags& ShowFlags, UWorld* World)
{

	if (!Renderer) return;

	// 디버그 드로우 전체 꺼져있으면 스킵
	if (!ShowFlags.HasFlag(EEngineShowFlags::SF_DebugDraw))
	{
		Clear();
		return;
	}


	if (ShowFlags.HasFlag(EEngineShowFlags::SF_Collision) && World)
	{
		DrawAllCollisionBounds(Renderer, World);
	}

	for (const auto& Cube : Cubes)
	{
		Renderer->DrawCube(Cube.Center, Cube.Extent, Cube.Color);
	}
	for (const auto& Line : Lines)
	{
		Renderer->DrawLine(Line.Start, Line.End, Line.Color);
	}


	// 월드 축
	if (ShowFlags.HasFlag(EEngineShowFlags::SF_WorldAxis))
	{
		Renderer->DrawLine({ 0,0,0 }, { 1000,0,0 }, { 1,0,0,1 });  // X: Red
		Renderer->DrawLine({ 0,0,0 }, { 0,1000,0 }, { 0,1,0,1 });  // Y: Green
		Renderer->DrawLine({ 0,0,0 }, { 0,0,1000 }, { 0,0,1,1 });  // Z: Blue
	}

	Renderer->ExecuteLineCommands();
	Clear();
}

void FDebugDrawManager::Clear()
{
	Lines.clear();
	Cubes.clear();
	bDrawWorldAxis = false;
}

void FDebugDrawManager::DrawAllCollisionBounds(CRenderer* Renderer, UWorld* World)
{
	TArray<AActor*> AllActors = World->GetAllActors();
	for (AActor* Actor : AllActors)
	{
		if (!Actor || Actor->IsPendingDestroy() || !Actor->IsVisible())
			continue;

		for (UActorComponent* Comp : Actor->GetComponents())
		{
			if (!Comp->IsA(UPrimitiveComponent::StaticClass()))
				continue;

			UPrimitiveComponent* PrimComp = static_cast<UPrimitiveComponent*>(Comp);

			// 빌보드, SubUV 제외
			if (!PrimComp->ShouldDrawDebugBounds()) continue;

			if (!PrimComp->GetPrimitive() || !PrimComp->GetPrimitive()->GetMeshData())
				continue;

			FBoxSphereBounds Bound = PrimComp->GetWorldBounds();
			Renderer->DrawCube(Bound.Center, Bound.BoxExtent, FVector4(1, 0, 0, 1));  // 초록색
		}
	}
}
