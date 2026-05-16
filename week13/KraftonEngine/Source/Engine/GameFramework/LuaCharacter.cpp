#include "GameFramework/LuaCharacter.h"

#include "Component/CameraComponent.h"
#include "Component/CapsuleComponent.h"
#include "Component/LuaScriptComponent.h"
#include "Component/SpringArmComponent.h"

IMPLEMENT_CLASS(ALuaCharacter, ACharacter)

void ALuaCharacter::InitDefaultComponents(const FString& SkeletalMeshFileName, const FString& ScriptFile)
{
	Super::InitDefaultComponents(SkeletalMeshFileName);

	// 3인칭 카메라 체인 — Capsule → SpringArm → Camera. lag 적용해 부드럽게 따라옴.
	SpringArm = AddComponent<USpringArmComponent>();
	SpringArm->AttachToComponent(CapsuleComponent);
	SpringArm->TargetArmLength       = 10.0f;
	SpringArm->SocketOffset          = FVector(0.0f, 0.0f, 3.0f);
	SpringArm->bEnableCameraLag      = true;
	SpringArm->bEnableCameraRotationLag = true;

	Camera = AddComponent<UCameraComponent>();
	Camera->AttachToComponent(SpringArm);

	LuaScriptComponent = AddComponent<ULuaScriptComponent>();
	if (!ScriptFile.empty())
	{
		LuaScriptComponent->SetScriptFile(ScriptFile);
	}
}

void ALuaCharacter::PostDuplicate()
{
	Super::PostDuplicate();
	LuaScriptComponent = GetComponentByClass<ULuaScriptComponent>();
	SpringArm          = GetComponentByClass<USpringArmComponent>();
	Camera             = GetComponentByClass<UCameraComponent>();
}
