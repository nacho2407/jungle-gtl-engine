#include "PlayerCameraManager.h"
#include "Camera/CameraModifier.h"
#include "GameFramework/PlayerController.h"

namespace
{
    float ApplyCameraBlendType(float Alpha, ECameraBlendType BlendType)
    {
        switch (BlendType)
        {
        case ECameraBlendType::Linear:
            return Alpha;
        case ECameraBlendType::EaseIn:
            return Alpha * Alpha;
        case ECameraBlendType::EaseOut:
            return 1.f - (1.f - Alpha) * (1.f - Alpha);
        case ECameraBlendType::EaseInOut:
            return (Alpha < 0.5f)
                ? 2.f * Alpha * Alpha
                : 1.f - 2.f * (1.f - Alpha) * (1.f - Alpha);
        case ECameraBlendType::SmoothStep:
        default:
            return Alpha * Alpha * (3.f - 2.f * Alpha);
        }
    }
}

APlayerCameraManager::~APlayerCameraManager()
{
    if (CacheCameraShakeMod)
    {
        RemoveModifier(CacheCameraShakeMod);
        UObjectManager::Get().DestroyObject(CacheCameraShakeMod);
        CacheCameraShakeMod = nullptr;
    }
}

void APlayerCameraManager::BeginPlay()
{
    AActor::BeginPlay();
}

void APlayerCameraManager::Tick(float DeltaTime)
{
    AActor::Tick(DeltaTime);
    UpdateCamera(DeltaTime);
}

void APlayerCameraManager::SetViewTarget(AActor* NewTarget)
{
    SetViewTargetWithBlend(NewTarget, DefaultBlendTime, DefaultBlendType);
}

void APlayerCameraManager::SetViewTargetWithBlend(AActor* NewTarget, float BlendTime)
{
    SetViewTargetWithBlend(NewTarget, BlendTime, DefaultBlendType);
}

void APlayerCameraManager::SetViewTargetWithBlend(AActor* NewTarget, float BlendTime, ECameraBlendType BlendType)
{
	// 언리얼 설계 상 ViewTarget 은 절대 nullptr 로 두지 않는다
    if (NewTarget == nullptr)
	{
        NewTarget = PCOwner;
	}
    if (NewTarget == nullptr)
    {
        return;
    }

    // 현재 카메라 상태 저장
    UpdateCamera(0);
    Transition.From = CachedView;

    // 새 타겟 설정
    ViewTarget.Target = NewTarget;
    ViewTarget.CameraComp = NewTarget->FindComponent<UCameraComponent>();

    FMinimalViewInfo NewView = CachedView;
    if (ViewTarget.CameraComp)
    {
        ViewTarget.CameraComp->GetCameraView(0.f, NewView);
    }
    else
    {
        NewView.Location = ViewTarget.Target->GetActorLocation();
        NewView.Rotation = FQuat::MakeFromEuler(ViewTarget.Target->GetActorRotation()).GetNormalized();
    }
    ViewTarget.POV = NewView;

    Transition.To = NewView;
    Transition.TotalTime = std::max(0.f, BlendTime);
    Transition.RemainingTime = Transition.TotalTime;
    Transition.BlendType = BlendType;
    Transition.bActive = Transition.TotalTime > 0.f;
	
	UE_LOG("[PlayerCameraManager] SetViewTarget. Target=%s Camera=%s",
           ViewTarget.Target ? ViewTarget.Target->GetFName().ToString().c_str() : "None",
           ViewTarget.CameraComp ? ViewTarget.CameraComp->GetName().c_str() : "None");
}

void APlayerCameraManager::SetDefaultViewTargetBlend(float BlendTime, ECameraBlendType BlendType)
{
    DefaultBlendTime = std::max(0.f, BlendTime);
    DefaultBlendType = BlendType;
}

const FMinimalViewInfo& APlayerCameraManager::GetCameraView()
{
	// Tick 마다 UpdateCamera 하는 것만으론 불안정한 갱신이 발생했음
	// Actor 간 Tick 순서 등이 의심되나, 우선은 임시 해결을 위해 값을 가져가기 전 갱신을 택함
    UpdateCamera(0);
    return CachedView;
}

void APlayerCameraManager::UpdateCamera(float DeltaTime)
{
    FMinimalViewInfo OutResult;
    UpdateViewTarget(DeltaTime);
    ComputeCamera(DeltaTime, OutResult);
    UpdateTransition(DeltaTime, OutResult);
    ApplyModifiers(DeltaTime, OutResult);
    UpdateFade(DeltaTime);
	CachedView = OutResult;
}

void APlayerCameraManager::StartFade(float FromAlpha, float ToAlpha, float Duration, const FColor& Color)
{
    Fade.FromAlpha = FromAlpha;
    Fade.ToAlpha = ToAlpha;
    Fade.Duration = Duration;
    Fade.Elapsed = 0.f;
    Fade.Color = Color;
    Fade.CurrentAlpha = FromAlpha;
    Fade.bActive = true;
}

void APlayerCameraManager::StopFade()
{
    Fade.bActive = false;
}

void APlayerCameraManager::AddModifier(UCameraModifier* Modifier)
{
    if (!Modifier)
        return;

    Modifier->SetOwner(this);
    ModifierList.push_back(Modifier);

    std::sort(ModifierList.begin(), ModifierList.end(),
              [](UCameraModifier* A, UCameraModifier* B)
              {
                  return A->GetPriority() < B->GetPriority();
              });
}

void APlayerCameraManager::RemoveModifier(UCameraModifier* Modifier)
{
    ModifierList.erase(
        std::remove(ModifierList.begin(), ModifierList.end(), Modifier),
        ModifierList.end());
}

void APlayerCameraManager::StopAllCameraShakes(bool bImmediately)
{
    if (CacheCameraShakeMod)
    {
        CacheCameraShakeMod->StopAllCameraShakes(bImmediately);
    }
}

void APlayerCameraManager::InitializeFor(APlayerController* PC)
{
    PCOwner = PC;
}

void APlayerCameraManager::UpdateViewTarget(float DeltaTime)
{
    if (ViewTarget.CameraComp)
    {
        ViewTarget.CameraComp->GetCameraView(DeltaTime, ViewTarget.POV);
    }
    else if (ViewTarget.Target)
    {
        ViewTarget.POV.Location = ViewTarget.Target->GetActorLocation();
        ViewTarget.POV.Rotation = FQuat::MakeFromEuler(ViewTarget.Target->GetActorRotation()).GetNormalized();
		// FOV 등 나머진 Default 값
    }
}

void APlayerCameraManager::ComputeCamera(float DeltaTime, FMinimalViewInfo& OutView)
{
    OutView = ViewTarget.POV;
}

void APlayerCameraManager::ApplyModifiers(float DeltaTime, FMinimalViewInfo& InOutView)
{
	// erase 고려 for문
    for (int i = 0; i < ModifierList.size();)
    {
        UCameraModifier* Modifier = ModifierList[i];
        if (!Modifier)
        {
            ++i;
            continue;
        }

        Modifier->ModifyCamera(DeltaTime, InOutView);

        if (Modifier->IsDisabled())
        {
            ModifierList.erase(ModifierList.begin() + i);
        }
        else
        {
            ++i;
        }
    }
}

UCameraModifier_CameraShake* APlayerCameraManager::GetOrCreateCameraShakeModifier()
{
    if (CacheCameraShakeMod)
    {
        return CacheCameraShakeMod;
    }

    CacheCameraShakeMod =
        UObjectManager::Get().CreateObject<UCameraModifier_CameraShake>();

    if (!CacheCameraShakeMod)
        return nullptr;

    AddModifier(CacheCameraShakeMod);
    return CacheCameraShakeMod;
}

void APlayerCameraManager::UpdateTransition(float DeltaTime, FMinimalViewInfo& InOutView)
{
    if (!Transition.bActive)
        return;

    Transition.RemainingTime -= DeltaTime;

    float Alpha = 1.f - (Transition.RemainingTime / Transition.TotalTime);
    Alpha = std::clamp(Alpha, 0.f, 1.f);
    // 추가 (smoothstep)
    Alpha = ApplyCameraBlendType(Alpha, Transition.BlendType);

    InOutView.Location = FVector::Lerp(Transition.From.Location, Transition.To.Location, Alpha);
    InOutView.Rotation = FQuat::Slerp(Transition.From.Rotation, Transition.To.Rotation, Alpha);
    InOutView.FOV = std::lerp(Transition.From.FOV, Transition.To.FOV, Alpha);

    if (Transition.RemainingTime <= 0.f)
    {
        Transition.bActive = false;
    }
}

void APlayerCameraManager::UpdateFade(float DeltaTime)
{
    if (!Fade.bActive)
        return;

    Fade.Elapsed += DeltaTime;

    float T = (Fade.Duration > 0.f) ? (Fade.Elapsed / Fade.Duration) : 1.f;
    T = std::clamp(T, 0.f, 1.f);

    Fade.CurrentAlpha = std::lerp(Fade.FromAlpha, Fade.ToAlpha, T);

    if (Fade.Elapsed >= Fade.Duration)
    {
        Fade.bActive = false;
    }
}
