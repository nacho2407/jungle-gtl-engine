#include "DebugDrawManager.h"

#include "Actor/Actor.h"
#include "Component/PrimitiveComponent.h"
#include "Core/ShowFlags.h"
#include "Object/Class.h"
#include "World/World.h"

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
	(void)Length;
	bDrawWorldAxis = true;
}

void FDebugDrawManager::BuildRenderRequest(
	const FShowFlags& ShowFlags,
	UWorld* World,
	FDebugLineRenderRequest& OutRequest) const
{
	// 엔진 쪽 수집 데이터는 여기서만 feature가 이해하는 request 형식으로 변환한다.
	if (!ShowFlags.HasFlag(EEngineShowFlags::SF_DebugDraw))
	{
		return;
	}

	if (ShowFlags.HasFlag(EEngineShowFlags::SF_Collision) && World)
	{
		DrawAllCollisionBounds(World, OutRequest);
	}

	for (const FDebugCube& Cube : Cubes)
	{
		FDebugLineRenderFeature::AddCube(OutRequest, Cube.Center, Cube.Extent, Cube.Color);
	}

	for (const FDebugLine& Line : Lines)
	{
		FDebugLineRenderFeature::AddLine(OutRequest, Line.Start, Line.End, Line.Color);
	}

	if (ShowFlags.HasFlag(EEngineShowFlags::SF_WorldAxis))
	{
		FDebugLineRenderFeature::AddLine(OutRequest, { 0, 0, 0 }, { 1000, 0, 0 }, { 1, 0, 0, 1 });
		FDebugLineRenderFeature::AddLine(OutRequest, { 0, 0, 0 }, { 0, 1000, 0 }, { 0, 1, 0, 1 });
		FDebugLineRenderFeature::AddLine(OutRequest, { 0, 0, 0 }, { 0, 0, 1000 }, { 0, 0, 1, 1 });
	}
}

void FDebugDrawManager::Clear()
{
	Lines.clear();
	Cubes.clear();
	bDrawWorldAxis = false;
}

void FDebugDrawManager::DrawAllCollisionBounds(UWorld* World, FDebugLineRenderRequest& OutRequest) const
{
	// 충돌 디버그 가시화는 월드의 프리미티브 컴포넌트를 순회하며 바운드를 선분으로 바꾼다.
	TArray<AActor*> AllActors = World->GetAllActors();
	for (AActor* Actor : AllActors)
	{
		if (!Actor || Actor->IsPendingDestroy() || !Actor->IsVisible())
		{
			continue;
		}

		for (UActorComponent* Component : Actor->GetComponents())
		{
			if (!Component || !Component->IsA(UPrimitiveComponent::StaticClass()))
			{
				continue;
			}

			UPrimitiveComponent* PrimitiveComponent = static_cast<UPrimitiveComponent*>(Component);
			if (!PrimitiveComponent->ShouldDrawDebugBounds())
			{
				continue;
			}

			const FBoxSphereBounds Bounds = PrimitiveComponent->GetWorldBounds();
			if (Bounds.BoxExtent.SizeSquared() > 0.0f)
			{
				FDebugLineRenderFeature::AddCube(OutRequest, Bounds.Center, Bounds.BoxExtent, FVector4(1.0f, 0.0f, 0.0f, 1.0f));
			}
		}
	}
}
