#pragma once

#include "Core/CoreMinimal.h"

/*
	Vertex 구조체들을 정의하는 Header입니다.
	추후에 다양한 Vertex 구조체들을 추가할 수 있습니다.
*/

struct FVertex
{ 
	FVector Position;
	FColor	Color;
	int		SubID;
};

struct FNormalVertex
{
	FVector		Position;
	FColor		Color;
	FVector		Normal;
	FVector2	UVs;	//	TexCoord
    FVector		Tangent;
};

struct FOverlayVertex
{
	float X, Y;
};

// Position + TexCoord 범용 버텍스 (FontBatcher, SubUVBatcher 등 텍스처 기반 배처 공용)
struct FTextureVertex
{
	FVector  Position;
	FVector2 TexCoord;
};

struct FMeshData
{
	TArray<FVertex> Vertices;
	TArray<uint32> Indices;
};

namespace TangentSpace {
	void GetTangent(FVector& OutTangent, FVector& OutBitangent,
					const FVector& P0, const FVector& P1, const FVector& P2,
					const FVector2& UV0, const FVector2& UV1, const FVector2& UV2) {
		FVector Edge1 = P1 - P0;
		FVector Edge2 = P2 - P0;
		FVector2 dUV1 = UV1 - UV0;
		FVector2 dUV2 = UV2 - UV0;
        float det = dUV1.X * dUV2.Y - dUV1.Y * dUV2.X;

        float r = (fabs(det) > 1e-8f) ? (1.0f / det) : 0.0f;

        OutTangent   = (Edge1 * dUV2.Y - Edge2 * dUV1.Y) * r;
        OutBitangent = (Edge2 * dUV1.X - Edge1 * dUV2.X) * r;
	}

	float GetSign(const FVector& N, const FVector& T, const FVector& VertexB) {
		FVector B = FVector::CrossProduct(N, T);
        return (FVector::DotProduct(B, VertexB) < 0.0f) ? -1.0f : 1.0f;
	}

} // namespace TangentSpace