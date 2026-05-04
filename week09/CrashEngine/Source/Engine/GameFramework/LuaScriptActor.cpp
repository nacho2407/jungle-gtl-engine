#include "GameFramework/LuaScriptActor.h"

#include "Component/BillboardComponent.h"
#include "Component/LuaScriptComponent.h"
#include "Component/SceneComponent.h"
#include "Materials/MaterialManager.h"
#include "Object/ObjectFactory.h"

IMPLEMENT_CLASS(ALuaScriptActor, AActor)

ALuaScriptActor::ALuaScriptActor()
{
    bTickInEditor = false;
}

void ALuaScriptActor::InitDefaultComponents()
{
    if (!SceneRootComponent)
    {
        SceneRootComponent = AddComponent<USceneComponent>();
        SetRootComponent(SceneRootComponent);
    }

    if (!BillboardComponent)
    {
        BillboardComponent = AddComponent<UBillboardComponent>();
        auto LuaScriptActorIcon = FMaterialManager::Get().GetOrCreateMaterial(LuaScriptActorIconPath);
        BillboardComponent->SetVisibleInEditor(true);
        BillboardComponent->SetVisibleInGame(false);
        BillboardComponent->SetEditorHelper(true);
        BillboardComponent->SetMaterial(LuaScriptActorIcon);
        BillboardComponent->AttachToComponent(SceneRootComponent);
    }

    if (!LuaScriptComponent)
    {
        LuaScriptComponent = AddComponent<ULuaScriptComponent>();
    }
}
