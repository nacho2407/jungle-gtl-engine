// Defines the Lua actor type used by the editor.
#pragma once

#include "GameFramework/AActor.h"

class ULuaScriptComponent;
class USceneComponent;

class ALuaActor : public AActor
{
public:
    DECLARE_CLASS(ALuaActor, AActor)

    ALuaActor() {}

    void InitDefaultComponents();

    ULuaScriptComponent* GetLuaScriptComponent() const { return LuaScriptComponent; }

private:
    USceneComponent* SceneRootComponent = nullptr;
    ULuaScriptComponent* LuaScriptComponent = nullptr;
};
