#include "Render/DebugDraw/DebugDrawQueue.h"
#include <cmath>

void FDebugDrawQueue::AddLine(const FVector& Start, const FVector& End,
	const FColor& Color, float Duration)
{
	FDebugDrawItem Item;
	Item.Start = Start;
	Item.End = End;
	Item.Color = Color;
	Item.RemainingTime = Duration;
	Item.bOneFrame = (Duration <= 0.0f);
	Items.push_back(Item);
}

void FDebugDrawQueue::AddBox(const FVector& Center, const FVector& Extent,
	const FColor& Color, float Duration)
{
	// 8개 꼭짓점
	FVector V[8];
	V[0] = FVector(Center.X - Extent.X, Center.Y - Extent.Y, Center.Z - Extent.Z);
	V[1] = FVector(Center.X + Extent.X, Center.Y - Extent.Y, Center.Z - Extent.Z);
	V[2] = FVector(Center.X + Extent.X, Center.Y + Extent.Y, Center.Z - Extent.Z);
	V[3] = FVector(Center.X - Extent.X, Center.Y + Extent.Y, Center.Z - Extent.Z);
	V[4] = FVector(Center.X - Extent.X, Center.Y - Extent.Y, Center.Z + Extent.Z);
	V[5] = FVector(Center.X + Extent.X, Center.Y - Extent.Y, Center.Z + Extent.Z);
	V[6] = FVector(Center.X + Extent.X, Center.Y + Extent.Y, Center.Z + Extent.Z);
	V[7] = FVector(Center.X - Extent.X, Center.Y + Extent.Y, Center.Z + Extent.Z);

	// 12개 에지
	static constexpr int32 Edges[][2] = {
		{0,1},{1,2},{2,3},{3,0},  // 하단
		{4,5},{5,6},{6,7},{7,4},  // 상단
		{0,4},{1,5},{2,6},{3,7}   // 수직
	};

	for (const auto& E : Edges)
	{
		AddLine(V[E[0]], V[E[1]], Color, Duration);
	}
}

void FDebugDrawQueue::AddSphere(const FVector& Center, float Radius, int32 Segments,
	const FColor& Color, float Duration)
{
	if (Segments < 4) Segments = 4;

	const float AngleStep = 2.0f * 3.14159265f / static_cast<float>(Segments);

	// XY 평면 (Z축 기준 원)
	for (int32 i = 0; i < Segments; ++i)
	{
		float A0 = AngleStep * i;
		float A1 = AngleStep * (i + 1);
		AddLine(
			Center + FVector(cosf(A0) * Radius, sinf(A0) * Radius, 0.0f),
			Center + FVector(cosf(A1) * Radius, sinf(A1) * Radius, 0.0f),
			Color, Duration);
	}

	// XZ 평면 (Y축 기준 원)
	for (int32 i = 0; i < Segments; ++i)
	{
		float A0 = AngleStep * i;
		float A1 = AngleStep * (i + 1);
		AddLine(
			Center + FVector(cosf(A0) * Radius, 0.0f, sinf(A0) * Radius),
			Center + FVector(cosf(A1) * Radius, 0.0f, sinf(A1) * Radius),
			Color, Duration);
	}

	// YZ 평면 (X축 기준 원)
	for (int32 i = 0; i < Segments; ++i)
	{
		float A0 = AngleStep * i;
		float A1 = AngleStep * (i + 1);
		AddLine(
			Center + FVector(0.0f, cosf(A0) * Radius, sinf(A0) * Radius),
			Center + FVector(0.0f, cosf(A1) * Radius, sinf(A1) * Radius),
			Color, Duration);
	}
}

void FDebugDrawQueue::Tick(float DeltaTime)
{
	for (int32 i = static_cast<int32>(Items.size()) - 1; i >= 0; --i)
	{
		FDebugDrawItem& Item = Items[i];

		if (Item.bOneFrame)
		{
			// 1프레임 항목: 이미 렌더링됐으므로 제거
			Items.erase(Items.begin() + i);
			continue;
		}

		Item.RemainingTime -= DeltaTime;
		if (Item.RemainingTime <= 0.0f)
		{
			Items.erase(Items.begin() + i);
		}
	}
}
