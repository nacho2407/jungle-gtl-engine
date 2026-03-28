#include "PrimitiveSphere.h"
#include "Core/Paths.h"
#include "Math/MathUtility.h"
#include <cmath>

const FString CPrimitiveSphere::Key = "Sphere";

// FString CPrimitiveSphere::GetFilePath() { return FPaths::MeshDir() + "Sphere.mesh"; }

CPrimitiveSphere::CPrimitiveSphere(int32 Segments, int32 Rings)
{
	auto Cached = GetCached(Key);
	if (Cached)
	{
		MeshData = Cached;
	}
	else
	{
		Generate(Segments, Rings);
	}
}

void CPrimitiveSphere::Generate(int32 Segments, int32 Rings)
{
	auto Data = std::make_shared<FMeshData>();

	for (int32 Ring = 0; Ring <= Rings; ++Ring)
	{
		float Phi = FMath::PI * static_cast<float>(Ring) / static_cast<float>(Rings);
		float Z = cosf(Phi);
		float SinPhi = sinf(Phi);

		for (int32 Seg = 0; Seg <= Segments; ++Seg)
		{
			float Theta = FMath::TwoPi * static_cast<float>(Seg) / static_cast<float>(Segments);
			float X = SinPhi * cosf(Theta);
			float Y = SinPhi * sinf(Theta);

			FVector Position = { X * 0.5f, Y * 0.5f, Z * 0.5f };
			FVector Normal = { X, Y, Z };

			// Normal 기반 색상: 각 축 방향을 RGB로 매핑
			float R = Normal.X * 0.5f + 0.5f;
			float G = Normal.Y * 0.5f + 0.5f;
			float B = Normal.Z * 0.5f + 0.5f;
			FVector4 Color = { R, G, B, 1.0f };

			Data->Vertices.push_back({ Position, Color, Normal });
		}
	}

	for (int32 Ring = 0; Ring < Rings; ++Ring)
	{
		for (int32 Seg = 0; Seg < Segments; ++Seg)
		{
			uint32 Current = Ring * (Segments + 1) + Seg;
			uint32 Next = Current + Segments + 1;

			Data->Indices.push_back(Current);
			Data->Indices.push_back(Next);
			Data->Indices.push_back(Current + 1);

			Data->Indices.push_back(Current + 1);
			Data->Indices.push_back(Next);
			Data->Indices.push_back(Next + 1);
		}
	}

	Data->Topology = EMeshTopology::EMT_TriangleList;

	MeshData = Data;
	RegisterMeshData(Key, Data);
}
