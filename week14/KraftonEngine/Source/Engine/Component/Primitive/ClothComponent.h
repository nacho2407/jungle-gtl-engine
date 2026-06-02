#pragma once

#include "Component/PrimitiveComponent.h"
#include "Object/Ptr/SoftObjectPtr.h"
#include "Physics/ClothInstance.h"

#include "Source/Engine/Component/Primitive/ClothComponent.generated.h"

class UMaterialInterface;
class USkinnedMeshComponent;
class UStaticMesh;

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
	void UpdateBoneAttachment();
	USkinnedMeshComponent* ResolveAttachMeshComponent();
	void CaptureAttachOffsetFromCurrentTransform();

	void PostEditProperty(const char* PropertyName) override;
	void PostDuplicate() override;

	void RebuildCloth(bool bRecreateRenderState);

	void SetClothMesh(UStaticMesh* InMesh);
	UStaticMesh* GetClothMesh() const { return ClothMesh; }

	void SetMaterial(UMaterialInterface* InMaterial);
	UMaterialInterface* GetMaterial() const { return Material; }

	const FClothInstance& GetClothInstance() const { return ClothInstance; }

private:
	UPROPERTY(Edit, Save, Category = "Cloth|Mesh", DisplayName = "Cloth Mesh", AssetType = "StaticMesh")
	FSoftObjectPtr ClothMeshPath = "None";

	UStaticMesh* ClothMesh = nullptr;

	UPROPERTY(Edit, Save, Category = "Materials", DisplayName = "Material", AssetType = "Material")
	FSoftObjectPtr MaterialSlot = "Content/Material/Editor/ClothDefault.mat";

	UMaterialInterface* Material = nullptr;

	UPROPERTY(Edit, Save, Category = "Cloth", DisplayName = "Cloth Setup", Type = Struct)
	FClothDesc ClothDesc;

	UPROPERTY(Edit, Save, Category = "Cloth|Collision", DisplayName = "Ignore Owner Capsule")
	bool bIgnoreOwnerCapsuleCollision = true;

	UPROPERTY(Edit, Save, Category = "Cloth|Attach", DisplayName = "Attach To Owner Mesh Bone")
	bool bAttachToOwnerMeshBone = true;

	UPROPERTY(Edit, Save, Category = "Cloth|Attach", DisplayName = "Edit Attach Offset")
	bool bEditAttachOffset = false;

	UPROPERTY(Edit, Category = "Cloth|Attach", DisplayName = "Capture Current Attach Offset")
	bool bCaptureCurrentAttachOffset = false;

	UPROPERTY(Edit, Save, Category = "Cloth|Attach", DisplayName = "Attach Bone")
	FString AttachBoneName = "spine_03";

	UPROPERTY(Edit, Save, Category = "Cloth|Attach", DisplayName = "Attach Offset Location", Type = Vec3, Speed = 0.01f)
	FVector AttachOffsetLocation = FVector::ZeroVector;

	UPROPERTY(Edit, Save, Category = "Cloth|Attach", DisplayName = "Attach Offset Rotation", Type = Rotator, Speed = 0.1f)
	FRotator AttachOffsetRotation = FRotator::ZeroRotator;

	UPROPERTY(Edit, Save, Category = "Cloth|Attach", DisplayName = "Attach Offset Scale", Type = Vec3, Speed = 0.01f)
	FVector AttachOffsetScale = FVector(1.0f, 1.0f, 1.0f);

	USkinnedMeshComponent* AttachMeshComponent = nullptr;

	FClothInstance ClothInstance;

	FVector PreviousSimulationLocation = FVector::ZeroVector;
	FQuat PreviousSimulationRotation = FQuat::Identity;
	bool bHasPreviousSimulationTransform = false;
};
