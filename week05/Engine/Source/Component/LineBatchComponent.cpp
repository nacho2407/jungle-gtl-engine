#include "LineBatchComponent.h"
#include "PrimitiveLineBatch.h"
#include "Math/MathUtility.h"
#include "Object/Class.h"

IMPLEMENT_RTTI(ULineBatchComponent, UNewPrimitiveComponent)

void ULineBatchComponent::PostConstruct()
{
	LineMesh = std::make_shared<FDynamicMesh>();
	LineMesh->Topology = EMeshTopology::EMT_LineList;
}

void ULineBatchComponent::DrawLine(FVector InStart, FVector InEnd, FVector4 InColor)
{
	if (!LineMesh) return;

	uint32 CurrentSize = static_cast<uint32>(LineMesh->Vertices.size());

	FVertex V1, V2;
	V1.Position = InStart;
	V1.Color = InColor;
	V1.Normal = FVector::ZeroVector; // 노멀 초기화

	V2.Position = InEnd;
	V2.Color = InColor;
	V2.Normal = FVector::ZeroVector;

	LineMesh->Vertices.push_back(V1);
	LineMesh->Vertices.push_back(V2);

	// 인덱스 버퍼 업데이트
	LineMesh->Indices.push_back(CurrentSize);
	LineMesh->Indices.push_back(CurrentSize + 1);

	// 상태 갱신
	LineMesh->bIsDirty = true;
	LineMesh->UpdateLocalBound();
}

void ULineBatchComponent::DrawWireCube(FVector InCenter, FQuat InRotation, FVector InScale, FVector4 InColor)
{
	if (!LineMesh) return;
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
	if (!LineMesh) return;

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

FBoxSphereBounds ULineBatchComponent::GetLocalBounds() const
{
	if (LineMesh)
	{
		return { LineMesh->GetCenterCoord(), LineMesh->GetLocalBoundRadius(),
				 (LineMesh->GetMaxCoord() - LineMesh->GetMinCoord()) * 0.5f };
	}
	return { FVector::ZeroVector, 0.f, FVector::ZeroVector };
}

void ULineBatchComponent::Clear()
{
	if (LineMesh)
	{
		LineMesh->Vertices.clear();
		LineMesh->Indices.clear();
		LineMesh->bIsDirty = true;
		LineMesh->UpdateLocalBound();
	}
}