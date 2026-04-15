#pragma once
#include "PrimitiveComponent.h"
#include "Render/Resource/MeshBufferManager.h"
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

	void SetBillboardEnabled(bool bEnable) { bIsBillboard = bEnable; }

	// --- Texture ---
	void SetTexture(const FName& InTextureName);
	const FTextureResource* GetTexture() const { return CachedTexture; }
	const FName& GetTextureName() const { return TextureName; }

	// --- Sprite Size (월드 공간) ---
	void SetSpriteSize(float InWidth, float InHeight) { Width = InWidth; Height = InHeight; }
	float GetWidth()  const { return Width; }
	float GetHeight() const { return Height; }

	// 주어진 카메라 방향으로 빌보드 월드 행렬을 계산 (per-view 렌더링용)
	FMatrix ComputeBillboardMatrix(const FVector& CameraForward) const;

	FMeshBuffer* GetMeshBuffer() const override { return &FMeshBufferManager::Get().GetMeshBuffer(EMeshShape::Quad); }
	FMeshDataView GetMeshDataView() const override { return FMeshDataView::FromMeshData(FMeshBufferManager::Get().GetMeshData(EMeshShape::Quad)); }

protected:
	bool bIsBillboard = true;

	FName TextureName;
	FTextureResource* CachedTexture = nullptr;	// ResourceManager 소유, 참조만

	float Width  = 1.0f;
	float Height = 1.0f;
};

