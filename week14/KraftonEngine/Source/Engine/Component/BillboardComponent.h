#pragma once
#include "PrimitiveComponent.h"
#include "Render/Resource/MeshBufferManager.h"

class FPrimitiveSceneProxy;

class UBillboardComponent : public UPrimitiveComponent
{
public:
	DECLARE_CLASS(UBillboardComponent, UPrimitiveComponent)

	void TickComponent(float DeltaTime) override;
	FPrimitiveSceneProxy* CreateSceneProxy() override;

	void Serialize(FArchive& Ar) override;

	void SetBillboardEnabled(bool bEnable) { bIsBillboard = bEnable; }

	// 주어진 카메라 방향으로 빌보드 월드 행렬을 계산 (per-view 렌더링용)
	FMatrix ComputeBillboardMatrix(const FVector& CameraForward) const;

	FMeshBuffer* GetMeshBuffer() const override { return &FMeshBufferManager::Get().GetMeshBuffer(EMeshShape::Quad); }
	const FMeshData* GetMeshData() const override { return &FMeshBufferManager::Get().GetMeshData(EMeshShape::Quad); }

protected:
	bool bIsBillboard = true;
};

