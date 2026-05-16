#pragma once

#include "GameFramework/Pawn.h"

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
class ACharacter : public APawn
{
public:
	DECLARE_CLASS(ACharacter, APawn)

	ACharacter() = default;
	~ACharacter() override = default;

	// SkeletalMesh fbx (또는 .sketbin path) 받아 default 컴포넌트 구성.
	// 자식 (예: ALuaCharacter) 이 Super 호출 후 자기 컴포넌트 추가 가능.
	virtual void InitDefaultComponents(const FString& SkeletalMeshFileName);

	void PostDuplicate() override;

	// CharacterMovement->AddInputVector 의 액터 레벨 wrapper. UE 의 APawn::AddMovementInput 대응.
	void AddMovementInput(const FVector& WorldDirection, float ScaleValue = 1.0f);

	UCapsuleComponent*           GetCapsuleComponent()  const { return CapsuleComponent; }
	USkeletalMeshComponent*      GetMesh()              const { return Mesh; }
	UCharacterMovementComponent* GetCharacterMovement() const { return CharacterMovement; }

	// 자동 WASD 입력 처리. true 면 Tick 안에서 InputSystem 직접 읽어 +X/-X/-Y/+Y 로 AddMovementInput.
	// 게임에선 보통 false 로 끄고 PlayerController/lua 가 명시 input 처리. 데모 편의용 기본 true.
	bool bAutoInputWASD = true;

protected:
	void Tick(float DeltaTime) override;

	UCapsuleComponent*           CapsuleComponent  = nullptr;
	USkeletalMeshComponent*      Mesh              = nullptr;
	UCharacterMovementComponent* CharacterMovement = nullptr;
};
