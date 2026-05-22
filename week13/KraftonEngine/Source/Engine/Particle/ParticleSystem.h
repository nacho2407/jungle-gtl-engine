#pragma once

#include "Object/Object.h"
#include "Core/Types/CoreTypes.h"
#include "Math/Vector.h"

#include "Source/Engine/Particle/ParticleSystem.generated.h"

class FArchive;
class UParticleEmitter;

UCLASS()
class UParticleSystem : public UObject
{
public:
    GENERATED_BODY()

    UParticleSystem()           = default;
    ~UParticleSystem() override = default;

    void SetSourcePath(const FString& InPath) { SourcePath = InPath; }

    const FString& GetSourcePath() const { return SourcePath; }

    TArray<UParticleEmitter*>& GetEmitters() { return Emitters; }

    const TArray<UParticleEmitter*>& GetEmitters() const { return Emitters; }

    void Serialize(FArchive& Ar) override;

private:
    TArray<UParticleEmitter*> Emitters;
    FString                   SourcePath;
};
