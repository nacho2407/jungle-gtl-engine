#pragma once
#include "PrimitiveComponent.h"
#include "Core/ResourceTypes.h"
#include "Object/FName.h"

#include "BillboardComponent.generated.h"


class FViewportCamera;
struct FTextureResource;

UCLASS()
class UBillboardComponent : public UPrimitiveComponent
{
    GENERATED_BODY_UBillboardComponent()
protected:
	bool bIsBillboard = true;
    UPROPERTY(EditAnywhere, Category="Transform", DisplayName="Inherit Owner Scale")
	bool bInheritOwnerScale = false;
	bool TryGetActiveCamera(const FViewportCamera*& OutCamera) const;
	
	virtual void PostDuplicate(UObject* Original) override;

public:
	DECLARE_CLASS(UBillboardComponent, UPrimitiveComponent)

	virtual void Serialize(FArchive& Ar) override;

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
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	float GetWidth()  const { return Width; }
	float GetHeight() const { return Height; }
	FColor GetColor() const { return Color; }
	void SetColor(const FColor& InColor) { Color = InColor; }
	// Billboard는 outline 미지원 (Batcher 계열)
	//void SetSpriteSize(float InWidth, float InHeight) { Width = InWidth; Height = InHeight; }

	///////////////////////////////////////////////////////////

private:
    UPROPERTY(EditAnywhere, Category="Asset", DisplayName="Texture")
	FName TextureName;
	UTexture* Texture = nullptr; // ResourceManager 소유, 여기선 참조만
    UPROPERTY(EditAnywhere, Category="Rendering", DisplayName="Color")
	FColor Color = FColor::White();

protected:
	uint32 FrameIndex = 0;
    UPROPERTY(EditAnywhere, Category="Rendering", DisplayName="Width")
	float  Width = 1.0f;
    UPROPERTY(EditAnywhere, Category="Rendering", DisplayName="Height")
	float  Height = 1.0f;
    UPROPERTY(EditAnywhere, Category="Animation", DisplayName="Play Rate")
	float  PlayRate = 30.0f; // 초당 프레임 수
	float  TimeAccumulator = 0.0f;
    UPROPERTY(EditAnywhere, Category="Animation", DisplayName="Loop")
	bool   bLoop = true;
};

