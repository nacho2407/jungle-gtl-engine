#include "GameFramework/Pawn/Character.h"

#include "Component/Shape/CapsuleComponent.h"
#include "Component/Input/InputComponent.h"
#include "Component/Movement/CharacterMovementComponent.h"
#include "Component/Physics/PhysicalAnimationComponent.h"
#include "Component/Primitive/SkeletalMeshComponent.h"
#include "Input/InputSystem.h"
#include "Math/Rotator.h"
#include "Mesh/MeshManager.h"
#include "Physics/BodyInstance.h"
#include "Runtime/Engine.h"

#include <algorithm>

namespace
{
	constexpr float InitialRagdollVelocityClampDuration = 0.30f;
	constexpr float InitialRagdollMaxLinearSpeed = 8.0f;
	constexpr float InitialRagdollMaxAngularSpeed = 12.0f;

	FVector ClampVectorLength(const FVector& Value, float MaxLength)
	{
		const float Length = Value.Length();
		if (Length <= MaxLength || Length <= 0.0001f)
		{
			return Value;
		}

		return Value * (MaxLength / Length);
	}

	void ClampRagdollBodyVelocities(USkeletalMeshComponent* Mesh)
	{
		if (!Mesh)
		{
			return;
		}

		for (FBodyInstance* Body : Mesh->GetRagdollBodies())
		{
			if (!Body || !Body->IsValidBodyInstance())
			{
				continue;
			}

			Body->SetLinearVelocity(ClampVectorLength(Body->GetLinearVelocity(), InitialRagdollMaxLinearSpeed));
			Body->SetAngularVelocity(ClampVectorLength(Body->GetAngularVelocity(), InitialRagdollMaxAngularSpeed));
		}
	}
}

void ACharacter::InitDefaultComponents(const FString& SkeletalMeshFileName)
{
	// 1) Capsule — Root. CharacterMovement 의 UpdatedComponent 가 이걸 가리킴.
	CapsuleComponent = AddComponent<UCapsuleComponent>();
	SetRootComponent(CapsuleComponent);

	// 2) SkeletalMesh — Capsule 의 자식.
	Mesh = AddComponent<USkeletalMeshComponent>();
	Mesh->AttachToComponent(CapsuleComponent);

	ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
	if (!SkeletalMeshFileName.empty())
	{
		USkeletalMesh* Asset = FMeshManager::LoadSkeletalMesh(SkeletalMeshFileName, Device);
		Mesh->SetSkeletalMesh(Asset);
	}

	// 3) CharacterMovement — non-scene. UpdatedComponent = Capsule.
	CharacterMovement = AddComponent<UCharacterMovementComponent>();
	CharacterMovement->SetUpdatedComponent(CapsuleComponent);
}

void ACharacter::RefreshCharacterComponents()
{
	CapsuleComponent  = Cast<UCapsuleComponent>(GetRootComponent());
	Mesh              = GetComponentByClass<USkeletalMeshComponent>();
	CharacterMovement = GetComponentByClass<UCharacterMovementComponent>();

	if (CharacterMovement && CapsuleComponent)
	{
		CharacterMovement->SetUpdatedComponent(CapsuleComponent);
	}
}

void ACharacter::BeginPlay()
{
	RefreshCharacterComponents();
	Super::BeginPlay();
}

void ACharacter::PostDuplicate()
{
	Super::PostDuplicate();
	RefreshCharacterComponents();
}

void ACharacter::AddMovementInput(const FVector& WorldDirection, float ScaleValue)
{
	if (CharacterMovement)
	{
		CharacterMovement->AddInputVector(WorldDirection, ScaleValue);
	}
}

void ACharacter::Jump()
{
	if (CharacterMovement)
	{
		CharacterMovement->Jump();
	}
}

void ACharacter::EnterFullRagdoll()
{
	RefreshCharacterComponents();

	if (InputComponent)
	{
		InputComponent->ClearBindings();
	}

	if (CharacterMovement)
	{
		CharacterMovement->Deactivate();
		CharacterMovement->SetComponentTickEnabled(false);
	}

	if (CapsuleComponent)
	{
		CapsuleComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (UPhysicalAnimationComponent* PhysicalAnimation = GetComponentByClass<UPhysicalAnimationComponent>())
	{
		PhysicalAnimation->StopDrivingKeepRagdoll();
	}

	if (Mesh)
	{
		Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Mesh->SetRagdollEnabled(true);
		Mesh->SetAllBodiesSimulatePhysics(true);
		Mesh->SetAllBodiesPhysicsBlendWeight(1.0f);
		ClampRagdollBodyVelocities(Mesh);
		Mesh->WakeAllRagdollBodies();
	}

	bAutoInputMouseLook = false;
	InitialRagdollVelocityClampTimeRemaining = InitialRagdollVelocityClampDuration;
}

void ACharacter::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (!bAutoInputWASD || !InputComponent) return;

	// Capsule (RootComponent) 기준 — yaw 회전이 곧 캐릭터 facing. mouse look 이 yaw 만
	// 변경 → forward/right vector 가 자동 회전 → WASD 가 "카메라 보는 방향" 으로 이동.
	InputComponent->AddAxisMapping("MoveForward", 'W',  1.0f);
	InputComponent->AddAxisMapping("MoveForward", 'S', -1.0f);
	InputComponent->AddAxisMapping("MoveRight",   'D',  1.0f);
	InputComponent->AddAxisMapping("MoveRight",   'A', -1.0f);

	// WASD 의 forward/right 는 ControlRotation.Yaw 기준 — capsule rotation 과 무관.
	// "카메라가 보는 방향" (yaw 만, pitch 무시) 으로 이동.
	InputComponent->BindAxis("MoveForward", [this](float Value)
	{
		if (Value == 0.0f) return;
		const FRotator YawOnly(0.0f, GetControlRotation().Yaw, 0.0f);
		AddMovementInput(YawOnly.GetForwardVector(), Value);
	});
	InputComponent->BindAxis("MoveRight", [this](float Value)
	{
		if (Value == 0.0f) return;
		const FRotator YawOnly(0.0f, GetControlRotation().Yaw, 0.0f);
		AddMovementInput(YawOnly.GetRightVector(), Value);
	});

	// Space = Jump (VK_SPACE = 0x20). Walking 중에만 effective (CharacterMovement::Jump 가 guard).
	InputComponent->AddActionMapping("Jump", 0x20);
	InputComponent->BindAction("Jump", EInputEvent::Pressed, [this]()
	{
		Jump();
	});
}

void ACharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (InitialRagdollVelocityClampTimeRemaining > 0.0f)
	{
		ClampRagdollBodyVelocities(Mesh);
		InitialRagdollVelocityClampTimeRemaining =
			(std::max)(0.0f, InitialRagdollVelocityClampTimeRemaining - DeltaTime);
	}

	if (bAutoInputMouseLook)
	{
		const InputSystem& In = InputSystem::Get();
		const int DX = In.MouseDeltaX();
		const int DY = In.MouseDeltaY();
		if (DX != 0 || DY != 0)
		{
			// APawn::ControlRotation 누적. SpringArm 이 bUsePawnControlRotation 통해 이걸 사용.
			// capsule 회전은 옵션 (bUseControllerRotationYaw 등) — 아래 ApplyControllerRotationToRoot 가 처리.
			FRotator Rot = GetControlRotation();
			Rot.Yaw   += static_cast<float>(DX) * MouseSensitivity;
			Rot.Pitch += static_cast<float>(DY) * MouseSensitivity;
			Rot.Pitch  = std::clamp(Rot.Pitch, MinCameraPitch, MaxCameraPitch);
			SetControlRotation(Rot);
		}
	}

	// 같은 frame 안 ControlRotation 변경을 capsule (RootComponent) 에 즉시 반영 — 1 frame 지연 없음.
	// 옵션 충돌 가드:
	//   1) bOrientRotationToMovement = true → yaw 는 Movement::PhysOrientToMovement 가 처리.
	//   2) 직전 frame 에 root motion 이 yaw 를 적용했다 → 이번 frame 도 root motion 이 yaw 를
	//      이어받을 가능성이 큼. Character 가 control yaw 로 덮으면 root motion 회전이 즉시
	//      뒤집혀 토글링 됨 (turn-in-place / strafe anim 의 시각 손상). Movement 측에 양보.
	// 두 경우 모두 pitch/roll 만 apply, yaw 는 movement 에 양보.
	if (CapsuleComponent)
	{
		const bool bMovementHandlesYaw = CharacterMovement &&
			(CharacterMovement->bOrientRotationToMovement ||
			 CharacterMovement->HasYawDrivenByRootMotion());

		FRotator R = CapsuleComponent->GetRelativeRotation();
		bool bChanged = false;
		if (bUseControllerRotationYaw && !bMovementHandlesYaw)
		{
			R.Yaw   = ControlRotation.Yaw;
			bChanged = true;
		}
		if (bUseControllerRotationPitch)
		{
			R.Pitch = ControlRotation.Pitch;
			bChanged = true;
		}
		if (bUseControllerRotationRoll)
		{
			R.Roll  = ControlRotation.Roll;
			bChanged = true;
		}
		if (bChanged) CapsuleComponent->SetRelativeRotation(R);
	}
}

void ACharacter::OnOwnedComponentRemoved(UActorComponent* Component)
{
	Super::OnOwnedComponentRemoved(Component);
	if (Component == CapsuleComponent)
	{
		CapsuleComponent = nullptr;
	}
	if (Component == Mesh)
	{
		Mesh = nullptr;
	}
	if (Component == CharacterMovement)
	{
		CharacterMovement = nullptr;
	}
}
