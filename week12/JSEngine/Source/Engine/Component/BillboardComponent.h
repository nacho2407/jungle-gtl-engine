#pragma once
#include "PrimitiveComponent.h"
#include "Core/ResourceTypes.h"
#include "Object/FName.h"


class FViewportCamera;
struct FTextureResource;

UCLASS(SpawnableComponent, DisplayName = "Billboard Component", Category = "Basic")
class UBillboardComponent : public UPrimitiveComponent
{
protected:
	bool bIsBillboard = true;

	UPROPERTY(DisplayName = "Inherit Owner Scale")
	bool bInheritOwnerScale = false;
	bool TryGetActiveCamera(const FViewportCamera*& OutCamera) const;
	
	virtual void PostDuplicate(UObject* Original) override;

public:
	GENERATED_BODY(UBillboardComponent, UPrimitiveComponent)

	void TickComponent(float DeltaTime) override;

	void SetBillboardEnabled(bool bEnable) { bIsBillboard = bEnable; }
	void SetInheritOwnerScale(bool bInherit) { bInheritOwnerScale = bInherit; }
	bool ShouldInheritOwnerScale() const { return bInheritOwnerScale; }
	FVector GetBillboardWorldScale() const;
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
	float GetWidth()  const { return Width; }
	float GetHeight() const { return Height; }
	FColor GetColor() const { return Color; }
	void SetColor(const FColor& InColor) { Color = InColor; }
	// Billboard는 outline 미지원 (Batcher 계열)
	//void SetSpriteSize(float InWidth, float InHeight) { Width = InWidth; Height = InHeight; }

	///////////////////////////////////////////////////////////

private:
	UPROPERTY(DisplayName = "Particle")
	FName TextureName;

	UTexture* Texture = nullptr; // ResourceManager 소유, 여기선 참조만
	FColor Color = FColor::White();

protected:
	uint32 FrameIndex = 0;

	UPROPERTY(DisplayName = "Width", Min = 0.1f, Max = 100.0f, Speed = 0.1f)
	float  Width = 1.0f;

	UPROPERTY(DisplayName = "Height", Min = 0.1f, Max = 100.0f, Speed = 0.1f)
	float  Height = 1.0f;

	UPROPERTY(DisplayName = "Play Rate", Min = 1.0f, Max = 120.0f, Speed = 1.0f)
	float  PlayRate = 30.0f; // 초당 프레임 수

	float  TimeAccumulator = 0.0f;

	UPROPERTY(DisplayName = "Loop")
	bool   bLoop = true;
};

