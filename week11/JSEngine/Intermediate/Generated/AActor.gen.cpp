// [UHT generated source - do not edit]
#include "Engine/GameFramework/AActor.h"
#include "Core/ReflectionDatabase.h"

void Register_AActor()
{
    FClassInfo& info = AActor::StaticClassInfo;
    info.Properties.clear();
    info.GcPointerOffsets.clear();
    info.ClassName = "AActor";
    info.ParentClassName = "UObject";
    info.Properties.push_back({ "RootComponent", "USceneComponent*", offsetof(AActor, RootComponent), true });
    info.GcPointerOffsets.push_back(offsetof(AActor, RootComponent));
    info.Properties.push_back({ "OwningWorld", "UWorld*", offsetof(AActor, OwningWorld), false });
    info.GcPointerOffsets.push_back(offsetof(AActor, OwningWorld));
    info.Properties.push_back({ "PendingActorLocation", "FVector", offsetof(AActor, PendingActorLocation), true });
    info.Properties.push_back({ "PendingActorRotation", "FVector", offsetof(AActor, PendingActorRotation), true });
    info.Properties.push_back({ "PendingActorScale", "FVector", offsetof(AActor, PendingActorScale), true });
    info.Properties.push_back({ "bVisible", "bool", offsetof(AActor, bVisible), true });
    info.Properties.push_back({ "bIsActive", "bool", offsetof(AActor, bIsActive), true });
    info.Properties.push_back({ "bTickInEditor", "bool", offsetof(AActor, bTickInEditor), true });
    info.Properties.push_back({ "OwnedComponents", "TArray<UActorComponent*>", offsetof(AActor, OwnedComponents), false });
    info.GcPointerOffsets.push_back(offsetof(AActor, OwnedComponents));
    info.Properties.push_back({ "Tags", "TArray<FString>", offsetof(AActor, Tags), true });
    ReflectionDatabase::AddClass("AActor", &info);
}

struct FAutoRegister_AActor
{
    FAutoRegister_AActor() { Register_AActor(); }
};
static FAutoRegister_AActor AutoRegister_AActor_Instance;

