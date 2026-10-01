#pragma once
#include "ActorComponent.h"
#include "Math/Vector.h"


class UHitReactionComponent : public UActorComponent
{
public:
    DECLARE_CLASS(UHitReactionComponent, UActorComponent)

    void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

    void Serialize(FArchive& Ar) override;
    void PostDuplicate() override;

    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void PostEditProperty(const char* PropertyName) override;

    void ApplyKnockback(FVector HitFromPosition, float KnockbackPower);
    void ApplySquash(FVector SquashScaleRatio, float Duration);

    void SetKnockbackDamping(float InKnockbackDamping) { KnockbackDamping = InKnockbackDamping; }


private:
    void UpdateKnockback(float DeltaTime);
    void UpdateSquash(float DeltaTime);

    float KnockbackDamping = 8.0f;
    FVector KnockbackVelocity;

    float SquashElapsedTime = 0.0f;
    float SquashDuration = 0.0f;
    FVector OriginalScale;
    FVector SquashScale;
    bool bIsSquashing = false;
};
