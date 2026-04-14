#pragma once

#include "Core/Singleton.h"
#include "Render/Resource/Buffer.h"
#include "Core/CoreTypes.h"

/* 수정될 구조

class FConstantBufferPool
{
	struct FBucket
	{
		uint32 BufferSize;
		std::vector<ID3D11Buffer*> FreeList;
	};

	std::vector<FBucket> Buckets; // 16, 32, 64, 128, 256, 512...

	ID3D11Device* Device;

public:
	// 요청 크기 이상인 버킷에서 꺼내줌, 없으면 새로 생성
	ID3D11Buffer* Allocate(uint32 Size);

	// 반납 (실제 해제 안 하고 FreeList에 돌려놓음)
	void Release(ID3D11Buffer* Buffer, uint32 Size);
};


*/
// 슬롯별 FConstantBuffer를 생성/관리하는 풀
// 공용 CB(Frame, PerObject)는 FRenderResources가 소유하고,
// 타입별 CB(Gizmo, Editor, Outline 등)는 이 풀에서 관리
class FConstantBufferPool : public TSingleton<FConstantBufferPool>
{
	friend class TSingleton<FConstantBufferPool>;

public:
	void Initialize(ID3D11Device* InDevice);
	void Release();

	// 슬롯별 CB 조회 (최초 호출 시 ByteWidth로 자동 생성)
	FConstantBuffer* GetBuffer(uint32 Slot, uint32 ByteWidth);

	// CB 조회 (최초 호출 시 ByteWidth로 자동 생성)
	FConstantBuffer* GetCBuffer(uint32 Index, uint32 ByteWidth);
private:
	FConstantBufferPool() = default;

	ID3D11Device* Device = nullptr;
	TMap<uint32, FConstantBuffer> Pool;

	TArray<FConstantBuffer> ConstantBuffers;
};
