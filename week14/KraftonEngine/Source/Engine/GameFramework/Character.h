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

	UCapsuleComponent*           GetCapsuleComponent()  const { return CapsuleComponent; }
	USkeletalMeshComponent*      GetMesh()              const { return Mesh; }
	UCharacterMovementComponent* GetCharacterMovement() const { return CharacterMovement; }

protected:
	UCapsuleComponent*           CapsuleComponent  = nullptr;
	USkeletalMeshComponent*      Mesh              = nullptr;
	UCharacterMovementComponent* CharacterMovement = nullptr;
};
