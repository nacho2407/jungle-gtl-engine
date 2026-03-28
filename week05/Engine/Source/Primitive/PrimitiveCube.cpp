#include "PrimitiveCube.h"
#include "Core/Paths.h"

const FString CPrimitiveCube::Key = "Cube";

// FString CPrimitiveCube::GetFilePath() { return FPaths::MeshDir() + "Cube.mesh"; }

CPrimitiveCube::CPrimitiveCube()
{
	auto Cached = GetCached(Key);
	if (Cached)
	{
		MeshData = Cached;
	}
	else
	{
		Generate();
	}
}

void CPrimitiveCube::Generate()
{
	auto Data = std::make_shared<FMeshData>();

	FVector4 Red = { 1.0f, 0.3f, 0.3f, 1.0f };
	FVector4 Green = { 0.3f, 1.0f, 0.3f, 1.0f };
	FVector4 Blue = { 0.3f, 0.3f, 1.0f, 1.0f };
	FVector4 Yellow = { 1.0f, 1.0f, 0.3f, 1.0f };
	FVector4 Cyan = { 0.3f, 1.0f, 1.0f, 1.0f };
	FVector4 Magenta = { 1.0f, 0.3f, 1.0f, 1.0f };

	// Front face (x = +0.5) — Red
	Data->Vertices.push_back({ {  0.5f, -0.5f, -0.5f }, Red, {  1.0f,  0.0f,  0.0f } });
	Data->Vertices.push_back({ {  0.5f,  0.5f, -0.5f }, Red, {  1.0f,  0.0f,  0.0f } });
	Data->Vertices.push_back({ {  0.5f,  0.5f,  0.5f }, Red, {  1.0f,  0.0f,  0.0f } });
	Data->Vertices.push_back({ {  0.5f, -0.5f,  0.5f }, Red, {  1.0f,  0.0f,  0.0f } });

	// Back face (x = -0.5) — Green
	Data->Vertices.push_back({ { -0.5f,  0.5f, -0.5f }, Green, { -1.0f,  0.0f,  0.0f } });
	Data->Vertices.push_back({ { -0.5f, -0.5f, -0.5f }, Green, { -1.0f,  0.0f,  0.0f } });
	Data->Vertices.push_back({ { -0.5f, -0.5f,  0.5f }, Green, { -1.0f,  0.0f,  0.0f } });
	Data->Vertices.push_back({ { -0.5f,  0.5f,  0.5f }, Green, { -1.0f,  0.0f,  0.0f } });

	// Top face (z = +0.5) — Blue
	Data->Vertices.push_back({ { -0.5f, -0.5f,  0.5f }, Blue, {  0.0f,  0.0f,  1.0f } });
	Data->Vertices.push_back({ {  0.5f, -0.5f,  0.5f }, Blue, {  0.0f,  0.0f,  1.0f } });
	Data->Vertices.push_back({ {  0.5f,  0.5f,  0.5f }, Blue, {  0.0f,  0.0f,  1.0f } });
	Data->Vertices.push_back({ { -0.5f,  0.5f,  0.5f }, Blue, {  0.0f,  0.0f,  1.0f } });

	// Bottom face (z = -0.5) — Yellow
	Data->Vertices.push_back({ { -0.5f,  0.5f, -0.5f }, Yellow, {  0.0f,  0.0f, -1.0f } });
	Data->Vertices.push_back({ {  0.5f,  0.5f, -0.5f }, Yellow, {  0.0f,  0.0f, -1.0f } });
	Data->Vertices.push_back({ {  0.5f, -0.5f, -0.5f }, Yellow, {  0.0f,  0.0f, -1.0f } });
	Data->Vertices.push_back({ { -0.5f, -0.5f, -0.5f }, Yellow, {  0.0f,  0.0f, -1.0f } });

	// Right face (y = +0.5) — Cyan
	Data->Vertices.push_back({ {  0.5f,  0.5f, -0.5f }, Cyan, {  0.0f,  1.0f,  0.0f } });
	Data->Vertices.push_back({ { -0.5f,  0.5f, -0.5f }, Cyan, {  0.0f,  1.0f,  0.0f } });
	Data->Vertices.push_back({ { -0.5f,  0.5f,  0.5f }, Cyan, {  0.0f,  1.0f,  0.0f } });
	Data->Vertices.push_back({ {  0.5f,  0.5f,  0.5f }, Cyan, {  0.0f,  1.0f,  0.0f } });

	// Left face (y = -0.5) — Magenta
	Data->Vertices.push_back({ { -0.5f, -0.5f, -0.5f }, Magenta, {  0.0f, -1.0f,  0.0f } });
	Data->Vertices.push_back({ {  0.5f, -0.5f, -0.5f }, Magenta, {  0.0f, -1.0f,  0.0f } });
	Data->Vertices.push_back({ {  0.5f, -0.5f,  0.5f }, Magenta, {  0.0f, -1.0f,  0.0f } });
	Data->Vertices.push_back({ { -0.5f, -0.5f,  0.5f }, Magenta, {  0.0f, -1.0f,  0.0f } });

	// 36 indices (6 faces * 2 triangles * 3 vertices)
	for (uint32 i = 0; i < 6; ++i)
	{
		uint32 Base = i * 4;
		Data->Indices.push_back(Base + 0);
		Data->Indices.push_back(Base + 1);
		Data->Indices.push_back(Base + 2);
		Data->Indices.push_back(Base + 0);
		Data->Indices.push_back(Base + 2);
		Data->Indices.push_back(Base + 3);
	}

	Data->Topology = EMeshTopology::EMT_TriangleList;
	MeshData = Data;
	RegisterMeshData(Key, Data);

}
