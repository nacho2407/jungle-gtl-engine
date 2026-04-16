#pragma once

#include "Core/Singleton.h"
#include "Render/Resource/Buffer.h"
#include "Core/CoreTypes.h"

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

private:
	FConstantBufferPool() = default;

	ID3D11Device* Device = nullptr;
	TMap<uint32, FConstantBuffer> Pool;
};
