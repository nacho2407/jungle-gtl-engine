#include "Render/Proxy/ShapeSceneProxy.h"
#include "Component/ShapeComponent.h"
#include "Component/BoxComponent.h"
#include "Component/SphereComponent.h"
#include "Component/CapsuleComponent.h"
#include "GameFramework/AActor.h"
#include "Math/MathUtils.h"

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

	void BuildBoxLines(TArray<FWireLine>& Lines, const FVector& Center, const FVector& Ext)
	{
		FVector Corners[8];
		for (int32 i = 0; i < 8; ++i)
		{
			Corners[i] = Center + FVector(
				(i & 1) ? Ext.X : -Ext.X,
				(i & 2) ? Ext.Y : -Ext.Y,
				(i & 4) ? Ext.Z : -Ext.Z
			);
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

	void BuildCapsuleLines(TArray<FWireLine>& Lines, const FVector& Center, float Radius, float HalfHeight)
	{
		const float CylinderHalf = HalfHeight - Radius;
		constexpr int32 Segments = 24;
		constexpr int32 HalfSegments = 12;

		const FVector TopCenter = Center + FVector(0, 0, CylinderHalf);
		const FVector BotCenter = Center - FVector(0, 0, CylinderHalf);

		// Top and bottom circles
		AddWireCircle(Lines, TopCenter, FVector(1, 0, 0), FVector(0, 1, 0), Radius, Segments);
		AddWireCircle(Lines, BotCenter, FVector(1, 0, 0), FVector(0, 1, 0), Radius, Segments);

		// 4 vertical lines
		Lines.push_back({ TopCenter + FVector(Radius, 0, 0), BotCenter + FVector(Radius, 0, 0) });
		Lines.push_back({ TopCenter - FVector(Radius, 0, 0), BotCenter - FVector(Radius, 0, 0) });
		Lines.push_back({ TopCenter + FVector(0, Radius, 0), BotCenter + FVector(0, Radius, 0) });
		Lines.push_back({ TopCenter - FVector(0, Radius, 0), BotCenter - FVector(0, Radius, 0) });

		// Top hemisphere caps
		AddWireHalfCircle(Lines, TopCenter, FVector(1, 0, 0), FVector(0, 0, 1), Radius, HalfSegments, 0.0f);
		AddWireHalfCircle(Lines, TopCenter, FVector(0, 1, 0), FVector(0, 0, 1), Radius, HalfSegments, 0.0f);

		// Bottom hemisphere caps
		AddWireHalfCircle(Lines, BotCenter, FVector(1, 0, 0), FVector(0, 0, 1), Radius, HalfSegments, FMath::Pi);
		AddWireHalfCircle(Lines, BotCenter, FVector(0, 1, 0), FVector(0, 0, 1), Radius, HalfSegments, FMath::Pi);
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

	if (const UBoxComponent* Box = Cast<UBoxComponent>(OwnerComp))
	{
		BuildBoxLines(CachedLines, Box->GetWorldLocation(), Box->GetScaledBoxExtent());
	}
	else if (const USphereComponent* Sphere = Cast<USphereComponent>(OwnerComp))
	{
		BuildSphereLines(CachedLines, Sphere->GetWorldLocation(), Sphere->GetScaledSphereRadius());
	}
	else if (const UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(OwnerComp))
	{
		BuildCapsuleLines(CachedLines,
			Capsule->GetWorldLocation(),
			Capsule->GetScaledCapsuleRadius(),
			Capsule->GetScaledCapsuleHalfHeight());
	}
}
