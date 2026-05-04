// Implements the Lua actor default component setup.
#include "GameFramework/LuaActor.h"

#include "Component/LuaScriptComponent.h"
#include "Component/SceneComponent.h"

IMPLEMENT_CLASS(ALuaActor, AActor)

void ALuaActor::InitDefaultComponents()
{
    SceneRootComponent = AddComponent<USceneComponent>();
    SetRootComponent(SceneRootComponent);

    LuaScriptComponent = AddComponent<ULuaScriptComponent>();
}
