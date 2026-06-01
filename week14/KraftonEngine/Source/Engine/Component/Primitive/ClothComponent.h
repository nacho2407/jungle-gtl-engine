#pragma once

#include "Component/PrimitiveComponent.h"
#include "Physics/ClothInstance.h"

#include "Source/Engine/Component/Primitive/ClothComponent.generated.h"

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

	const FClothInstance& GetClothInstance() const { return ClothInstance; }

private:
	UPROPERTY(Edit, Save, Category = "Cloth", DisplayName = "Cloth Setup", Type = Struct)
	FClothDesc ClothDesc;

	FClothInstance ClothInstance;
};
