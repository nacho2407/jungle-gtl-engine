#pragma once

#include "Object/Object.h"
#include "Core/Types/CoreTypes.h"
#include "Math/Vector.h"

#include "Source/Engine/Particles/ParticleSystem.generated.h"

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

    UPROPERTY(Edit, Save, Category="LOD", DisplayName="LOD Distances")
    TArray<float> LODDistances;

    void AddReferencedObjects(FReferenceCollector& Collector) override;

    // System update / warmup. Cascade의 ParticleSystemUpdateMode 와 동일.
    UPROPERTY(Edit, Save, Category="ParticleSystem", DisplayName="System Update Mode")
    int32 SystemUpdateMode = 0;   // 0 = EPSUM_RealTime, 1 = EPSUM_FixedTime
    UPROPERTY(Edit, Save, Category="ParticleSystem", DisplayName="Update Time FPS")
    float UpdateTimeFPS = 60.0f;
    UPROPERTY(Edit, Save, Category="ParticleSystem", DisplayName="Warmup Time")
    float WarmupTime = 0.0f;
    UPROPERTY(Edit, Save, Category="ParticleSystem", DisplayName="Warmup Tick Rate")
    float WarmupTickRate = 0.0f;
    UPROPERTY(Edit, Save, Category="ParticleSystem", DisplayName="Orient ZAxis Toward Camera")
    bool bOrientZAxisTowardCamera = false;
    UPROPERTY(Edit, Save, Category="ParticleSystem", DisplayName="Seconds Before Inactive")
    float SecondsBeforeInactive = 0.0f;

    // Thumbnail.
    UPROPERTY(Edit, Save, Category="Thumbnail", DisplayName="Thumbnail Warmup")
    float ThumbnailWarmup = 1.0f;
    UPROPERTY(Edit, Save, Category="Thumbnail", DisplayName="Use Realtime Thumbnail")
    bool bUseRealtimeThumbnail = false;

    // LOD policy.
    UPROPERTY(Edit, Save, Category="LOD", DisplayName="LOD Distance Check Time")
    float LODDistanceCheckTime = 0.25f;
    UPROPERTY(Edit, Save, Category="LOD", DisplayName="LOD Method")
    int32 LODMethod = 0;   // 0 = Automatic, 1 = DirectSet, 2 = ActivateAutomatic

private:
    TArray<UParticleEmitter*> Emitters;
    FString                   SourcePath;
};
