#pragma once

#include "GameFramework/Pawn/Pawn.h"

class UCapsuleComponent;
class USkeletalMeshComponent;
class UCharacterMovementComponent;

// UE 의 ACharacter 패턴 — Capsule(Root) → SkeletalMesh + CharacterMovement 의 표준 구성.
//
//   Root: UCapsuleComponent           (충돌/이동 본체. CharacterMovement 의 UpdatedComponent)
//     └ USkeletalMeshComponent (Mesh) (시각화. Animation 시스템과 통합)
//   UCharacterMovementComponent       (non-scene — Capsule 을 UpdatedComponent 로 가리킴)
//
// minimal: Z=0 평지 가정, gravity/jump/floor sweep 없음. 후속 phase 에서 확장.
// LuaScriptComponent 는 이 베이스에 부착하지 않는다 — Lua 로직이 필요하면 ALuaCharacter 사용.

#include "Source/Engine/GameFramework/Pawn/Character.generated.h"

UCLASS()
class ACharacter : public APawn
{
public:
	GENERATED_BODY()
	ACharacter() = default;
	~ACharacter() override = default;

	// SkeletalMesh fbx (또는 .sketbin path) 받아 default 컴포넌트 구성.
	// 자식 (예: ALuaCharacter) 이 Super 호출 후 자기 컴포넌트 추가 가능.
	virtual void InitDefaultComponents(const FString& SkeletalMeshFileName);

	void PostDuplicate() override;

	// CharacterMovement->AddInputVector 의 액터 레벨 wrapper. UE 의 APawn::AddMovementInput 대응.
	void AddMovementInput(const FVector& WorldDirection, float ScaleValue = 1.0f);

	// CharacterMovement->Jump 의 액터 레벨 wrapper. Walking 중에만 effective.
	void Jump();

	UCapsuleComponent*           GetCapsuleComponent()  const { return CapsuleComponent; }
	USkeletalMeshComponent*      GetMesh()              const { return Mesh; }
	UCharacterMovementComponent* GetCharacterMovement() const { return CharacterMovement; }

	// 자동 WASD 매핑/binding — SetupInputComponent 가 InputComponent 에 등록.
	// 게임에선 보통 false 로 끄고 자식이 자기 매핑/binding 추가. 데모 편의용 기본 true.
	bool bAutoInputWASD = true;

	// 자동 mouse look — Tick 안에서 mouse delta X/Y * 기준 감도 * MouseSensitivity 로 APawn::ControlRotation 누적.
	// capsule 자체 회전은 안 함 — SpringArm 의 bUsePawnControlRotation 가 ControlRotation 사용해
	// 카메라만 회전. WASD 도 ControlRotation.Yaw 기준 forward/right 로 이동.
	bool  bAutoInputMouseLook = true;
	float MouseSensitivity    = 1.0f;   // 기준 감도 배율 — yaw/pitch 공통
	float MinCameraPitch      = -85.0f; // 위 한도 (마이너스 = 위)
	float MaxCameraPitch      =  85.0f; // 아래 한도

protected:
	// InputComponent 가 부착된 후 호출 — WASD axis mapping + AddMovementInput binding 등록.
	void SetupInputComponent() override;

	// 자동 mouse look + 향후 다른 per-frame 입력 처리.
	void Tick(float DeltaTime) override;

	UCapsuleComponent*           CapsuleComponent  = nullptr;
	USkeletalMeshComponent*      Mesh              = nullptr;
	UCharacterMovementComponent* CharacterMovement = nullptr;
};
