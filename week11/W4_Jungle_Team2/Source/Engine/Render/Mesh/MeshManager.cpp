#include "MeshManager.h"

#include "Math/Utils.h"

#include <fstream>
#include <iostream>
#include <sstream>

FMeshData FEditorMeshLibrary::TranslationGizmoMeshData;
FMeshData FEditorMeshLibrary::RotationGizmoMeshData;
FMeshData FEditorMeshLibrary::ScaleGizmoMeshData;
bool FEditorMeshLibrary::bIsInitialized = false;

void FEditorMeshLibrary::Initialize()
{
	if (bIsInitialized) return;

	if (TranslationGizmoMeshData.Vertices.empty())
	{
		CreateTranslationGizmo();
	}

	if (ScaleGizmoMeshData.Vertices.empty())
	{
		CreateScaleGizmo();
	}

	if (RotationGizmoMeshData.Vertices.empty())
	{
		CreateRotationGizmo();
	}

    bIsInitialized = true;
}

void FEditorMeshLibrary::CreateRotationGizmo()
{
	TArray<FVertex>& vertices = RotationGizmoMeshData.Vertices;
	TArray<uint32>& indices = RotationGizmoMeshData.Indices;

	vertices.clear();
	indices.clear();

	const float Radius = 1.0f;
	const float Thickness = 0.03f;
	const int Segments = 64;
	const int TubeSegments = 8;

	FColor Colors[3] = {
		FColor(1.0f, 0.0f, 0.0f, 1.0f), // X-Axis (Red)
		FColor(0.0f, 1.0f, 0.0f, 1.0f), // Y-Axis (Green)
		FColor(0.0f, 0.0f, 1.0f, 1.0f)  // Z-Axis (Blue)
	};

	// 각 축(X, Y, Z)에 대해 고리 생성
	for (int axis = 0; axis < 3; ++axis)
	{
		uint32 StartVertexIdx = (uint32)vertices.size();

		for (int i = 0; i <= Segments; ++i)
		{
			float longitude = (float)i / Segments * 2.0f * MathUtil::PI;
			float sinLong = sin(longitude);
			float cosLong = cos(longitude);

			for (int j = 0; j < TubeSegments; ++j)
			{
				float latitude = (float)j / TubeSegments * 2.0f * MathUtil::PI;
				float sinLat = sin(latitude);
				float cosLat = cos(latitude);

				// 1. 로컬 토러스 좌표 계산 (기본 Z축 중심)
				float x = (Radius + Thickness * cosLat) * cosLong;
				float y = (Radius + Thickness * cosLat) * sinLong;
				float z = Thickness * sinLat;

				FVector pos;
				// 2. 축 방향에 따른 회전 정렬
				if (axis == 0)      pos = FVector(z, x, y); // X축 회전 (YZ 평면)
				else if (axis == 1) pos = FVector(x, z, y); // Y축 회전 (XZ 평면)
				else                pos = FVector(x, y, z); // Z축 회전 (XY 평면)

				vertices.push_back({ pos, Colors[axis], axis });
			}
		}

		// 인덱스 생성 (Side Quads)
		for (int i = 0; i < Segments; ++i)
		{
			for (int j = 0; j < TubeSegments; ++j)
			{
				uint32 nextI = i + 1;
				uint32 nextJ = (j + 1) % TubeSegments;

				uint32 i0 = StartVertexIdx + (i * TubeSegments + j);
				uint32 i1 = StartVertexIdx + (nextI * TubeSegments + j);
				uint32 i2 = StartVertexIdx + (nextI * TubeSegments + nextJ);
				uint32 i3 = StartVertexIdx + (i * TubeSegments + nextJ);

				indices.push_back(i0); indices.push_back(i1); indices.push_back(i2);
				indices.push_back(i0); indices.push_back(i2); indices.push_back(i3);
			}
		}
	}
}

void FEditorMeshLibrary::CreateScaleGizmo()
{
	TArray<FVertex>& vertices = ScaleGizmoMeshData.Vertices;
	TArray<uint32>& indices = ScaleGizmoMeshData.Indices;

	vertices.clear();
	indices.clear();

	const float LineLength = 1.0f;
	const float BoxSize = 0.05f;
	const float StemThickness = 0.03f;

	FColor colors[3] = {
		FColor(1.0f, 0.0f, 0.0f, 1.0f), // X
		FColor(0.0f, 1.0f, 0.0f, 1.0f), // Y
		FColor(0.0f, 0.0f, 1.0f, 1.0f)  // Z
	};

	FVector dirs[3] = { FVector(1,0,0), FVector(0,1,0), FVector(0,0,1) };

	auto AddBox = [&](const FVector& Center, const FVector& Extent, const FColor& Color, int SubID) {
		uint32 StartIdx = (uint32)vertices.size();
		FVector p[8] = {
			Center + FVector(-Extent.X, -Extent.Y, -Extent.Z), Center + FVector(Extent.X, -Extent.Y, -Extent.Z),
			Center + FVector(Extent.X, Extent.Y, -Extent.Z),   Center + FVector(-Extent.X, Extent.Y, -Extent.Z),
			Center + FVector(-Extent.X, -Extent.Y, Extent.Z),  Center + FVector(Extent.X, -Extent.Y, Extent.Z),
			Center + FVector(Extent.X, Extent.Y, Extent.Z),    Center + FVector(-Extent.X, Extent.Y, Extent.Z)
		};

		for (int j = 0; j < 8; ++j)
		{
			vertices.push_back({ p[j], Color, SubID });
		}

		uint32 BoxIndices[] = {
			0,2,1, 0,3,2, 4,5,6, 4,6,7,
			0,1,5, 0,5,4, 2,3,7, 2,7,6,
			0,4,7, 0,7,3, 1,2,6, 1,6,5
		};
		for (uint32 Idx : BoxIndices) indices.push_back(StartIdx + Idx);
		};

	for (int i = 0; i < 3; ++i) {
		FVector StemExtent = (i == 0) ? FVector(LineLength * 0.5f, StemThickness, StemThickness) :
			(i == 1) ? FVector(StemThickness, LineLength * 0.5f, StemThickness) :
			FVector(StemThickness, StemThickness, LineLength * 0.5f);

		AddBox(dirs[i] * (LineLength * 0.5f), StemExtent, colors[i], i);

		float boxSizeHalf = BoxSize;
		AddBox(dirs[i] * LineLength, FVector(boxSizeHalf, boxSizeHalf, boxSizeHalf), colors[i], i);
	}
}

void FEditorMeshLibrary::CreateTranslationGizmo()
{
	TArray<FVertex>& vertices = TranslationGizmoMeshData.Vertices;
	TArray<uint32>& indices = TranslationGizmoMeshData.Indices;

	vertices.clear();
	indices.clear();

	const int32 segments = 16;
	const float radius = 0.06f;
	const float headRadius = 0.12f;
	const float stemLength = 0.8f;
	const float totalLength = 1.0f;

	FColor colors[3] = {
		FColor(1.0f, 0.0f, 0.0f, 1.0f), // X
		FColor(0.0f, 1.0f, 0.0f, 1.0f), // Y
		FColor(0.0f, 0.0f, 1.0f, 1.0f)  // Z
	};

	for (int32 axis = 0; axis < 3; ++axis)
	{
		int32 axisStartVertex = (int32)vertices.size();

		auto GetRotatedPos = [&](float x, float y, float z) -> FVector {
			FVector P(x, y, z);
			if (axis == 0) return FVector(P.Z, P.X, P.Y);
			if (axis == 1) return FVector(P.X, P.Z, P.Y);
			return P;
			};

		// 링 버텍스 생성
		for (int32 i = 0; i <= segments; ++i)
		{
			float angle = (2.0f * MathUtil::PI * i) / segments;
			float c = cos(angle);
			float s = sin(angle);

			vertices.push_back({ GetRotatedPos(c * radius, s * radius, 0.0f), colors[axis], axis });
			vertices.push_back({ GetRotatedPos(c * radius, s * radius, stemLength), colors[axis], axis });
			vertices.push_back({ GetRotatedPos(c * headRadius, s * headRadius, stemLength), colors[axis], axis });
		}

		// 화살표 끝
		FVector TipPos = (axis == 0) ? FVector(totalLength, 0, 0) :
			(axis == 1) ? FVector(0, totalLength, 0) :
			FVector(0, 0, totalLength);

		vertices.push_back({ TipPos, colors[axis], axis });
		int32 tipIndex = (int32)vertices.size() - 1;

		// === 추가 1: cone 밑면 중심점 ===
		FVector baseCenterPos = (axis == 0) ? FVector(stemLength, 0, 0) :
			(axis == 1) ? FVector(0, stemLength, 0) :
			FVector(0, 0, stemLength);

		vertices.push_back({ baseCenterPos, colors[axis], axis });
		int32 baseCenterIndex = (int32)vertices.size() - 1;

		for (int32 i = 0; i < segments; ++i)
		{
			int32 curr = axisStartVertex + (i * 3);
			int32 next = axisStartVertex + ((i + 1) * 3);

			// 몸통
			indices.push_back(curr);
			indices.push_back(curr + 1);
			indices.push_back(next + 1);

			indices.push_back(curr);
			indices.push_back(next + 1);
			indices.push_back(next);

			// 몸통 끝 ~ cone 시작 연결
			indices.push_back(curr + 1);
			indices.push_back(next + 2);
			indices.push_back(curr + 2);

			indices.push_back(curr + 1);
			indices.push_back(next + 1);
			indices.push_back(next + 2);

			// cone 옆면
			indices.push_back(curr + 2);
			indices.push_back(next + 2);
			indices.push_back(tipIndex);

			// === 추가 2: cone 밑면 cap ===
			indices.push_back(baseCenterIndex);
			indices.push_back(next + 2);
			indices.push_back(curr + 2);
		}
	}
}


