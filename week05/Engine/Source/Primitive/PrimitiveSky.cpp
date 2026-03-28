#include "PrimitiveSky.h"
#include "Core/Paths.h"
#include "Math/MathUtility.h"
#include <cmath>
const FString CPrimitiveSky::Key = "SkySphere";



CPrimitiveSky::CPrimitiveSky(int32 Segments, int32 Rings)
{
	auto Cached = GetCached(Key);
	if (Cached)
	{
		MeshData = Cached;
	}
	else
		Generate(Segments, Rings);
}

void CPrimitiveSky::Generate(int32 Segments, int32 Rings)
{
	auto Data = std::make_shared<FMeshData>();

	const float Radius = 0.5f;
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

			FVector Position = { X * Radius, Y * Radius, Z * Radius };
			// Normal reverse
			FVector Normal = { -X, -Y, -Z };
			FVector4 Color = { 1.0f, 1.0f, 1.0f, 1.0f };

			Data->Vertices.push_back({ Position, Color, Normal });
		}
	}

	// counter clock wise
	for (int32 Ring = 0; Ring < Rings; ++Ring)
	{
		for (int32 Seg = 0; Seg < Segments; ++Seg)
		{
			uint32 Current = Ring * (Segments + 1) + Seg;
			uint32 Next = Current + Segments + 1;

			// Current+1, Next, Current
			Data->Indices.push_back(Current);
			Data->Indices.push_back(Current + 1);
			Data->Indices.push_back(Next);

			Data->Indices.push_back(Current + 1);
			Data->Indices.push_back(Next + 1);
			Data->Indices.push_back(Next);
		}
	}
	Data->Topology = EMeshTopology::EMT_TriangleList;
	MeshData = Data;
	RegisterMeshData(Key, Data);
}
