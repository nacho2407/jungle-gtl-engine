#pragma once

#include "Component/PrimitiveComponent.h"
#include "Object/Ptr/SoftObjectPtr.h"
#include "Physics/ClothInstance.h"

#include "Source/Engine/Component/Primitive/ClothComponent.generated.h"

class UMaterialInterface;

UCLASS()
class UClothComponent : public UPrimitiveComponent
{
public:
	GENERATED_BODY()

	UClothComponent();
	~UClothComponent() override;

public:
	FPrimitiveSceneProxy* CreateSceneProxy() override;

	void BeginPlay() override;
	void EndPlay() override;
	
	void TickClothPostPhysics(float DeltaTime);
	void UpdateWorldAABB() const override;

	void UpdateClothWorldCollision();

	void PostEditProperty(const char* PropertyName) override;
	void PostDuplicate() override;

	void RebuildCloth(bool bRecreateRenderState);

	void SetMaterial(UMaterialInterface* InMaterial);
	UMaterialInterface* GetMaterial() const { return Material; }

	const FClothInstance& GetClothInstance() const { return ClothInstance; }

private:
	UPROPERTY(Edit, Save, Category = "Materials", DisplayName = "Material", AssetType = "Material")
	FSoftObjectPtr MaterialSlot = "Content/Material/Editor/ClothDefault.mat";

	UMaterialInterface* Material = nullptr;

	UPROPERTY(Edit, Save, Category = "Cloth", DisplayName = "Cloth Setup", Type = Struct)
	FClothDesc ClothDesc;

	FClothInstance ClothInstance;
};
