#pragma once

#include "GameFramework/AActor.h"
#include "Platform/Paths.h"

class UBillboardComponent;
class ULuaScriptComponent;
class USceneComponent;

// Logic-only actor with a transform, editor billboard, and Lua script component.
class ALuaScriptActor : public AActor
{
public:
    DECLARE_CLASS(ALuaScriptActor, AActor)
    ALuaScriptActor();

    void InitDefaultComponents();

    ULuaScriptComponent* GetLuaScriptComponent() const { return LuaScriptComponent; }

private:
    USceneComponent* SceneRootComponent = nullptr;
    UBillboardComponent* BillboardComponent = nullptr;
    ULuaScriptComponent* LuaScriptComponent = nullptr;
    FString LuaScriptActorIconPath = FPaths::EditorRelativePath("Icons/Materials/PawnIcon.json");
};
