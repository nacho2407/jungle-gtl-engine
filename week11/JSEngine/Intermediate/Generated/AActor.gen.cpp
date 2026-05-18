// [UHT generated source - do not edit]
#include "Engine/GameFramework/AActor.h"
#include "Core/ReflectionDatabase.h"
#include "Core/ReflectionUtils.h"

void Register_AActor()
{
    FClassInfo& info = AActor::StaticClassInfo;
    info.Properties.clear();
    info.GcPointerOffsets.clear();
    info.ClassName = "AActor";
    info.ParentClassName = "UObject";
    info.Properties.push_back({ "RootComponent", "USceneComponent*", offsetof(AActor, RootComponent), PF_EditAnywhere, "Actor", "Root Component" });
    info.GcPointerOffsets.push_back(offsetof(AActor, RootComponent));
    info.Properties.push_back({ "OwningWorld", "UWorld*", offsetof(AActor, OwningWorld), PF_None, "Default", "OwningWorld" });
    info.GcPointerOffsets.push_back(offsetof(AActor, OwningWorld));
    info.Properties.push_back({ "PendingActorLocation", "FVector", offsetof(AActor, PendingActorLocation), PF_EditAnywhere, "Transform", "Location" });
    info.Properties.push_back({ "PendingActorRotation", "FVector", offsetof(AActor, PendingActorRotation), PF_EditAnywhere, "Transform", "Rotation" });
    info.Properties.push_back({ "PendingActorScale", "FVector", offsetof(AActor, PendingActorScale), PF_EditAnywhere, "Transform", "Scale" });
    info.Properties.push_back({ "bVisible", "bool", offsetof(AActor, bVisible), PF_EditAnywhere, "Actor", "Visible" });
    info.Properties.push_back({ "bIsActive", "bool", offsetof(AActor, bIsActive), PF_EditAnywhere, "Actor", "Active" });
    info.Properties.push_back({ "bTickInEditor", "bool", offsetof(AActor, bTickInEditor), PF_EditAnywhere, "Actor", "Tick In Editor" });
    info.Properties.push_back({ "OwnedComponents", "TArray<UActorComponent*>", offsetof(AActor, OwnedComponents), PF_None, "Default", "OwnedComponents" });
    info.GcPointerOffsets.push_back(offsetof(AActor, OwnedComponents));
    info.Properties.push_back({ "Tags", "TArray<FString>", offsetof(AActor, Tags), PF_EditAnywhere, "Actor", "Tags" });
    ReflectionDatabase::AddClass("AActor", &info);
}

struct FAutoRegister_AActor
{
    FAutoRegister_AActor() { Register_AActor(); }
};
static FAutoRegister_AActor AutoRegister_AActor_Instance;

