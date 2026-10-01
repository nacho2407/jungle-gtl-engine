#include "HitReactionComponent.h"
#include "Engine/Serialization/Archive.h"
#include "GameFramework/AActor.h"
#include <algorithm>
#include <cmath>
#include <cstring>

IMPLEMENT_COMPONENT_CLASS(UHitReactionComponent, UActorComponent, EEditorComponentCategory::Movement)

void UHitReactionComponent::ApplyKnockback(FVector HitFromPosition, float KnockbackPower)
{
    if (!Owner) return;

    FVector HitDirection = Owner->GetActorLocation() - HitFromPosition;
    if (HitDirection.Length() <= 0.0f)
    {
        return;
    }

    HitDirection.Normalize();
    KnockbackVelocity = HitDirection * KnockbackPower;
}

void UHitReactionComponent::ApplySquash(FVector SquashScaleRatio, float Duration)
{
    if (!Owner) return;
    if (Duration <= 0.0f) return;

    OriginalScale = Owner->GetActorScale();

    SquashScale = {
        OriginalScale.X * SquashScaleRatio.X,
        OriginalScale.Y * SquashScaleRatio.Y,
        OriginalScale.Z * SquashScaleRatio.Z
    };

    SquashDuration = Duration;
    SquashElapsedTime = 0.0f;
    bIsSquashing = true;

    Owner->SetActorScale(SquashScale);
}

void UHitReactionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
    UActorComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);

    UpdateKnockback(DeltaTime);
    UpdateSquash(DeltaTime);
}

void UHitReactionComponent::Serialize(FArchive& Ar)
{
    UActorComponent::Serialize(Ar);

    Ar << KnockbackDamping;
}

void UHitReactionComponent::PostDuplicate()
{
    UActorComponent::PostDuplicate();

    // 충돌 된 처리를 복제하면 안되므로 0으로 초기화
    KnockbackVelocity = FVector::ZeroVector;

    SquashElapsedTime = 0.0f;
    SquashDuration = 0.0f;
    OriginalScale = FVector::ZeroVector;
    SquashScale = FVector::ZeroVector;
    bIsSquashing = false;
}

void UHitReactionComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UActorComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "Knockback Damping", EPropertyType::Float, &KnockbackDamping });
}

void UHitReactionComponent::PostEditProperty(const char* PropertyName)
{
    UActorComponent::PostEditProperty(PropertyName);

    if (std::strcmp(PropertyName, "Knockback Damping") == 0)
    {
        if (KnockbackDamping < 0.0f)
        {
            KnockbackDamping = 0.0f;
        }
    }
}

void UHitReactionComponent::UpdateKnockback(float DeltaTime)
{
    if (!Owner) return;
    if (KnockbackVelocity.Length() <= 0.01f)
    {
        KnockbackVelocity = FVector::ZeroVector;
        return;
    }

    FVector OriginownerLocation = Owner->GetActorLocation();
    FVector KnockbackLocation = OriginownerLocation + KnockbackVelocity * DeltaTime;
    Owner->SetActorLocation(KnockbackLocation);

    KnockbackVelocity *= std::exp(-KnockbackDamping * DeltaTime);
}

void UHitReactionComponent::UpdateSquash(float Deltatime)
{
    if (!Owner) return;
    if (!bIsSquashing) return;

    SquashElapsedTime += Deltatime;

    float Alpha = SquashElapsedTime / SquashDuration;
    Alpha = std::clamp(Alpha, 0.0f, 1.0f);

    FVector NewScale = SquashScale + (OriginalScale - SquashScale) * Alpha;
    Owner->SetActorScale(NewScale);

    if (Alpha >= 1.0f)
    {
        Owner->SetActorScale(OriginalScale);
        bIsSquashing = false;
    }
}
