#include "Mesh/Importer/Fbx/FbxTangentBuilder.h"
#include "Math/MathUtils.h"

#include <algorithm>
#include <cmath>

void FFbxTangentBuilder::AccumulateSkeletalTriangle(FFbxImportContext& Context, const uint32 TriIndices[3])
{
	const FVertexPNCTBW& V0 = Context.SkeletalVertices[TriIndices[0]];
	const FVertexPNCTBW& V1 = Context.SkeletalVertices[TriIndices[1]];
	const FVertexPNCTBW& V2 = Context.SkeletalVertices[TriIndices[2]];

	const FVector Edge1 = V1.Position - V0.Position;
	const FVector Edge2 = V2.Position - V0.Position;
	const FVector2 DeltaUV1 = V1.UV - V0.UV;
	const FVector2 DeltaUV2 = V2.UV - V0.UV;

	const float Det = DeltaUV1.X * DeltaUV2.Y - DeltaUV1.Y * DeltaUV2.X;
	if (std::abs(Det) >= 1e-8f)
	{
		const float InvDet = 1.0f / Det;
		const FVector Tangent = (Edge1 * DeltaUV2.Y - Edge2 * DeltaUV1.Y) * InvDet;
		const FVector Bitangent = (Edge2 * DeltaUV1.X - Edge1 * DeltaUV2.X) * InvDet;

		Context.TangentSums[TriIndices[0]] += Tangent;
		Context.TangentSums[TriIndices[1]] += Tangent;
		Context.TangentSums[TriIndices[2]] += Tangent;

		Context.BitangentSums[TriIndices[0]] += Bitangent;
		Context.BitangentSums[TriIndices[1]] += Bitangent;
		Context.BitangentSums[TriIndices[2]] += Bitangent;
	}
}

void FFbxTangentBuilder::BuildSkeletalTangentsForVertexRange(FFbxImportContext& Context, uint32 VertexStart)
{
	for (uint32 Index = VertexStart; Index < static_cast<uint32>(Context.SkeletalVertices.size()); ++Index)
	{
		FVector N = Context.SkeletalVertices[Index].Normal;
		FVector T = Context.TangentSums[Index];

		T = T - N * N.Dot(T);
		if (T.Length() < FMath::Epsilon)
		{
			const FVector Axis = std::abs(N.Z) < 0.999f ? FVector(0.0f, 0.0f, 1.0f) : FVector(0.0f, 1.0f, 0.0f);
			T = Axis.Cross(N).Normalized();
		}
		else
		{
			T.Normalize();
		}

		const FVector B = Context.BitangentSums[Index];
		const float Handedness = N.Cross(T).Dot(B) < 0.0f ? -1.0f : 1.0f;
		Context.SkeletalVertices[Index].Tangent = FVector4(T, Handedness);
	}
}

void FFbxTangentBuilder::AccumulateStaticTriangle(const FStaticMesh& Mesh, const uint32 TriIndices[3], TArray<FVector>& TangentSums, TArray<FVector>& BitangentSums)
{
	const FNormalVertex& V0 = Mesh.Vertices[TriIndices[0]];
	const FNormalVertex& V1 = Mesh.Vertices[TriIndices[1]];
	const FNormalVertex& V2 = Mesh.Vertices[TriIndices[2]];

	const FVector Edge1 = V1.pos - V0.pos;
	const FVector Edge2 = V2.pos - V0.pos;
	const FVector2 DeltaUV1 = V1.tex - V0.tex;
	const FVector2 DeltaUV2 = V2.tex - V0.tex;

	const float Det = DeltaUV1.X * DeltaUV2.Y - DeltaUV1.Y * DeltaUV2.X;
	if (std::abs(Det) >= 1e-8f)
	{
		const float InvDet = 1.0f / Det;
		const FVector Tangent = (Edge1 * DeltaUV2.Y - Edge2 * DeltaUV1.Y) * InvDet;
		const FVector Bitangent = (Edge2 * DeltaUV1.X - Edge1 * DeltaUV2.X) * InvDet;

		for (int32 CornerIndex = 0; CornerIndex < 3; ++CornerIndex)
		{
			const uint32 TriIndex = TriIndices[CornerIndex];
			TangentSums[TriIndex] += Tangent;
			BitangentSums[TriIndex] += Bitangent;
		}
	}
}

void FFbxTangentBuilder::FinalizeStaticTangents(FStaticMesh& Mesh, const TArray<FVector>& TangentSums, const TArray<FVector>& BitangentSums)
{
	for (uint32 VertexIndex = 0; VertexIndex < static_cast<uint32>(Mesh.Vertices.size()); ++VertexIndex)
	{
		FNormalVertex& Vertex = Mesh.Vertices[VertexIndex];
		FVector N = Vertex.normal.Normalized();
		FVector T = TangentSums[VertexIndex];
		T = T - N * N.Dot(T);

		if (T.Length() < 1e-8f)
		{
			const FVector Axis = std::abs(N.Z) < 0.999f ? FVector(0.0f, 0.0f, 1.0f) : FVector(0.0f, 1.0f, 0.0f);
			T = Axis.Cross(N).Normalized();
		}
		else
		{
			T.Normalize();
		}

		const FVector B = BitangentSums[VertexIndex];
		const float Handedness = N.Cross(T).Dot(B) < 0.0f ? -1.0f : 1.0f;
		Vertex.tangent = FVector4(T, Handedness);
	}
}
