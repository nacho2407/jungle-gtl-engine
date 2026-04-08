#pragma once
#include "PrimitiveComponent.h"

struct FDynamicMesh;

class ENGINE_API UBillboardComponent : public UPrimitiveComponent
{
public:
	DECLARE_RTTI(UBillboardComponent, UPrimitiveComponent)

	void PostConstruct() override;

	virtual bool UseSpherePicking() const override { return true; }
	virtual FBoxSphereBounds GetWorldBounds() const override;
	virtual FRenderMesh* GetRenderMesh() const override;

	void SetSize(const FVector2& InSize) { Size = InSize; }
	const FVector2& GetSize() const { return Size; }

	void SetTexturePath(const std::wstring& InPath) { TexturePath = InPath; }
	const std::wstring& GetTexturePath() const { return TexturePath; }

	void SetUVMin(const FVector2& InUVMin) { UVMin = InUVMin; }
	const FVector2& GetUVMin() const { return UVMin; }

	void SetUVMax(const FVector2& InUVMax) { UVMax = InUVMax; }
	const FVector2& GetUVMax() const { return UVMax; }

	FDynamicMesh* GetBillboardMesh() const { return BillboardMesh.get(); }

private:
	std::wstring TexturePath;

	FVector2 Size = FVector2(1.f, 1.f);
	FVector2 UVMin = FVector2(0.f, 0.f);
	FVector2 UVMax = FVector2(1.f, 1.f);

	std::shared_ptr<struct FDynamicMesh> BillboardMesh;
};