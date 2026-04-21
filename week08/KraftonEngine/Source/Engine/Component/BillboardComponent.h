#pragma once
#include "Render/Scene/Proxies/Primitive/PrimitiveShapeTypes.h"
#include "PrimitiveComponent.h"
#include "Render/Resources/Buffers/MeshBufferManager.h"
#include "Core/ResourceTypes.h"
#include "Object/FName.h"

class FPrimitiveSceneProxy;

class UBillboardComponent : public UPrimitiveComponent
{
public:
	DECLARE_CLASS(UBillboardComponent, UPrimitiveComponent)

	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;
	FPrimitiveSceneProxy* CreateSceneProxy() override;

	void Serialize(FArchive& Ar) override;
	void PostDuplicate() override;

	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;

	void SetBillboardEnabled(bool bEnable) { bIsBillboard = bEnable; MarkProxyDirty(EDirtyFlag::Transform); MarkWorldBoundsDirty(); }
	bool IsBillboardEnabled() const { return bIsBillboard; }

	// --- Material ---
	void SetMaterial(class UMaterial* InMaterial);
	class UMaterial* GetMaterial() const { return Material; }

	// --- Sprite Size (월드 공간) ---
	void SetSpriteSize(float InWidth, float InHeight) { Width = InWidth; Height = InHeight; }
	float GetWidth()  const { return Width; }
	float GetHeight() const { return Height; }

	// 주어진 카메라 방향으로 빌보드 월드 행렬을 계산 (per-view 렌더링용)
	FMatrix ComputeBillboardMatrix(const FVector& CameraForward) const;

	FMeshBuffer* GetMeshBuffer() const override { return &FMeshBufferManager::Get().GetMeshBuffer(EMeshShape::Quad); }
	FMeshDataView GetMeshDataView() const override { return FMeshDataView::FromMeshData(FMeshBufferManager::Get().GetMeshData(EMeshShape::Quad)); }
	void UpdateWorldAABB() const override;

protected:
	bool bIsBillboard = true;

	FMaterialSlot MaterialSlot;
	UMaterial* Material = nullptr;

	float Width  = 1.0f;
	float Height = 1.0f;
};

