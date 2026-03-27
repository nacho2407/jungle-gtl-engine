#include "LineBatchComponent.h"
#include "PrimitiveLineBatch.h"
#include "Math/MathUtility.h"
#include "Object/Class.h"

IMPLEMENT_RTTI(ULineBatchComponent, UPrimitiveComponent)

void ULineBatchComponent::Initialize()
{
	Primitive = std::make_shared<CPrimitiveLineBatch>();
}

void ULineBatchComponent::DrawLine(FVector InStart, FVector InEnd, FVector4 InColor)
{
	auto primitive = static_pointer_cast<CPrimitiveLineBatch>(Primitive);
	primitive->AddLine(InStart, InEnd, InColor);
}

void ULineBatchComponent::DrawWireCube(FVector InCenter, FQuat InRotation, FVector InScale, FVector4 InColor)
{
	auto LineBatch = static_pointer_cast<CPrimitiveLineBatch>(Primitive);
	const FVector BaseCube[12][2] = {
		{{-0.5f, -0.5f, 0.5f}, {0.5f, -0.5f, 0.5f}},  // 왼쪽 위
		{{-0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, -0.5f}},  // 왼쪽 아래
		{{-0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, 0.5f}},  // 오른쪽 위
		{{-0.5f, 0.5f, -0.5f}, {0.5f, 0.5f, -0.5f}},  // 오른쪽 아래
		{{0.5f, -0.5f, 0.5f}, {0.5f, 0.5f, 0.5f}},  // 앞쪽 위
		{{0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, -0.5f}},  // 앞쪽 아래
		{{-0.5f, -0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f}},  // 뒷쪽 위
		{{-0.5f, -0.5f, -0.5f}, {-0.5f, 0.5f, -0.5f}},  // 뒷쪽 아래
		{{0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, 0.5f}},  // 앞쪽 왼
		{{0.5f, 0.5f, -0.5f}, {0.5f, 0.5f, 0.5f}},  // 앞쪽 오른
		{{-0.5f, -0.5f, -0.5f}, {-0.5f, -0.5f, 0.5f}},  // 뒷쪽 왼
		{{-0.5f, 0.5f, -0.5f}, {-0.5f, 0.5f, 0.5f}}  // 뒷쪽 오른
	};
	for (int i = 0; i < 12; i++)
	{
		FVector Start = InRotation * FVector::Multiply(BaseCube[i][0], InScale) + InCenter;
		FVector End = InRotation * FVector::Multiply(BaseCube[i][1], InScale) + InCenter;
		DrawLine(Start, End, InColor);
	}
}

void ULineBatchComponent::DrawWireSphere(FVector InCenter, float InRadius, FVector4 InColor)
{
	auto LineBatch = static_pointer_cast<CPrimitiveLineBatch>(Primitive);
	if (!LineBatch) return;

	const int32 Segments = 16; // 선의 개수 (정밀도)
	const float AngleStep = 2.0f * FMath::PI / Segments;

	for (int32 i = 0; i < Segments; i++)
	{
		float A1 = i * AngleStep;
		float A2 = (i + 1) * AngleStep;

		float S1 = sinf(A1) * InRadius;
		float C1 = cosf(A1) * InRadius;
		float S2 = sinf(A2) * InRadius;
		float C2 = cosf(A2) * InRadius;

		// XY 평면 원 (가로)
		DrawLine(
			InCenter + FVector(C1, S1, 0.0f),
			InCenter + FVector(C2, S2, 0.0f),
			InColor
		);

		// YZ 평면 원 (세로 1)
		DrawLine(
			InCenter + FVector(0.0f, C1, S1),
			InCenter + FVector(0.0f, C2, S2),
			InColor
		);

		// ZX 평면 원 (세로 2)
		DrawLine(
			InCenter + FVector(S1, 0.0f, C1),
			InCenter + FVector(S2, 0.0f, C2),
			InColor
		);
	}
}

void ULineBatchComponent::Clear()
{
	auto primitive = static_pointer_cast<CPrimitiveLineBatch>(Primitive);
	primitive->ClearVertices();
}