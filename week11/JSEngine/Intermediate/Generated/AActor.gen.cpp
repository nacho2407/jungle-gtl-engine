// [UHT 자동 생성 소스 파일 - 절대 직접 수정하지 마세요!]
#include "Engine/GameFramework/AActor.h"
#include "Core/ReflectionDatabase.h" 

void Register_AActor() {
    FClassInfo& info = AActor::StaticClassInfo;
    info.ClassName = "AActor";
    info.Properties.push_back({ "RootComponent", "USceneComponent*", offsetof(AActor, RootComponent), true });
    info.GcPointerOffsets.push_back(offsetof(AActor, RootComponent)); // GC 추적 대상
    info.Properties.push_back({ "OwningWorld", "UWorld*", offsetof(AActor, OwningWorld), false });
    info.GcPointerOffsets.push_back(offsetof(AActor, OwningWorld)); // GC 추적 대상
    info.Properties.push_back({ "bVisible", "bool", offsetof(AActor, bVisible), true });
    info.Properties.push_back({ "bIsActive", "bool", offsetof(AActor, bIsActive), true });
    info.Properties.push_back({ "bTickInEditor", "bool", offsetof(AActor, bTickInEditor), true });
    info.Properties.push_back({ "OwnedComponents", "TArray<UActorComponent*>", offsetof(AActor, OwnedComponents), false });
    info.GcPointerOffsets.push_back(offsetof(AActor, OwnedComponents)); // GC 추적 대상
    info.Properties.push_back({ "Tags", "TArray<FString>", offsetof(AActor, Tags), true });

    ReflectionDatabase::AddClass("AActor", &info);
}

struct FAutoRegister_AActor {
    FAutoRegister_AActor() {
        Register_AActor();
    }
};
static FAutoRegister_AActor AutoRegister_AActor_Instance;
