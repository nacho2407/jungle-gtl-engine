#pragma once
#include "ParticleModuleColorBase.h"
#include "Core/Types/EngineTypes.h"

#include "Source/Engine/Particles/Color/ParticleModuleColor.generated.h"

UCLASS()
class UParticleModuleColor : public UParticleModuleColorBase
{
public:
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Color")
	FColor StartColor;
	UPROPERTY(EditAnywhere, Category = "Color")
	float StartAlpha = 0;

	uint8 bClampAlpha : 1;

	virtual void Spawn(const FSpawnContext& Context) override;

#if WITH_EDITOR
	virtual	void PostEditChangeProperty(const FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
};
