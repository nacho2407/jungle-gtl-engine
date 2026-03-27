#include "MeshManager.h"
#include "Math/Utils.h"

#include <fstream>
#include <iostream>
#include <sstream>

FMeshData FMeshManager::CubeMeshData;
FMeshData FMeshManager::PlaneMeshData;
FMeshData FMeshManager::SphereMeshData;
FMeshData FMeshManager::TranslationGizmoMeshData;
FMeshData FMeshManager::RotationGizmoMeshData;
FMeshData FMeshManager::ScaleGizmoMeshData;
FMeshData FMeshManager::QuadMeshData;
bool FMeshManager::bIsInitialized = false;

void FMeshManager::Initialize()
{
	if (bIsInitialized) return;

	if (CubeMeshData.Vertices.empty())
	{
		CreateCube();
	}

	if (PlaneMeshData.Vertices.empty())
	{
		CreatePlane();
	}

	if (SphereMeshData.Vertices.empty())
	{
		CreateSphere();
	}

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

	if (QuadMeshData.Vertices.empty())
	{
		CreateQuad();
	}

    bIsInitialized = true;
}

void FMeshManager::CreateCube()
{
	TArray<FVertex>& vertices = CubeMeshData.Vertices;
	TArray<uint32>& indices = CubeMeshData.Indices;

	vertices.clear();
	indices.clear();

	auto MakeColor = [](float x, float y, float z) -> FVector4
		{
			// -0.5 ~ 0.5 → 0 ~ 1
			float r = (x + 0.5f);
			float g = (y + 0.5f);
			float b = (z + 0.5f);

			// 살짝 대비 주기 (optional)
			r = powf(r, 0.8f);
			g = powf(g, 0.8f);
			b = powf(b, 0.8f);

			return FVector4(r, g, b, 1.0f);
		};

	auto V = [&](float x, float y, float z)
		{
			return FVertex{ FVector(x, y, z), MakeColor(x, y, z) };
		};

	vertices = {

		// FRONT
		V(0.5f,-0.5f,-0.5f),
		V(0.5f,-0.5f, 0.5f),
		V(0.5f, 0.5f, 0.5f),
		V(0.5f, 0.5f,-0.5f),

		// BACK
		V(-0.5f,-0.5f,-0.5f),
		V(-0.5f, 0.5f,-0.5f),
		V(-0.5f, 0.5f, 0.5f),
		V(-0.5f,-0.5f, 0.5f),

		// LEFT
		V(-0.5f,-0.5f,-0.5f),
		V(-0.5f,-0.5f, 0.5f),
		V(0.5f,-0.5f, 0.5f),
		V(0.5f,-0.5f,-0.5f),

		// RIGHT
		V(-0.5f, 0.5f,-0.5f),
		V(0.5f, 0.5f,-0.5f),
		V(0.5f, 0.5f, 0.5f),
		V(-0.5f, 0.5f, 0.5f),

		// TOP
		V(-0.5f,-0.5f, 0.5f),
		V(-0.5f, 0.5f, 0.5f),
		V(0.5f, 0.5f, 0.5f),
		V(0.5f,-0.5f, 0.5f),

		// BOTTOM
		V(-0.5f,-0.5f,-0.5f),
		V(0.5f,-0.5f,-0.5f),
		V(0.5f, 0.5f,-0.5f),
		V(-0.5f, 0.5f,-0.5f),
	};

	indices = {
		0,2,1, 0,3,2,
		4,6,5, 4,7,6,
		8,10,9, 8,11,10,
		12,14,13, 12,15,14,
		16,18,17, 16,19,18,
		20,22,21, 20,23,22
	};
}

void FMeshManager::CreateSphere(int slices, int stacks)
{
	TArray<FVertex>& vertices = SphereMeshData.Vertices;
	TArray<uint32>& indices = SphereMeshData.Indices;

	vertices.clear();
	indices.clear();

	const float Radius = 0.5f;
	//FVector4 color(1.0f, 1.0f, 1.0f, 1.0f);

	//Create Vertex
	for (int i = 0; i <= stacks; ++i) {
		float pi = 3.141592f * (float)i / stacks;
		for (int j = 0; j <= slices; ++j) {
			float theta = 2.0f * 3.141592f * (float)j / slices;

			float x = Radius * sin(pi) * cos(theta);
			float y = Radius * sin(pi) * sin(theta);
			float z = Radius * cos(pi);

#if TEST

			FVector4 color(1.0f, 1.0f, 1.0f, 1.0f);

#else

			FVector4 color(
				x * 0.5f + 0.5f,
				y * 0.5f + 0.5f,
				z * 0.5f + 0.5f,
				1.0f
			);
#endif

			vertices.push_back({ {x, y, z}, color });
		}
	}

	//Create Index
	for (int i = 0; i < stacks; ++i) {
		for (int j = 0; j < slices; ++j) {
			uint32 first = i * (slices + 1) + j;
			uint32 second = first + slices + 1;

			// 구 표면의 외곽이 front face가 되도록 winding을 맞춘다.
			indices.push_back(first);
			indices.push_back(second);
			indices.push_back(first + 1);

			indices.push_back(second);
			indices.push_back(second + 1);
			indices.push_back(first + 1);
		}
	}
}

void FMeshManager::CreateRotationGizmo()
{
	TArray<FVertex>& vertices = RotationGizmoMeshData.Vertices;
	TArray<uint32>& indices = RotationGizmoMeshData.Indices;

	vertices.clear();
	indices.clear();

	const float Radius = 1.0f;
	const float Thickness = 0.03f;
	const int Segments = 64;
	const int TubeSegments = 8;

	FVector4 Colors[3] = {
		FVector4(1.0f, 0.0f, 0.0f, 1.0f), // X-Axis (Red)
		FVector4(0.0f, 1.0f, 0.0f, 1.0f), // Y-Axis (Green)
		FVector4(0.0f, 0.0f, 1.0f, 1.0f)  // Z-Axis (Blue)
	};

	// 각 축(X, Y, Z)에 대해 고리 생성
	for (int axis = 0; axis < 3; ++axis)
	{
		uint32 StartVertexIdx = (uint32)vertices.size();

		for (int i = 0; i <= Segments; ++i)
		{
			float longitude = (float)i / Segments * 2.0f * M_PI;
			float sinLong = sin(longitude);
			float cosLong = cos(longitude);

			for (int j = 0; j < TubeSegments; ++j)
			{
				float latitude = (float)j / TubeSegments * 2.0f * M_PI;
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

void FMeshManager::CreateScaleGizmo()
{
	TArray<FVertex>& vertices = ScaleGizmoMeshData.Vertices;
	TArray<uint32>& indices = ScaleGizmoMeshData.Indices;

	vertices.clear();
	indices.clear();

	const float LineLength = 1.0f;
	const float BoxSize = 0.05f;
	const float StemThickness = 0.03f;

	FVector4 colors[3] = {
		FVector4(1.0f, 0.0f, 0.0f, 1.0f), // X
		FVector4(0.0f, 1.0f, 0.0f, 1.0f), // Y
		FVector4(0.0f, 0.0f, 1.0f, 1.0f)  // Z
	};

	FVector dirs[3] = { FVector(1,0,0), FVector(0,1,0), FVector(0,0,1) };

	auto AddBox = [&](const FVector& Center, const FVector& Extent, const FVector4& Color, int SubID) {
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

void FMeshManager::CreateQuad()
{
	TArray<FVertex>& vertices = QuadMeshData.Vertices;
	TArray<uint32>& indices = QuadMeshData.Indices;

	vertices.clear();
	indices.clear();

	FVector4 DefaultColor(1.0f, 1.0f, 1.0f, 1.0f);

	// 좌상단 (X: -0.5, Z: 0.5)
	vertices.push_back({ FVector(0.0f, -0.5f,  0.5f), DefaultColor, 0 });
	// 우상단 (X: 0.5, Z: 0.5)
	vertices.push_back({ FVector(0.0f, 0.5f,  0.5f), DefaultColor, 0 });
	// 우하단 (X: 0.5, Z: -0.5)
	vertices.push_back({ FVector(0.0f, 0.5f, -0.5f), DefaultColor, 0 });
	// 좌하단 (X: -0.5, Z: -0.5)
	vertices.push_back({ FVector(0.0f, -0.5f, -0.5f), DefaultColor, 0 });

	indices.assign({ 0, 1, 2, 0, 2, 3 });
}

void FMeshManager::CreateStaticMesh()
{
	//TArray<FStaticMesh>& vertices = QuadMeshData.Vertices;
	TArray<uint32>& indices = QuadMeshData.Indices;
}

void FMeshManager::CreateTranslationGizmo()
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

	FVector4 colors[3] = {
		FVector4(1.0f, 0.0f, 0.0f, 1.0f), // X
		FVector4(0.0f, 1.0f, 0.0f, 1.0f), // Y
		FVector4(0.0f, 0.0f, 1.0f, 1.0f)  // Z
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
			float angle = (2.0f * M_PI * i) / segments;
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

void FMeshManager::CreatePlane()
{
	TArray<FVertex>& vertices = PlaneMeshData.Vertices;
	TArray<uint32>& indices = PlaneMeshData.Indices;

	vertices.clear();
	indices.clear();

	FVector4 color(1.0f, 1.0f, 1.0f, 1.0f);

	// Front face (Z = 0.01f)
	vertices = {
		{ {-0.5f, -0.5f, 0.01f}, color }, // 0
		{ {-0.5f,  0.5f, 0.01f}, color }, // 1
		{ { 0.5f,  0.5f, 0.01f}, color }, // 2
		{ { 0.5f, -0.5f, 0.01f}, color }, // 3

		// Back face (Z = -0.01f) - reversed winding for opposite normal
		{ {-0.5f, -0.5f, -0.01f}, color }, // 4
		{ { 0.5f,  0.5f, -0.01f}, color }, // 5
		{ {-0.5f,  0.5f, -0.01f}, color }, // 6
		{ { 0.5f, -0.5f, -0.01f}, color }  // 7
	};

	// Front face triangles
	indices = {
		0, 2, 1,  // Front tri 1
		0, 3, 2,  // Front tri 2

		// Back face triangles (reversed winding for correct normal)
		4, 6, 5,  // Back tri 1
		4, 5, 7   // Back tri 2
	};
}

