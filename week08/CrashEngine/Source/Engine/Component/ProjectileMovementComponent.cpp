// 컴포넌트 영역의 세부 동작을 구현합니다.
#include "ProjectileMovementComponent.h"

#include "Component/SceneComponent.h"
#include "GameFramework/AActor.h"
#include "Math/MathUtils.h"
#include "Object/ObjectFactory.h"
#include "Render/Scene/Debug/DebugRenderAPI.h"
#include "Render/Scene/Scene.h"
#include "Serialization/Archive.h"

IMPLEMENT_CLASS(UProjectileMovementComponent, UMovementComponent)

void UProjectileMovementComponent::BeginPlay()
{
    UMovementComponent::BeginPlay();
}

void UProjectileMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
    UMovementComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);

    USceneComponent* UpdatedSceneComponent = GetUpdatedComponent();
    if (!UpdatedSceneComponent)
    {
        return;
    }

    FVector EffectiveVelocity = ComputeEffectiveVelocity();
    const float CurrentSpeed = EffectiveVelocity.Length();
    if (MaxSpeed > 0.0f && CurrentSpeed > MaxSpeed)
    {
        EffectiveVelocity = EffectiveVelocity.Normalized() * MaxSpeed;
    }

    const FVector MoveDelta = EffectiveVelocity * DeltaTime;
    if (MoveDelta.Length() <= FMath::Epsilon)
    {
        return;
    }

    UpdatedSceneComponent->SetWorldLocation(UpdatedSceneComponent->GetWorldLocation() + MoveDelta);
}

void UProjectileMovementComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UMovementComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "Velocity", EPropertyType::Vec3, &Velocity, 0.0f, 0.0f, 1.0f });
    OutProps.push_back({ "Initial Speed", EPropertyType::Float, &InitialSpeed, 0.0f, 0.0f, 10.0f });
    OutProps.push_back({ "Max Speed", EPropertyType::Float, &MaxSpeed, 0.0f, 0.0f, 10.0f });
}

void UProjectileMovementComponent::Serialize(FArchive& Ar)
{
    UMovementComponent::Serialize(Ar);
    Ar << Velocity;
    Ar << InitialSpeed;
    Ar << MaxSpeed;
}

void UProjectileMovementComponent::ContributeSelectedVisuals(FScene& Scene) const
{
    const FVector PreviewVelocity = GetPreviewVelocity();
    if (PreviewVelocity.Length() <= FMath::Epsilon)
    {
        return;
    }

    USceneComponent* SourceComponent = GetUpdatedComponent();
    if (!SourceComponent)
    {
        AActor* OwnerActor = GetOwner();
        SourceComponent = OwnerActor ? OwnerActor->GetRootComponent() : nullptr;
    }
    if (!SourceComponent)
    {
        return;
    }

    constexpr float ProjectileArrowScale = 0.25f;
    const FVector ScaledVelocity = PreviewVelocity * ProjectileArrowScale;
    RenderDebugArrow(Scene, SourceComponent->GetWorldLocation(), ScaledVelocity.Normalized(),
                     ScaledVelocity.Length(), FColor(135, 206, 235));
}

void UProjectileMovementComponent::StopSimulating()
{
    Velocity = FVector(0.0f, 0.0f, 0.0f);
}

FVector UProjectileMovementComponent::GetPreviewVelocity() const
{
    return ComputeEffectiveVelocity();
}

EProjectileHitBehavior UProjectileMovementComponent::GetHitBehavior() const
{
    return EProjectileHitBehavior::Stop;
}

FVector UProjectileMovementComponent::ComputeEffectiveVelocity() const
{
    FVector EffectiveVelocity = Velocity;

    if (EffectiveVelocity.Length() <= FMath::Epsilon)
    {
        USceneComponent* SourceComponent = GetUpdatedComponent();
        if (!SourceComponent)
        {
            AActor* OwnerActor = GetOwner();
            SourceComponent = OwnerActor ? OwnerActor->GetRootComponent() : nullptr;
        }

        if (SourceComponent)
        {
            EffectiveVelocity = SourceComponent->GetForwardVector().Normalized();
        }
    }

    if (InitialSpeed > 0.0f && EffectiveVelocity.Length() > FMath::Epsilon)
    {
        EffectiveVelocity *= InitialSpeed;
    }

    return EffectiveVelocity;
}

bool UProjectileMovementComponent::HandleBlockingHit(USceneComponent* UpdatedSceneComponent, const FVector& CurrentLocation, const FVector& MoveDelta, const FHitResult& HitResult)
{
    (void)UpdatedSceneComponent;
    (void)CurrentLocation;
    (void)MoveDelta;
    (void)HitResult;
    return true;
}
