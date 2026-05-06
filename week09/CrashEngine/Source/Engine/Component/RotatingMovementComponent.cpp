// 컴포넌트 영역의 세부 동작을 구현합니다.
#include "RotatingMovementComponent.h"

#include "Object/ObjectFactory.h"
#include "SceneComponent.h"
#include "CameraManage/APlayerCameraManager.h"
#include "Runtime/Engine.h"
#include "Serialization/Archive.h"

IMPLEMENT_COMPONENT_CLASS(URotatingMovementComponent, UMovementComponent, EEditorComponentCategory::Movement)

void URotatingMovementComponent::BeginPlay()
{
    UMovementComponent::BeginPlay();
    if (APlayerCameraManager* CameraManager = GEngine ? GEngine->GetPlayerCameraManager() : nullptr)
    {
        CameraManager->PlayCameraShake(2.f, 2.f, 2.f, 2.f);
        CameraManager->PlayCameraLetterBox(0.0f, 0.2f, 2.f);
        CameraManager->PlayCameraFade(0.0f, 0.5f, 5.f);
    }
    FCameraTransitionParams param = {};
    param.Duration = 1;
    param.BlendType = ECameraTransitionBlendType::Cubic;
    GEngine->GetPlayerCameraManager()->SetViewTargetWithBlend(GetOwner(), param);
}

void URotatingMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
    UMovementComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);

    USceneComponent* UpdatedSceneComponent = GetUpdatedComponent();
    if (!UpdatedSceneComponent)
    {
        return;
    }

    // AddLocalRotation은 내부에서 quat 합성으로 누적하므로 짐벌락에 안전.
    UpdatedSceneComponent->AddLocalRotation(RotationRate * DeltaTime);
}

void URotatingMovementComponent::Serialize(FArchive& Ar)
{
    UMovementComponent::Serialize(Ar);
    Ar << RotationRate.Pitch;
    Ar << RotationRate.Yaw;
    Ar << RotationRate.Roll;
}

void URotatingMovementComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UMovementComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "Rotation Rate", EPropertyType::Rotator, &RotationRate, 0.0f, 0.0f, 0.1f });
}
