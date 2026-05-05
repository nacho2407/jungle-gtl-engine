#include "PlayerCameraManager.h"
#include "Component/CameraComponent.h"
#include "GameFramework/CameraShakeBase.h"
#include "Object/ObjectFactory.h"
#include "Object/UClass.h"
#include <algorithm>

IMPLEMENT_CLASS(APlayerCameraManager, AActor)

void APlayerCameraManager::RegisterCamera(UCameraComponent* Camera)
{
	if (Camera)
	{
		if (RegisteredCameras.insert(Camera).second)
		{
			RegisteredCameraOrder.push_back(Camera);
		}
	}
}

void APlayerCameraManager::UnregisterCamera(UCameraComponent* Camera)
{
	if (Camera)
	{
		RegisteredCameras.erase(Camera);
		RegisteredCameraOrder.erase(
			std::remove(RegisteredCameraOrder.begin(), RegisteredCameraOrder.end(), Camera),
			RegisteredCameraOrder.end());
		if (ActiveCamera == Camera)
		{
			ActiveCamera = nullptr;
		}
		if (PossessedCamera == Camera)
		{
			PossessedCamera = nullptr;
		}
	}
}

void APlayerCameraManager::AutoPossessDefaultCamera()
{
	// 이미 누가 ActiveCamera를 설정했으면(예: APawn::PossessedBy에서 자기 카메라 지정)
	// 첫 등록 카메라로 덮어쓰지 않는다. World::BeginPlay 흐름상 GameMode->StartMatch가
	// 먼저 호출되어 Pawn 카메라가 설정될 수 있고, 그 후 본 함수가 호출되기 때문.
	if (ActiveCamera) return;

	if (!RegisteredCameraOrder.empty())
	{
		SetActiveCamera(RegisteredCameraOrder.front());
		Possess(RegisteredCameraOrder.front());
	}
}

// 현재는 Actor당 카메라 최대 2개만 가능
bool APlayerCameraManager::ToggleActiveCameraForActor(const FString& ActorName)
{
	TArray<UCameraComponent*> ActorCameras;
	for (UCameraComponent* Camera : RegisteredCameraOrder)
	{
		if (Camera && Camera->GetOwner()
			&& Camera->GetOwner()->GetFName().ToString() == ActorName)
		{
			ActorCameras.push_back(Camera);
		}
	}

	if (ActorCameras.empty())
	{
		return false;
	}

	if (ActorCameras.size() == 1)
	{
		SetActiveCamera(ActorCameras.front());
		Possess(ActorCameras.front());
		return true;
	}

	UCameraComponent* NextCamera = ActorCameras[0];
	if (ActiveCamera == ActorCameras[0])
	{
		NextCamera = ActorCameras[1];
	}

	SetActiveCamera(NextCamera);
	Possess(NextCamera);
	return true;
}

bool APlayerCameraManager::ToggleActiveCameraForActor(const AActor* Actor)
{
	if (!Actor)
	{
		return false;
	}

	TArray<UCameraComponent*> ActorCameras;
	for (UCameraComponent* Camera : RegisteredCameraOrder)
	{
		if (Camera && Camera->GetOwner() == Actor)
		{
			ActorCameras.push_back(Camera);
		}
	}

	if (ActorCameras.empty())
	{
		return false;
	}

	if (ActorCameras.size() == 1)
	{
		SetActiveCamera(ActorCameras.front());
		Possess(ActorCameras.front());
		return true;
	}

	UCameraComponent* NextCamera = ActorCameras[0];
	if (ActiveCamera == ActorCameras[0])
	{
		NextCamera = ActorCameras[1];
	}

	SetActiveCamera(NextCamera);
	Possess(NextCamera);
	return true;
}

// ─────────────────────────────────────────────────────────────────
// View Target
// ─────────────────────────────────────────────────────────────────
void APlayerCameraManager::SetViewTarget(AActor* NewViewTarget, FViewTargetTransitionParams TransitionParams)
{
	// TODO(B): NewViewTarget 의 UCameraComponent 를 찾아 ActiveCamera 로 전환.
	//          BlendTime > 0 이면 PendingViewTarget 으로 보관, UpdateCamera 에서 보간.
	//          BlendTime == 0 이면 즉시 전환.
	if (!NewViewTarget)
	{
		return;
	}

	if (!ViewTarget && ActiveCamera)
	{
		ViewTarget = ActiveCamera->GetOwner();
	}

	if (TransitionParams.BlendTime <= 0.0f || !ViewTarget)
	{
		ViewTarget = NewViewTarget;
		PendingViewTarget = nullptr;
		BlendTimeRemaining = 0.0f;

		
		if (UCameraComponent* Camera = NewViewTarget->GetComponentByClass<UCameraComponent>())
		{
			SetActiveCamera(Camera);
		}
		return;
	}

	PendingViewTarget = NewViewTarget;
	BlendParams = TransitionParams;
	BlendTimeRemaining = TransitionParams.BlendTime;
}

// ─────────────────────────────────────────────────────────────────
// Camera Shake
// ─────────────────────────────────────────────────────────────────
UCameraShakeBase* APlayerCameraManager::StartCameraShake(
	UClass* ShakeClass,
	float Scale,
	ECameraShakePlaySpace PlaySpace,
	FRotator UserPlaySpaceRot)
{
	if (!ShakeClass) return nullptr;

	// TODO(B): bSingleInstance 인 셰이크면 기존 인스턴스 검사 후 재시작/스킵.

	UObject* Obj = FObjectFactory::Get().Create(ShakeClass->GetName(), this);
	UCameraShakeBase* Shake = Cast<UCameraShakeBase>(Obj);
	if (!Shake)
	{
		// 실패 시 메모리 누수 방지
		if (Obj) UObjectManager::Get().DestroyObject(Obj);
		return nullptr;
	}

	Shake->StartShake(this, Scale, PlaySpace, UserPlaySpaceRot);
	ActiveShakes.push_back(Shake);
	return Shake;
}

void APlayerCameraManager::StopCameraShake(UCameraShakeBase* ShakeInstance, bool bImmediately)
{
	if (!ShakeInstance) return;
	ShakeInstance->StopShake(bImmediately);
	// 실제 제거는 UpdateCamera 에서 IsFinished() 체크 후.
}

void APlayerCameraManager::StopAllCameraShakes(bool bImmediately)
{
	for (UCameraShakeBase* Shake : ActiveShakes)
	{
		if (Shake) Shake->StopShake(bImmediately);
	}
}

void APlayerCameraManager::StopAllInstancesOfCameraShake(UClass* ShakeClass, bool bImmediately)
{
	if (!ShakeClass) return;
	for (UCameraShakeBase* Shake : ActiveShakes)
	{
		if (Shake && Shake->GetClass()->IsA(ShakeClass))
		{
			Shake->StopShake(bImmediately);
		}
	}
}

// ─────────────────────────────────────────────────────────────────
// Camera Fade
// ─────────────────────────────────────────────────────────────────
void APlayerCameraManager::StartCameraFade(
	float FromAlpha,
	float ToAlpha,
	float Duration,
	FLinearColor Color,
	bool bShouldFadeAudio,
	bool bHoldWhenFinished)
{
	bEnableFading = true;
	FadeAlphaFrom = FromAlpha;
	FadeAlphaTo = ToAlpha;
	FadeDuration = Duration;
	FadeTimeRemaining = Duration;
	FadeAmount = FromAlpha;
	FadeColor = Color;
	bFadeAudio = bShouldFadeAudio;
	bHoldFadeWhenFinished = bHoldWhenFinished;
}

void APlayerCameraManager::StopCameraFade()
{
	bEnableFading = false;
	FadeAmount = 0.0f;
	FadeTimeRemaining = 0.0f;
}

void APlayerCameraManager::SetManualCameraFade(float InFadeAmount, FLinearColor Color, bool bInFadeAudio)
{
	bEnableFading = true;
	FadeAmount = InFadeAmount;
	FadeColor = Color;
	bFadeAudio = bInFadeAudio;
	FadeTimeRemaining = 0.0f;	// 수동 모드는 자동 갱신 안 함
}

// ─────────────────────────────────────────────────────────────────
// Camera Vignette
// ─────────────────────────────────────────────────────────────────
void APlayerCameraManager::SetCameraVignette(float Intensity, float Radius, float Softness, FLinearColor Color)
{
	bEnableVignette = true;
	VignetteIntensity = Intensity;
	VignetteRadius = Radius;
	VignetteSoftness = Softness;
	VignetteColor = Color;
}

void APlayerCameraManager::ClearCameraVignette()
{
	bEnableVignette = false;
	VignetteIntensity = 0.0f;
}

// ─────────────────────────────────────────────────────────────────
// Camera Blend — ViewTarget A → Pending B 로 전환 중일 때 매 호출 시 보간된
// raw POV 를 산출. shake 는 미포함. UpdateCamera 가 이걸 호출해 base POV 로
// 받은 뒤 shake 를 누적해 CameraCachePOV 에 commit 한다.
// ─────────────────────────────────────────────────────────────────
bool APlayerCameraManager::GetCameraView(FMinimalViewInfo& OutPOV) const
{
	if (!ViewTarget && !ActiveCamera)
	{
		return false;
	}

	UCameraComponent* FromCamera = ViewTarget ? ViewTarget->GetComponentByClass<UCameraComponent>() : nullptr;
	if (!FromCamera)
	{
		FromCamera = ActiveCamera;
	}

	if (!FromCamera)
	{
		return false;
	}

	if (!PendingViewTarget || BlendTimeRemaining <= 0.0f || BlendParams.BlendTime <= 0.0f)
	{
		FromCamera->GetCameraView(0.0f, OutPOV);
		return true;
	}

	UCameraComponent* ToCamera = PendingViewTarget->GetComponentByClass<UCameraComponent>();
	if (!ToCamera)
	{
		FromCamera->GetCameraView(0.0f, OutPOV);
		return true;
	}

	FMinimalViewInfo FromPOV;
	FMinimalViewInfo ToPOV;
	FromCamera->GetCameraView(0.0f, FromPOV);
	ToCamera->GetCameraView(0.0f, ToPOV);

	float Alpha = 1.0f - (BlendTimeRemaining / BlendParams.BlendTime);
	Alpha = ApplyBlendFunction(Alpha, BlendParams);

	OutPOV = LerpPOV(FromPOV, ToPOV, Alpha);
	return true;
}

// ─────────────────────────────────────────────────────────────────
// Tick — ViewTarget blend timer → base+blend POV (GetCameraView) →
//        Shake 누적 → Fade timer → CameraCachePOV commit.
// ─────────────────────────────────────────────────────────────────
void APlayerCameraManager::UpdateCamera(float DeltaTime)
{
	// (1) ViewTarget blend timer 갱신 + 완료 시 ViewTarget 전환 + ActiveCamera 갱신.
	//     base POV 산출 *전* 에 와야 새 BlendTimeRemaining 으로 보간된 POV 가 cache 에 들어간다.
	if (PendingViewTarget && BlendTimeRemaining > 0.0f)
	{
		BlendTimeRemaining = std::max(0.0f, BlendTimeRemaining - DeltaTime);

		if (BlendTimeRemaining <= 0.0f)
		{
			ViewTarget = PendingViewTarget;
			PendingViewTarget = nullptr;

			if (UCameraComponent* Camera = ViewTarget->GetComponentByClass<UCameraComponent>())
			{
				SetActiveCamera(Camera);
			}
		}
	}

	// (2) base+blend POV — GetCameraView 가 ViewTarget/PendingViewTarget 보간된 raw POV 산출.
	//     실패(둘 다 없음) 시 캐시 무효 표시 후 fade/shake timer 만 진행.
	FMinimalViewInfo NewPOV;
	const bool bHasBasePOV = GetCameraView(NewPOV);

	// (3) Shake 누적 — 모든 active shake 의 결과를 합산해 NewPOV 에 가산.
	//     PlaySpace(CameraLocal/World/UserDefined) 변환은 셰이크 서브클래스 구현에 위임.
	//     현재 베이스는 OutResult 를 채우지 않으므로 add-zero (no-op) — 서브클래스 도입 시 자연 작동.
	FCameraShakeUpdateResult ShakeResult;
	for (UCameraShakeBase* Shake : ActiveShakes)
	{
		if (Shake && !Shake->IsFinished())
		{
			Shake->UpdateAndApplyCameraShake(DeltaTime, ShakeResult);
		}
	}
	if (bHasBasePOV)
	{
		NewPOV.Location       += ShakeResult.Location;
		NewPOV.Rotation.Pitch += ShakeResult.Rotation.Pitch;
		NewPOV.Rotation.Yaw   += ShakeResult.Rotation.Yaw;
		NewPOV.Rotation.Roll  += ShakeResult.Rotation.Roll;
		NewPOV.FOV            += ShakeResult.FOV;
	}

	// (4) 종료된 셰이크 정리
	ActiveShakes.erase(
		std::remove_if(ActiveShakes.begin(), ActiveShakes.end(),
			[](UCameraShakeBase* S) { return !S || S->IsFinished(); }),
		ActiveShakes.end());

	// (5) Fade 진행 — 시각적 합성은 PostProcess 측, 여기선 알파 시간 누적만.
	if (bEnableFading && FadeDuration > 0.0f && FadeTimeRemaining > 0.0f)
	{
		FadeTimeRemaining = std::max(0.0f, FadeTimeRemaining - DeltaTime);
		const float Alpha = 1.0f - (FadeTimeRemaining / FadeDuration);
		FadeAmount = FadeAlphaFrom + (FadeAlphaTo - FadeAlphaFrom) * Alpha;

		if (FadeTimeRemaining <= 0.0f && !bHoldFadeWhenFinished)
		{
			bEnableFading = false;
			FadeAmount = 0.0f;
		}
	}

	// (6) Cache commit — 외부는 GetCameraCachePOV 로 read (shake/blend 적용된 최종 POV).
	if (bHasBasePOV)
	{
		CameraCachePOV    = NewPOV;
		bCameraCacheValid = true;
	}
	else
	{
		bCameraCacheValid = false;
	}
}

bool APlayerCameraManager::GetCameraCachePOV(FMinimalViewInfo& OutPOV) const
{
	if (!bCameraCacheValid) return false;
	OutPOV = CameraCachePOV;
	return true;
}

float APlayerCameraManager::ApplyBlendFunction(float Alpha, FViewTargetTransitionParams Params) const
{
	switch (Params.BlendFunction)
	{
	case EViewTargetBlendFunction::VTBlend_Linear:
		return Alpha;
	case EViewTargetBlendFunction::VTBlend_EaseIn:
		return Alpha * Alpha;
	case EViewTargetBlendFunction::VTBlend_EaseOut:
		return 1.0f - (1.0f - Alpha) * (1.0f - Alpha);
	case EViewTargetBlendFunction::VTBlend_EaseInOut:
		return Alpha < 0.5f ? 2.0f * Alpha * Alpha : 1.0f - std::pow(-2.0f * Alpha + 2.0f, 2) / 2.0f;
	case EViewTargetBlendFunction::VTBlend_PreBlended:
		return Alpha; // TODO(B): PreBlended 는 별도 처리 필요할 수 있음.
	default:
		return Alpha;
	}
}

FMinimalViewInfo APlayerCameraManager::LerpPOV(const FMinimalViewInfo& From, const FMinimalViewInfo& To, float Alpha) const
{
	FMinimalViewInfo Result;
	Result.Location = From.Location + (To.Location - From.Location) * Alpha;
	Result.Rotation.Pitch = From.Rotation.Pitch + (To.Rotation.Pitch - From.Rotation.Pitch) * Alpha;
	Result.Rotation.Yaw = From.Rotation.Yaw + (To.Rotation.Yaw - From.Rotation.Yaw) * Alpha;
	Result.Rotation.Roll = From.Rotation.Roll + (To.Rotation.Roll - From.Rotation.Roll) * Alpha;
	Result.FOV = From.FOV + (To.FOV - From.FOV) * Alpha;
	Result.AspectRatio = From.AspectRatio + (To.AspectRatio - From.AspectRatio) * Alpha;
	Result.OrthoWidth = From.OrthoWidth + (To.OrthoWidth - From.OrthoWidth) * Alpha;
	Result.NearClip = From.NearClip + (To.NearClip - From.NearClip) * Alpha;
	Result.FarClip = From.FarClip + (To.FarClip - From.FarClip) * Alpha;
	Result.bIsOrtho = To.bIsOrtho; // 보통 블렌드 중간에 갑자기 바뀌는 경우는 없으므로 To 기준으로.
	return Result;
}
