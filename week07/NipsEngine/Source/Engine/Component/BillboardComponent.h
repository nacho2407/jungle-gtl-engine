#pragma once
#include "PrimitiveComponent.h"
#include "Core/ResourceTypes.h"
#include "Object/FName.h"


class FViewportCamera;
struct FTextureResource;

class UBillboardComponent : public UPrimitiveComponent
{
protected:
	bool bIsBillboard = true;
	bool TryGetActiveCamera(const FViewportCamera*& OutCamera) const;
	
	virtual void PostDuplicate(UObject* Original) override;

public:
	DECLARE_CLASS(UBillboardComponent, UPrimitiveComponent)

	void TickComponent(float DeltaTime) override;

	void SetBillboardEnabled(bool bEnable) { bIsBillboard = bEnable; }
	static constexpr EPrimitiveType PrimitiveType = EPrimitiveType::EPT_Billboard;

	static FMatrix MakeBillboardWorldMatrix(
		const FVector& WorldLocation,
		const FVector& WorldScale,
		const FVector& CameraForward,
		const FVector& CameraRight,
		const FVector& CameraUp);

	EPrimitiveType GetPrimitiveType() const override { return PrimitiveType; }

	void SetTextureName(FString InName);
	FString GetTextureName();
	UTexture* GetTexture();

	//////////////////// override ////////////////////////////
	void UpdateWorldAABB() const override;
	bool RaycastMesh(const FRay& Ray, FHitResult& OutHitResult) override;
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	float GetWidth()  const { return Width; }
	float GetHeight() const { return Height; }
	// Billboard는 outline 미지원 (Batcher 계열)
	//void SetSpriteSize(float InWidth, float InHeight) { Width = InWidth; Height = InHeight; }

	///////////////////////////////////////////////////////////

private:
	FName TextureName;
	UTexture* Texture = nullptr; // ResourceManager 소유, 여기선 참조만

protected:
	uint32 FrameIndex = 0;
	float  Width = 1.0f;
	float  Height = 1.0f;
	float  PlayRate = 30.0f; // 초당 프레임 수
	float  TimeAccumulator = 0.0f;
	bool   bLoop = true;
};

