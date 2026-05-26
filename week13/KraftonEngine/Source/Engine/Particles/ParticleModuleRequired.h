#pragma once
#include "ParticleModule.h"
#include "Particles/ParticleEmitter.h"
#include "Object/Ptr/SoftObjectPtr.h"

enum class EParticleBlendMode : uint8
{
	AlphaBlend,
	Additive,
	Opaque,
};

class UMaterial;

UENUM()
enum class EParticleUVFlipMode : uint8
{
	/** Flips UV on all particles. */
	None,
	/** Flips UV on all particles. */
	FlipUV,
	/** Flips U only on all particles. */
	FlipUOnly,
	/** Flips V only on all particles. */
	FlipVOnly,
	/** Flips UV randomly for each particle on spawn. */
	RandomFlipUV,
	/** Flips U only randomly for each particle on spawn. */
	RandomFlipUOnly,
	/** Flips V only randomly for each particle on spawn. */
	RandomFlipVOnly,
	/** Flips U and V independently at random for each particle on spawn. */
	RandomFlipUVIndependent,
};

/** Flips the sign of a particle's base size based on it's UV flip mode. */
inline void AdjustParticleBaseSizeForUVFlipping(FVector& OutSize, EParticleUVFlipMode FlipMode, FRandomStream& InRandomStream)
{
	static const float HalfRandMax = 0.5f;

	switch (FlipMode)
	{
	case EParticleUVFlipMode::None:
		return;

	case EParticleUVFlipMode::FlipUV:
		OutSize.X = -OutSize.X;
		OutSize.Y = -OutSize.Y;
		OutSize.Z = -OutSize.Z;
		return;

	case EParticleUVFlipMode::FlipUOnly:
		OutSize.X = -OutSize.X;
		return;

	case EParticleUVFlipMode::FlipVOnly:
		OutSize.Y = -OutSize.Y;
		return;

	case EParticleUVFlipMode::RandomFlipUV:
		OutSize = InRandomStream.FRand() > HalfRandMax ? -OutSize : OutSize;
		return;

	case EParticleUVFlipMode::RandomFlipUOnly:
		OutSize.X = InRandomStream.FRand() > HalfRandMax ? -OutSize.X : OutSize.X;
		return;

	case EParticleUVFlipMode::RandomFlipVOnly:
		OutSize.Y = InRandomStream.FRand() > HalfRandMax ? -OutSize.Y : OutSize.Y;
		return;

	case EParticleUVFlipMode::RandomFlipUVIndependent:
		OutSize.X = InRandomStream.FRand() > HalfRandMax ? -OutSize.X : OutSize.X;
		OutSize.Y = InRandomStream.FRand() > HalfRandMax ? -OutSize.Y : OutSize.Y;
		return;

	default:
		// checkNoEntry();
		break;
	}
}


/**
 *	The screen alignment to utilize for the emitter at this LOD level.
 *	One of the following:
 *	PSA_FacingCameraPosition - Faces the camera position, but is not dependent on the camera rotation.
 *								This method produces more stable particles under camera rotation.
 *	PSA_Square			- Uniform scale (via SizeX) facing the camera
 *	PSA_Rectangle		- Non-uniform scale (via SizeX and SizeY) facing the camera
 *	PSA_Velocity		- Orient the particle towards both the camera and the direction
 *						  the particle is moving. Non-uniform scaling is allowed.
 *	PSA_TypeSpecific	- Use the alignment method indicated in the type data module.
 *	PSA_FacingCameraDistanceBlend - Blends between PSA_FacingCameraPosition and PSA_Square over specified distance.
 */
enum EParticleScreenAlignment : int
{
	PSA_FacingCameraPosition,
	PSA_Square,
	PSA_Rectangle,
	PSA_Velocity,
	PSA_AwayFromCenter,
	PSA_TypeSpecific,
	PSA_FacingCameraDistanceBlend,
	PSA_MAX,
};

enum EParticleSortMode : int
{
	PSORTMODE_None,
	PSORTMODE_ViewProjDepth,
	PSORTMODE_DistanceToView,
	PSORTMODE_Age_OldestFirst,
	PSORTMODE_Age_NewestFirst,
	PSORTMODE_MAX,
};

#include "Source/Engine/Particles/ParticleModuleRequired.generated.h"

UCLASS()
class UParticleModuleRequired : public UParticleModule
{
public:
	GENERATED_BODY()

	void SetMaterial(UMaterial* InMaterial);
	void ResolveMaterialFromSlot();
	void PostEditProperty(const char* PropertyName) override;
	void Serialize(FArchive& Ar) override;

	UPROPERTY(Edit, Save, Category = "Rendering", DisplayName = "Material", AssetType = "Material")
	FSoftObjectPtr MaterialSlot = "None";

	UMaterial* Material = nullptr;

	UPROPERTY(EditAnywhere, Category = "Emitter")
	FVector EmitterOrigin;

	UPROPERTY(EditAnywhere, Category = "Emitter")
	FRotator EmitterRotation;

	uint8 bUseLocalSpace : 1;
	uint8 bKillOnDeactivate : 1;
	uint8 bKillOnCompleted : 1;

	float EmitterDuration = 0.0f;
	int32 EmitterLoops = 0;

	EParticleScreenAlignment ScreenAlignment = PSA_FacingCameraPosition;
	EParticleSortMode SortMode = PSORTMODE_None;

	int32 SubImages_Horizontal = 1;
	int32 SubImages_Vertical = 1;

	float SpawnRate = 10.0f;
	TArray<FParticleBurst> BurstList;

	UPROPERTY(Edit, Save, Category = "Rendering", DisplayName = "Use Max Draw Count")
	bool bUseMaxDrawCount = false;

	UPROPERTY(Edit, Save, Category = "Rendering", DisplayName = "Max Draw Count", Min = 0, Speed = 1)
	int32 MaxDrawCount = 0;

	float EmitterDelay = 0.1f;
	float EmitterDurationLow = 0.1f;
	bool bDelayFirstLoopOnly = false;
};

