#include "GameFramework/Actor/CurvedWallRunColliderActor.h"

#include "Component/Shape/BoxComponent.h"
#include "Component/Shape/CurvedWallRunColliderComponent.h"
#include "Math/MathUtils.h"

#include <algorithm>
#include <cmath>

namespace
{
	constexpr const char* WallRunnableTagName = "WallRunnable";

	float GetPanelHalfLength(float Radius, float SegmentAngle)
	{
		// 탄젠트 방향 길이를 chord보다 살짝 넉넉하게 잡아 segment 사이 틈을 줄인다.
		const float HalfLength = Radius * std::tan(SegmentAngle * 0.5f);
		return (std::max)(0.05f, HalfLength * 1.05f);
	}
}

void ACurvedWallRunColliderActor::InitDefaultComponents()
{
	CurvedColliderComponent = AddComponent<UCurvedWallRunColliderComponent>();
	SetRootComponent(CurvedColliderComponent);

	// Wall-run 컴포넌트가 요구하는 태그와 collision query 조건을 기본값으로 맞춤.
	AddTag(FName(WallRunnableTagName));
	CurvedColliderComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CurvedColliderComponent->SetCollisionObjectType(ECollisionChannel::WorldStatic);
	CurvedColliderComponent->SetCollisionResponseToAllChannels(ECollisionResponse::Block);
	CurvedColliderComponent->SetGenerateOverlapEvents(false);
	CurvedColliderComponent->SetEnableGravity(false);
}

void ACurvedWallRunColliderActor::PostDuplicate()
{
	CurvedColliderComponent = Cast<UCurvedWallRunColliderComponent>(GetRootComponent());
	PhysicalPanels.clear();

	if (!HasTag(FName(WallRunnableTagName)))
	{
		AddTag(FName(WallRunnableTagName));
	}
}

void ACurvedWallRunColliderActor::BeginPlay()
{
	Super::BeginPlay();
	BuildPhysicalPanels();
}

void ACurvedWallRunColliderActor::EndPlay()
{
	ClearPhysicalPanels();
	Super::EndPlay();
}

void ACurvedWallRunColliderActor::BuildPhysicalPanels()
{
	ClearPhysicalPanels();

	if (!CurvedColliderComponent || !CurvedColliderComponent->ShouldEnablePhysicalPanels())
	{
		return;
	}

	const int32 SegmentCount = CurvedColliderComponent->GetSafeSegmentCount();
	const float Radius = CurvedColliderComponent->GetRadius();
	const float HalfHeight = CurvedColliderComponent->GetHalfHeight();
	const float Thickness = CurvedColliderComponent->GetThickness();
	const float ArcRadians = CurvedColliderComponent->GetArcAngleRadians();
	const float SegmentAngle = ArcRadians / static_cast<float>(SegmentCount);
	const float PanelHalfLength = GetPanelHalfLength(Radius, SegmentAngle);

	PhysicalPanels.reserve(SegmentCount);

	for (int32 Index = 0; Index < SegmentCount; ++Index)
	{
		const float Alpha = (static_cast<float>(Index) + 0.5f) / static_cast<float>(SegmentCount);
		const float Angle = -ArcRadians * 0.5f + ArcRadians * Alpha;
		const float CosAngle = std::cos(Angle);
		const float SinAngle = std::sin(Angle);

		UBoxComponent* Panel = AddComponent<UBoxComponent>();
		if (!Panel)
		{
			continue;
		}

		// 루트 곡선 component의 local XY 원호 위에 얇은 직사각형 패널 배치.
		Panel->AttachToComponent(CurvedColliderComponent);
		Panel->SetHiddenInComponentTree(true);
		Panel->SetEditorOnly(false);
		Panel->SetBoxExtent(FVector(PanelHalfLength, Thickness * 0.5f, HalfHeight));
		Panel->SetRelativeLocation(FVector(CosAngle * Radius, SinAngle * Radius, 0.0f));
		Panel->SetRelativeRotation(FRotator(0.0f, Angle * FMath::RadToDeg + 90.0f, 0.0f));
		Panel->SetRelativeScale(FVector::OneVector);

		// PhysX가 이미 지원하는 box body로 실제 차단을 담당.
		Panel->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Panel->SetCollisionObjectType(ECollisionChannel::WorldStatic);
		Panel->SetCollisionResponseToAllChannels(ECollisionResponse::Block);
		Panel->SetGenerateOverlapEvents(false);
		Panel->SetSimulatePhysics(false);
		Panel->SetEnableGravity(false);
		Panel->SetVisibility(false);
		Panel->CreatePhysicsState();

		PhysicalPanels.push_back(Panel);
	}
}

void ACurvedWallRunColliderActor::ClearPhysicalPanels()
{
	while (!PhysicalPanels.empty())
	{
		UBoxComponent* Panel = PhysicalPanels.back();
		PhysicalPanels.pop_back();

		if (Panel)
		{
			RemoveComponent(Panel);
		}
	}
}
