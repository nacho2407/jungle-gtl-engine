#include "LuaScriptComponent.h"

#include "Core/Logging/LogMacros.h"
#include "LuaScript/LuaScriptManager.h"
#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"

IMPLEMENT_CLASS(ULuaScriptComponent, UActorComponent)

void ULuaScriptComponent::BeginPlay()
{
    UActorComponent::BeginPlay();
}

void ULuaScriptComponent::EndPlay()
{
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
    UE_LOG(this, Info, "들어오는중");
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