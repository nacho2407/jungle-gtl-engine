// [UHT generated source - do not edit]
#include "Engine/GameFramework/AActor.h"
#include "ReflectionSystem/ReflectionDatabase.h"
#include "ReflectionSystem/ReflectionUtils.h"
#include "ThirdParty/sol/sol.hpp"

void Register_AActor()
{
    FClassInfo& info = AActor::StaticClassInfo;
    info.Properties.clear();
    info.ReflectedProperties.clear();
    info.GcPointerOffsets.clear();
    info.Functions.clear();
    info.ClassName = "AActor";
    info.ParentClassName = "UObject";
    {
        FObjectProperty* prop = new FObjectProperty();
        prop->Name = FName("RootComponent");
        prop->CPPType = "USceneComponent*";
        prop->Offset = offsetof(AActor, RootComponent);
        prop->ElementSize = sizeof(USceneComponent*);
        prop->Flags = PF_EditAnywhere;
        prop->Category = "Actor";
        prop->DisplayName = "Root Component";
        info.ReflectedProperties.push_back(prop);
    }

    {
        FObjectProperty* prop = new FObjectProperty();
        prop->Name = FName("OwningWorld");
        prop->CPPType = "UWorld*";
        prop->Offset = offsetof(AActor, OwningWorld);
        prop->ElementSize = sizeof(UWorld*);
        prop->Flags = PF_None;
        prop->Category = "Default";
        prop->DisplayName = "OwningWorld";
        info.ReflectedProperties.push_back(prop);
    }

    {
        FStructProperty* prop = new FStructProperty();
        prop->Name = FName("PendingActorLocation");
        prop->CPPType = "FVector";
        prop->Offset = offsetof(AActor, PendingActorLocation);
        prop->ElementSize = sizeof(FVector);
        prop->Flags = PF_EditAnywhere;
        prop->Category = "Transform";
        prop->DisplayName = "Location";
        info.ReflectedProperties.push_back(prop);
    }

    {
        FStructProperty* prop = new FStructProperty();
        prop->Name = FName("PendingActorRotation");
        prop->CPPType = "FVector";
        prop->Offset = offsetof(AActor, PendingActorRotation);
        prop->ElementSize = sizeof(FVector);
        prop->Flags = PF_EditAnywhere;
        prop->Category = "Transform";
        prop->DisplayName = "Rotation";
        info.ReflectedProperties.push_back(prop);
    }

    {
        FStructProperty* prop = new FStructProperty();
        prop->Name = FName("PendingActorScale");
        prop->CPPType = "FVector";
        prop->Offset = offsetof(AActor, PendingActorScale);
        prop->ElementSize = sizeof(FVector);
        prop->Flags = PF_EditAnywhere;
        prop->Category = "Transform";
        prop->DisplayName = "Scale";
        info.ReflectedProperties.push_back(prop);
    }

    {
        FBoolProperty* prop = new FBoolProperty();
        prop->Name = FName("bVisible");
        prop->CPPType = "bool";
        prop->Offset = offsetof(AActor, bVisible);
        prop->ElementSize = sizeof(bool);
        prop->Flags = PF_EditAnywhere;
        prop->Category = "Actor";
        prop->DisplayName = "Visible";
        info.ReflectedProperties.push_back(prop);
    }

    {
        FBoolProperty* prop = new FBoolProperty();
        prop->Name = FName("bIsActive");
        prop->CPPType = "bool";
        prop->Offset = offsetof(AActor, bIsActive);
        prop->ElementSize = sizeof(bool);
        prop->Flags = PF_EditAnywhere;
        prop->Category = "Actor";
        prop->DisplayName = "Active";
        info.ReflectedProperties.push_back(prop);
    }

    {
        FBoolProperty* prop = new FBoolProperty();
        prop->Name = FName("bTickInEditor");
        prop->CPPType = "bool";
        prop->Offset = offsetof(AActor, bTickInEditor);
        prop->ElementSize = sizeof(bool);
        prop->Flags = PF_EditAnywhere;
        prop->Category = "Actor";
        prop->DisplayName = "Tick In Editor";
        info.ReflectedProperties.push_back(prop);
    }

    {
        FArrayProperty* prop = new FArrayProperty();
        prop->ArrayOps = TScriptArrayOps<UActorComponent*>::Make();
        FObjectProperty* prop_Inner = new FObjectProperty();
        prop_Inner->CPPType = "UActorComponent*";
        prop_Inner->ElementSize = sizeof(UActorComponent*);
        prop->Inner = prop_Inner;

        prop->Name = FName("OwnedComponents");
        prop->CPPType = "TArray<UActorComponent*>";
        prop->Offset = offsetof(AActor, OwnedComponents);
        prop->ElementSize = sizeof(TArray<UActorComponent*>);
        prop->Flags = PF_None;
        prop->Category = "Default";
        prop->DisplayName = "OwnedComponents";
        info.ReflectedProperties.push_back(prop);
    }

    {
        FArrayProperty* prop = new FArrayProperty();
        prop->ArrayOps = TScriptArrayOps<FString>::Make();
        FStrProperty* prop_Inner = new FStrProperty();
        prop_Inner->CPPType = "FString";
        prop_Inner->ElementSize = sizeof(FString);
        prop->Inner = prop_Inner;

        prop->Name = FName("Tags");
        prop->CPPType = "TArray<FString>";
        prop->Offset = offsetof(AActor, Tags);
        prop->ElementSize = sizeof(TArray<FString>);
        prop->Flags = PF_EditAnywhere;
        prop->Category = "Actor";
        prop->DisplayName = "Tags";
        info.ReflectedProperties.push_back(prop);
    }

    ReflectionDatabase::AddClass("AActor", &info);
}

struct FAutoRegister_AActor
{
    FAutoRegister_AActor() { Register_AActor(); }
};
static FAutoRegister_AActor AutoRegister_AActor_Instance;

void BindLua_AActor(sol::state& Lua)
{
    sol::table UserType = Lua["Actor"];
    if (!UserType.valid())
    {
        return;
    }

}

