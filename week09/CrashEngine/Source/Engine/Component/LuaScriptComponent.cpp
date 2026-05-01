#include "LuaScriptComponent.h"

#include "LuaScript/LuaScriptManager.h"
#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"

IMPLEMENT_CLASS(ULuaScriptComponent, UActorComponent)

void ULuaScriptComponent::BeginPlay()
{
    UActorComponent::BeginPlay();
    FLuaScriptManager::Get().RegisterComponent(this);
}

void ULuaScriptComponent::EndPlay()
{
    FLuaScriptManager::Get().UnRegisterComponent(this);
    UActorComponent::EndPlay();
}

void ULuaScriptComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UActorComponent::GetEditableProperties(OutProps);

    FPropertyDescriptor Prop;
    Prop.Name = "Script File";                 
    Prop.Type = EPropertyType::LuaScriptRef;
    Prop.ValuePtr = &LuaScriptPath;            
    OutProps.push_back(Prop);
}

void ULuaScriptComponent::PostEditProperty(const char* PropertyName)
{
    UActorComponent::PostEditProperty(PropertyName);

    if (strcmp(PropertyName, "Script File") == 0)
    {
        if (LuaScriptPath.empty() || LuaScriptPath == "None")
        {
            clearScript();
        }
        else
        {
            ReloadScript();
        }
    }
}

void ULuaScriptComponent::ReloadScript()
{
    
}

void ULuaScriptComponent::Serialize(FArchive& Ar)
{
    UActorComponent::Serialize(Ar);

    Ar << LuaScriptPath;
}

void ULuaScriptComponent::PostDuplicate()
{
    UActorComponent::PostDuplicate();

    if (!LuaScriptPath.empty() && LuaScriptPath != "None")
    {
        ReloadScript();
    }
}