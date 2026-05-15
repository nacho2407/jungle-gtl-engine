// [UHT 자동 생성 소스 파일 - 절대 직접 수정하지 마세요!]
#include "Engine/Component/StaticMeshComponent.h"
#include "Core/ReflectionDatabase.h" 

void Register_UStaticMeshComponent() {
    FClassInfo& info = UStaticMeshComponent::StaticClassInfo;
    info.ClassName = "UStaticMeshComponent";
    info.Properties.push_back({ "StaticMeshAsset", "UStaticMesh*", offsetof(UStaticMeshComponent, StaticMeshAsset), true });
    info.GcPointerOffsets.push_back(offsetof(UStaticMeshComponent, StaticMeshAsset)); // GC 추적 대상
    info.Properties.push_back({ "StaticMeshAssetPath", "FString", offsetof(UStaticMeshComponent, StaticMeshAssetPath), true });

    ReflectionDatabase::AddClass("UStaticMeshComponent", &info);
}

struct FAutoRegister_UStaticMeshComponent {
    FAutoRegister_UStaticMeshComponent() {
        Register_UStaticMeshComponent();
    }
};
static FAutoRegister_UStaticMeshComponent AutoRegister_UStaticMeshComponent_Instance;
