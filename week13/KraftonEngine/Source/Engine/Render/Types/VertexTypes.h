#pragma once

#include "Math/Vector.h"
#include "Render/Types/RenderTypes.h"
#include <cassert>

struct FVertex
{
	FVector Position;
	FVector4 Color;
	int SubID;
};

struct FOverlayVertex
{
	float X, Y;
};

// Position + TexCoord 범용 버텍스 (FFontGeometry 등 텍스처 기반 동적 지오메트리 공용)
struct FTextureVertex
{
	FVector  Position;
	FVector2 TexCoord;
};

// Position + Normal + Color + UV (StaticMesh GPU용 정점 형식)
struct FVertexPNCT
{
	FVector Position;
	FVector Normal;
	FVector4 Color;
	FVector2 UV;
};

// Position + Normal + Tangent + Color + UV(StaticMesh GPU용 정점 형식)
struct FVertexPNCTT
{
	FVector Position;
	FVector Normal;
	FVector4 Color;
	FVector2 UV;
	FVector4 Tangent;
};

template<typename VertexType>
struct TMeshData
{
	TArray<VertexType> Vertices;
	TArray<uint32> Indices;
};

using FMeshData = TMeshData<FVertex>;

// 정점 타입에 무관하게 메시 데이터를 참조하는 뷰.
// 모든 정점 구조체는 FVector Position을 첫 번째 멤버(offset 0)로 가져야 한다.
struct FMeshDataView
{
	const void*   VertexData  = nullptr;
	const uint32* IndexData   = nullptr;
	uint32 VertexCount = 0;
	uint32 IndexCount  = 0;
	uint32 Stride      = 0;

	bool IsValid() const { return VertexData && IndexCount > 0; }
	uint32 GetTriangleCount() const { return IndexCount / 3; }

	// N번째 정점을 T 타입으로 반환
	template<typename T>
	const T& GetVertex(uint32 Index) const
	{
		assert(sizeof(T) == Stride && "GetVertex<T>: sizeof(T) must match Stride");
		return *reinterpret_cast<const T*>(
			static_cast<const uint8*>(VertexData) + Index * Stride);
	}

	// Position은 모든 정점 타입의 offset 0에 있으므로 타입 없이 접근 가능
	const FVector& GetPosition(uint32 Index) const
	{
		return *reinterpret_cast<const FVector*>(
			static_cast<const uint8*>(VertexData) + Index * Stride);
	}

	// N번째 삼각형의 세 정점 인덱스를 반환
	void GetTriangleIndices(uint32 TriIndex, uint32& OutI0, uint32& OutI1, uint32& OutI2) const
	{
		assert(TriIndex * 3 + 2 < IndexCount && "GetTriangleIndices: TriIndex out of range");
		OutI0 = IndexData[TriIndex * 3];
		OutI1 = IndexData[TriIndex * 3 + 1];
		OutI2 = IndexData[TriIndex * 3 + 2];
	}

	template<typename VertexType>
	static FMeshDataView FromMeshData(const TMeshData<VertexType>& Data)
	{
		FMeshDataView View;
		if (!Data.Vertices.empty())
		{
			View.VertexData  = Data.Vertices.data();
			View.VertexCount = (uint32)Data.Vertices.size();
			View.Stride      = sizeof(VertexType);
		}
		if (!Data.Indices.empty())
		{
			View.IndexData  = Data.Indices.data();
			View.IndexCount = (uint32)Data.Indices.size();
		}
		return View;
	}
};
