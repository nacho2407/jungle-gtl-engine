#pragma once

#include "Render/Resource/Buffer.h"

// ============================================================
// FBatcherBase — Batcher 공통 인프라 (Dynamic VB/IB + Device 관리)
//
// 서브클래스는 CreateBuffers()로 VB/IB를 초기화하고,
// 고유 리소스(Shader, Sampler 등)는 자체 Create()에서 처리합니다.
// ============================================================
class FBatcherBase
{
public:
	virtual ~FBatcherBase() { ReleaseBuffers(); }

	FBatcherBase() = default;
	FBatcherBase(const FBatcherBase&) = delete;
	FBatcherBase& operator=(const FBatcherBase&) = delete;

protected:
	// VB/IB 생성 + Device 참조 획득 (AddRef)
	void CreateBuffers(ID3D11Device* InDevice, uint32 VBInitCount, uint32 VBStride, uint32 IBInitCount);
	// VB/IB 해제 + Device 참조 반환 (Release)
	void ReleaseBuffers();

	FDynamicVertexBuffer VB;
	FDynamicIndexBuffer IB;
	ID3D11Device* Device = nullptr;
};
