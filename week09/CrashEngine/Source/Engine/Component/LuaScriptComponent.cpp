#include "LuaScriptComponent.h"
#include "Object/ObjectFactory.h"

IMPLEMENT_CLASS(ULuaScriptComponent, UActorComponent)

void ULuaScriptComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UActorComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "Static Mesh", EPropertyType::LuaScriptRef, &LuaScriptFilePath });
}