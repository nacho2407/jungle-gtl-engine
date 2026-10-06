#pragma once

#include "GameFramework/AActor.h"

#include "Source/Engine/GameFramework/Actor/PhysicalAnimationActor.generated.h"

class UPhysicalAnimationComponent;
class USkeletalMeshComponent;

UCLASS()
class APhysicalAnimationActor : public AActor
{
public:
	GENERATED_BODY()

	APhysicalAnimationActor() = default;

	void BeginPlay() override;
	void PostDuplicate() override;

	void InitDefaultComponents(
		const FString& SkeletalMeshFileName = "Content/Data/hirasawa-yui/IdleWithSkin_SkeletalMesh.uasset");

	UFUNCTION(Callable, Category = "Physics|PhysicalAnimation")
	void ActivatePhysicalAnimation();

	UFUNCTION(Callable, Category = "Physics|PhysicalAnimation")
	void DeactivatePhysicalAnimation(bool bUseRecovery = true);

	UFUNCTION(Pure, Category = "Physics|PhysicalAnimation")
	USkeletalMeshComponent* GetSkeletalMeshComponent() const { return SkeletalMeshComponent; }

	UFUNCTION(Pure, Category = "Physics|PhysicalAnimation")
	UPhysicalAnimationComponent* GetPhysicalAnimationComponent() const { return PhysicalAnimationComponent; }

protected:
	void OnOwnedComponentRemoved(UActorComponent* Component) override;

private:
	void ResolveComponents();
	void BindPhysicalAnimationTarget();

private:
	UPROPERTY(Edit, Save, Category = "Physics|PhysicalAnimation", DisplayName = "Auto Activate Physical Animation")
	bool bAutoActivatePhysicalAnimation = true;

	USkeletalMeshComponent* SkeletalMeshComponent = nullptr;
	UPhysicalAnimationComponent* PhysicalAnimationComponent = nullptr;
};
