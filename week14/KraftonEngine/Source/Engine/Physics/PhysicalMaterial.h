#pragma once

#include "Object/Object.h"

#include "Source/Engine/Physics/PhysicalMaterial.generated.h"

UENUM()
enum class EPhysicalMaterialCombineMode : uint8
{
	Average,
	Minimum,
	Multiply,
	Maximum
};

UCLASS()
class UPhysicalMaterial : public UObject
{
public:
	GENERATED_BODY()

	void SetAssetPathFileName(const FString& InPathFileName) { AssetPathFileName = InPathFileName; }
	const FString& GetAssetPathFileName() const { return AssetPathFileName; }

	float GetFriction() const { return Friction; }
	float GetRestitution() const { return Restitution; }
	float GetDensity() const { return Density; }
	EPhysicalMaterialCombineMode GetFrictionCombineMode() const { return FrictionCombineMode; }
	EPhysicalMaterialCombineMode GetRestitutionCombineMode() const { return RestitutionCombineMode; }

	void Serialize(FArchive& Ar) override;

private:
	FString AssetPathFileName = "None";

	UPROPERTY(Edit, Save, Category="Physical Material", DisplayName="Friction", Min=0.0f, Max=10.0f, Speed=0.01f)
	float Friction = 0.7f;

	UPROPERTY(Edit, Save, Category="Physical Material", DisplayName="Restitution", Min=0.0f, Max=1.0f, Speed=0.01f)
	float Restitution = 0.3f;

	UPROPERTY(Edit, Save, Category="Physical Material", DisplayName="Density", Min=0.0f, Max=1000.0f, Speed=0.01f)
	float Density = 1.0f;

	UPROPERTY(Edit, Save, Category="Physical Material", DisplayName="Friction Combine Mode", Enum=EPhysicalMaterialCombineMode)
	EPhysicalMaterialCombineMode FrictionCombineMode = EPhysicalMaterialCombineMode::Average;

	UPROPERTY(Edit, Save, Category="Physical Material", DisplayName="Restitution Combine Mode", Enum=EPhysicalMaterialCombineMode)
	EPhysicalMaterialCombineMode RestitutionCombineMode = EPhysicalMaterialCombineMode::Average;
};
