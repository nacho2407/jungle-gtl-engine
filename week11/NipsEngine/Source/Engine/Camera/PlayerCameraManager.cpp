#include "PlayerCameraManager.h"
#include "Camera/CameraModifier.h"

void APlayerCameraManager::Tick(float DeltaTime)
{
    FMinimalViewInfo View;
    GetCameraView(DeltaTime, View);

    // 여기서 Renderer로 넘기거나 View 적용
}

void APlayerCameraManager::SetViewTarget(AActor* NewTarget)
{
    if (!NewTarget)
        return;

    // 현재 카메라 상태 저장
    FMinimalViewInfo CurrentView;
    GetCameraView(0.f, CurrentView);

    Transition.From = CurrentView;

    // 새 타겟 설정
    ViewTarget.Target = NewTarget;
    ViewTarget.CameraComp = NewTarget->FindComponent<UCameraComponent>();

    FMinimalViewInfo NewView{};
    if (ViewTarget.CameraComp)
    {
        ViewTarget.CameraComp->GetCameraView(0.f, NewView);
    }

    Transition.To = NewView;
    Transition.TotalTime = 0.3f;
    Transition.RemainingTime = 0.3f;
    Transition.bActive = true;
}

void APlayerCameraManager::GetCameraView(float DeltaTime, FMinimalViewInfo& OutView)
{
    UpdateViewTarget(DeltaTime);
    ComputeCamera(DeltaTime, OutView);

    UpdateTransition(DeltaTime, OutView);
    ApplyModifiers(DeltaTime, OutView);
    UpdateFade(DeltaTime);
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

void APlayerCameraManager::UpdateViewTarget(float DeltaTime)
{
    if (ViewTarget.CameraComp)
    {
        FMinimalViewInfo CamView{};
        ViewTarget.CameraComp->GetCameraView(DeltaTime, CamView);

        ViewTarget.Location = CamView.Location;
        ViewTarget.Rotation = CamView.Rotation;
        ViewTarget.FOV = CamView.FOV;
    }
    else
    {        
        ViewTarget.Location = ViewTarget.Target->GetActorLocation();
        ViewTarget.Rotation = FRotator::MakeFromEuler(ViewTarget.Target->GetActorRotation());		
    }
}

void APlayerCameraManager::ComputeCamera(float DeltaTime, FMinimalViewInfo& OutView)
{
    OutView.Location = ViewTarget.Location;
    OutView.Rotation = ViewTarget.Rotation;
    OutView.FOV = ViewTarget.FOV;
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

void APlayerCameraManager::UpdateTransition(float DeltaTime, FMinimalViewInfo& InOutView)
{
    if (!Transition.bActive)
        return;

    Transition.RemainingTime -= DeltaTime;

    float Alpha = 1.f - (Transition.RemainingTime / Transition.TotalTime);
    Alpha = std::clamp(Alpha, 0.f, 1.f);
    // 추가 (smoothstep)
    Alpha = Alpha * Alpha * (3.f - 2.f * Alpha);

    InOutView.Location = FVector::Lerp(Transition.From.Location, Transition.To.Location, Alpha);
    InOutView.Rotation = FRotator::Lerp(Transition.From.Rotation, Transition.To.Rotation, Alpha);
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

    // 실제 렌더러에서 Fade.Color + Fade.CurrentAlpha 사용
}