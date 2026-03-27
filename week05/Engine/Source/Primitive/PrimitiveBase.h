#pragma once

#include "CoreMinimal.h"
#include "Renderer/PrimitiveVertex.h"
#include <d3d11.h>
#include <limits>

enum class EMeshTopology
{
	EMT_Undefined = 0, // = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED
	EMT_Point = 1,	// =  D3D11_PRIMITIVE_TOPOLOGY_POINTLIST
	EMT_LineList = 2, // = D3D11_PRIMITIVE_TOPOLOGY_LINELIST
	EMT_LineStrip = 3,	// = D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP
	EMT_TriangleList = 4, // = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST
	EMT_TriangleStrip = 5, // = D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP
};

struct ENGINE_API FMeshData
{
	FMeshData() : SortId(NextSortId++) {}
	~FMeshData() { Release(); }

	uint32 GetSortId() const { return SortId; }
	bool bIsDirty = true;	// 최초 1회 초기화 보장

	bool UpdateVertexAndIndexBuffer(ID3D11Device* Device);
	bool CreateVertexAndIndexBuffer(ID3D11Device* Device);
	void Bind(ID3D11DeviceContext* Context);
	void Release();

	// 토폴로지 옵션
	EMeshTopology Topology = EMeshTopology::EMT_Undefined;

	// CPU 데이터
	TArray<FPrimitiveVertex> Vertices;
	TArray<uint32> Indices;
	TArray<FVector2> UVs;

	// GPU 버퍼
	ID3D11Buffer* VertexBuffer = nullptr;
	ID3D11Buffer* IndexBuffer = nullptr;

	/** AABB Box Extent 및 Local Bound Radius 갱신 */
	void UpdateLocalBound();
	float GetLocalBoundRadius() const { return LocalBoundRadius; }

	FVector GetMinCoord() const { return MinCoord; }
	FVector GetMaxCoord() const { return MaxCoord; }
	FVector GetCenterCoord() const { return (MaxCoord - MinCoord) * 0.5 + MinCoord; }

private:
	uint32 SortId = 0;
	static inline uint32 NextSortId = 0;

	FVector MinCoord = FVector(FLT_MAX, FLT_MAX, FLT_MAX);
	FVector MaxCoord = FVector(-FLT_MAX, -FLT_MAX, -FLT_MAX);
	float LocalBoundRadius = 0.f;
};

class ENGINE_API CPrimitiveBase
{
public:
	CPrimitiveBase() = default;
	virtual ~CPrimitiveBase() = default;

	FMeshData* GetMeshData() const 
	{ 
		if (MeshData)
		{
			return MeshData.get();
		}

		return nullptr;
	}

	// 캐시에서 가져오거나 파일에서 로드
	static std::shared_ptr<FMeshData> LoadFromFile(const FString& Key, const FString& FilePath);
	// 캐시에서만 조회
	static std::shared_ptr<FMeshData> GetCached(const FString& Key);
	// 코드로 생성한 데이터를 캐시에 등록
	static void RegisterMeshData(const FString& Key, std::shared_ptr<FMeshData> Data);
	static void ClearCache();

	void SetPrimitiveFileName(const FString& InFileName) { PrimitiveFileName = InFileName; }
	FString GetPrimitiveFileName() const { return PrimitiveFileName; }

protected:
	std::shared_ptr<FMeshData> MeshData;

private:
	static TMap<FString, std::shared_ptr<FMeshData>> MeshCache;

	static std::shared_ptr<FMeshData> LoadFromFile(const FString& FilePath);

	FString PrimitiveFileName = "";
};
