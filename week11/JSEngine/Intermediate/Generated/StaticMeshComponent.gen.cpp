// [UHT generated source - do not edit]
#include "Engine/Component/StaticMeshComponent.h"
#include "Core/ReflectionDatabase.h"

void Register_UStaticMeshComponent()
{
    FClassInfo& info = UStaticMeshComponent::StaticClassInfo;
    info.Properties.clear();
    info.GcPointerOffsets.clear();
    info.ClassName = "UStaticMeshComponent";
    info.ParentClassName = "UMeshComponent";
    info.ParentClass = ReflectionDatabase::GetClass("UMeshComponent");
    info.Properties.push_back({ "StaticMeshAsset", "UStaticMesh*", offsetof(UStaticMeshComponent, StaticMeshAsset), true });
    info.GcPointerOffsets.push_back(offsetof(UStaticMeshComponent, StaticMeshAsset));
    info.Properties.push_back({ "StaticMeshAssetPath", "FString", offsetof(UStaticMeshComponent, StaticMeshAssetPath), true });
    ReflectionDatabase::AddClass("UStaticMeshComponent", &info);
}

struct FAutoRegister_UStaticMeshComponent
{
    FAutoRegister_UStaticMeshComponent() { Register_UStaticMeshComponent(); }
};
static FAutoRegister_UStaticMeshComponent AutoRegister_UStaticMeshComponent_Instance;

