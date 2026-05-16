#include "GameFramework/LuaCharacter.h"

#include "Component/LuaScriptComponent.h"

IMPLEMENT_CLASS(ALuaCharacter, ACharacter)

void ALuaCharacter::InitDefaultComponents(const FString& SkeletalMeshFileName, const FString& ScriptFile)
{
	Super::InitDefaultComponents(SkeletalMeshFileName);

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
}
