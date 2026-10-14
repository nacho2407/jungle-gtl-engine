#include "Render/Proxy/ShapeSceneProxy.h"
#include "Component/ShapeComponent.h"
#include "Component/Shape/BoxComponent.h"
#include "Component/Shape/SphereComponent.h"
#include "Component/Shape/CapsuleComponent.h"
#include "Component/Shape/CurvedWallRunColliderComponent.h"
#include "GameFramework/AActor.h"
#include "Math/MathUtils.h"
#include "Math/Quat.h"

#include <cmath>
#include <algorithm>

// ============================================================
// 와이어프레임 빌드 헬퍼
// ============================================================
namespace
{
	void AddWireCircle(TArray<FWireLine>& Lines, const FVector& Center,
		const FVector& AxisA, const FVector& AxisB, float Radius, int32 Segments)
	{
		if (Radius <= 0.0f || Segments < 3) return;

		const float Step = 2.0f * FMath::Pi / static_cast<float>(Segments);
		FVector Prev = Center + AxisA * Radius;

		for (int32 i = 1; i <= Segments; ++i)
		{
			const float Angle = Step * static_cast<float>(i);
			FVector Next = Center + (AxisA * cosf(Angle) + AxisB * sinf(Angle)) * Radius;
			Lines.push_back({ Prev, Next });
			Prev = Next;
		}
	}

	void AddWireHalfCircle(TArray<FWireLine>& Lines, const FVector& Center,
		const FVector& AxisA, const FVector& AxisB, float Radius, int32 Segments, float StartAngle)
	{
		if (Radius <= 0.0f || Segments < 3) return;

		const float Step = FMath::Pi / static_cast<float>(Segments);
		FVector Prev = Center + (AxisA * cosf(StartAngle) + AxisB * sinf(StartAngle)) * Radius;

		for (int32 i = 1; i <= Segments; ++i)
		{
			const float Angle = StartAngle + Step * static_cast<float>(i);
			FVector Next = Center + (AxisA * cosf(Angle) + AxisB * sinf(Angle)) * Radius;
			Lines.push_back({ Prev, Next });
			Prev = Next;
		}
	}

	void BuildBoxLines(TArray<FWireLine>& Lines, const FVector& Center, const FVector& Ext,
		const FVector& AxisX, const FVector& AxisY, const FVector& AxisZ)
	{
		// Local axis-aligned 코너에 컴포넌트의 world rotation을 적용해야 차량처럼
		// 회전된 콜라이더가 시각상 자식 컴포넌트와 일치한다.
		FVector Corners[8];
		for (int32 i = 0; i < 8; ++i)
		{
			const float X = (i & 1) ? Ext.X : -Ext.X;
			const float Y = (i & 2) ? Ext.Y : -Ext.Y;
			const float Z = (i & 4) ? Ext.Z : -Ext.Z;
			Corners[i] = Center + AxisX * X + AxisY * Y + AxisZ * Z;
		}

		// Bottom 4
		Lines.push_back({ Corners[0], Corners[1] });
		Lines.push_back({ Corners[1], Corners[3] });
		Lines.push_back({ Corners[3], Corners[2] });
		Lines.push_back({ Corners[2], Corners[0] });
		// Top 4
		Lines.push_back({ Corners[4], Corners[5] });
		Lines.push_back({ Corners[5], Corners[7] });
		Lines.push_back({ Corners[7], Corners[6] });
		Lines.push_back({ Corners[6], Corners[4] });
		// Vertical 4
		Lines.push_back({ Corners[0], Corners[4] });
		Lines.push_back({ Corners[1], Corners[5] });
		Lines.push_back({ Corners[2], Corners[6] });
		Lines.push_back({ Corners[3], Corners[7] });
	}

	void BuildSphereLines(TArray<FWireLine>& Lines, const FVector& Center, float Radius)
	{
		constexpr int32 Segments = 24;
		AddWireCircle(Lines, Center, FVector(1, 0, 0), FVector(0, 1, 0), Radius, Segments);
		AddWireCircle(Lines, Center, FVector(1, 0, 0), FVector(0, 0, 1), Radius, Segments);
		AddWireCircle(Lines, Center, FVector(0, 1, 0), FVector(0, 0, 1), Radius, Segments);
	}

	void BuildCapsuleLines(TArray<FWireLine>& Lines, const FVector& Center, float Radius, float HalfHeight,
		const FVector& AxisX, const FVector& AxisY, const FVector& AxisZ)
	{
		const float CylinderHalf = HalfHeight - Radius;
		constexpr int32 Segments = 24;
		constexpr int32 HalfSegments = 12;

		const FVector TopCenter = Center + AxisZ * CylinderHalf;
		const FVector BotCenter = Center - AxisZ * CylinderHalf;

		// Top and bottom circles (XY 평면)
		AddWireCircle(Lines, TopCenter, AxisX, AxisY, Radius, Segments);
		AddWireCircle(Lines, BotCenter, AxisX, AxisY, Radius, Segments);

		// 4 vertical lines
		Lines.push_back({ TopCenter + AxisX * Radius, BotCenter + AxisX * Radius });
		Lines.push_back({ TopCenter - AxisX * Radius, BotCenter - AxisX * Radius });
		Lines.push_back({ TopCenter + AxisY * Radius, BotCenter + AxisY * Radius });
		Lines.push_back({ TopCenter - AxisY * Radius, BotCenter - AxisY * Radius });

		// Top hemisphere caps (XZ, YZ 평면, 위쪽 반원)
		AddWireHalfCircle(Lines, TopCenter, AxisX, AxisZ, Radius, HalfSegments, 0.0f);
		AddWireHalfCircle(Lines, TopCenter, AxisY, AxisZ, Radius, HalfSegments, 0.0f);

		// Bottom hemisphere caps (아래쪽 반원)
		AddWireHalfCircle(Lines, BotCenter, AxisX, AxisZ, Radius, HalfSegments, FMath::Pi);
		AddWireHalfCircle(Lines, BotCenter, AxisY, AxisZ, Radius, HalfSegments, FMath::Pi);
	}

	void BuildCurvedWallLines(TArray<FWireLine>& Lines, const UCurvedWallRunColliderComponent* Curve,
		const FVector& AxisX, const FVector& AxisY, const FVector& AxisZ)
	{
		if (!Curve) return;

		const FVector Center = Curve->GetWorldLocation();
		const float Radius = Curve->GetScaledRadius();
		const float HalfHeight = Curve->GetScaledHalfHeight();
		const float HalfThickness = Curve->GetScaledThickness() * 0.5f;
		const float ArcRadians = Curve->GetArcAngleRadians();
		const int32 Segments = (std::max)(Curve->GetSafeSegmentCount(), 4);

		auto PointAt = [&](float Angle, float RadiusOffset, float Height)
		{
			const float SampleRadius = (std::max)(0.0f, Radius + RadiusOffset);
			return Center
				+ (AxisX * std::cos(Angle) + AxisY * std::sin(Angle)) * SampleRadius
				+ AxisZ * Height;
		};

		for (float RadiusOffset : { -HalfThickness, HalfThickness })
		{
			FVector PrevTop = PointAt(-ArcRadians * 0.5f, RadiusOffset, HalfHeight);
			FVector PrevBottom = PointAt(-ArcRadians * 0.5f, RadiusOffset, -HalfHeight);
			for (int32 Index = 1; Index <= Segments; ++Index)
			{
				const float Alpha = static_cast<float>(Index) / static_cast<float>(Segments);
				const float Angle = -ArcRadians * 0.5f + ArcRadians * Alpha;
				const FVector NextTop = PointAt(Angle, RadiusOffset, HalfHeight);
				const FVector NextBottom = PointAt(Angle, RadiusOffset, -HalfHeight);

				Lines.push_back({ PrevTop, NextTop });
				Lines.push_back({ PrevBottom, NextBottom });
				PrevTop = NextTop;
				PrevBottom = NextBottom;
			}
		}

		const float StartAngle = -ArcRadians * 0.5f;
		const float EndAngle = ArcRadians * 0.5f;
		for (float Angle : { StartAngle, EndAngle })
		{
			Lines.push_back({ PointAt(Angle, -HalfThickness, HalfHeight), PointAt(Angle, HalfThickness, HalfHeight) });
			Lines.push_back({ PointAt(Angle, -HalfThickness, -HalfHeight), PointAt(Angle, HalfThickness, -HalfHeight) });
			Lines.push_back({ PointAt(Angle, -HalfThickness, -HalfHeight), PointAt(Angle, -HalfThickness, HalfHeight) });
			Lines.push_back({ PointAt(Angle, HalfThickness, -HalfHeight), PointAt(Angle, HalfThickness, HalfHeight) });
		}

		const int32 VerticalStep = (std::max)(1, Segments / 4);
		for (int32 Index = 0; Index <= Segments; Index += VerticalStep)
		{
			const float Alpha = static_cast<float>(Index) / static_cast<float>(Segments);
			const float Angle = -ArcRadians * 0.5f + ArcRadians * Alpha;
			Lines.push_back({ PointAt(Angle, 0.0f, -HalfHeight), PointAt(Angle, 0.0f, HalfHeight) });
		}
	}
}

// ============================================================
// FShapeSceneProxy
// ============================================================

FShapeSceneProxy::FShapeSceneProxy(UShapeComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent)
{
	ProxyFlags = EPrimitiveProxyFlags::EditorOnly
	           | EPrimitiveProxyFlags::NeverCull
	           | EPrimitiveProxyFlags::WireShape;

	bDrawOnlyIfSelected = InComponent->IsDrawOnlyIfSelected();
	FVector4 SC = InComponent->GetShapeColorVec4();
	WireColor = SC;

	bCastShadow = false;
	bCastShadowAsTwoSided = false;

	RebuildLines();
}

void FShapeSceneProxy::UpdateTransform()
{
	FPrimitiveSceneProxy::UpdateTransform();
	RebuildLines();
}

void FShapeSceneProxy::UpdateVisibility()
{
	// 컴포넌트/액터 가시성 우선 반영
	FPrimitiveSceneProxy::UpdateVisibility();

	// 부모가 visible 판정한 뒤, 추가 조건 적용
	if (bVisible && bDrawOnlyIfSelected)
	{
		bVisible = IsSelected();
	}
}

void FShapeSceneProxy::RebuildLines()
{
	CachedLines.clear();

	UPrimitiveComponent* OwnerComp = GetOwner();
	if (!OwnerComp) return;

	FVector AxisX = OwnerComp->GetForwardVector();
	FVector AxisY = OwnerComp->GetRightVector();
	FVector AxisZ = OwnerComp->GetUpVector();
	AxisX.Normalize();
	AxisY.Normalize();
	AxisZ.Normalize();

	if (const UBoxComponent* Box = Cast<UBoxComponent>(OwnerComp))
	{
		BuildBoxLines(CachedLines, Box->GetWorldLocation(), Box->GetScaledBoxExtent(), AxisX, AxisY, AxisZ);
	}
	else if (const USphereComponent* Sphere = Cast<USphereComponent>(OwnerComp))
	{
		// 구체는 회전 무관 — rotation 인자 불필요
		BuildSphereLines(CachedLines, Sphere->GetWorldLocation(), Sphere->GetScaledSphereRadius());
	}
	else if (const UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(OwnerComp))
	{
		BuildCapsuleLines(CachedLines,
			Capsule->GetWorldLocation(),
			Capsule->GetScaledCapsuleRadius(),
			Capsule->GetScaledCapsuleHalfHeight(),
			AxisX, AxisY, AxisZ);
	}
	else if (const UCurvedWallRunColliderComponent* Curve = Cast<UCurvedWallRunColliderComponent>(OwnerComp))
	{
		BuildCurvedWallLines(CachedLines, Curve, AxisX, AxisY, AxisZ);
	}
}
