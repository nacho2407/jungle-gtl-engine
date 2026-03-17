#pragma once

#include "Component/UPrimitiveComponent.h"

class UStaticMesh;

class UStaticMeshComponent : public UPrimitiveComponent
{
public:
private: inline static UClassData* StaticClass = nullptr; public: static UClassData* GetClass() {
    if (StaticClass != nullptr) return StaticClass; UClassData* classData = new UClassData(); classData->ClassName = "UStaticMeshComponent"; classData->ClassSize = sizeof(UStaticMeshComponent); classData->SuperClass = nullptr; StaticClass = classData; return StaticClass;
} void Initialize(const FUObjectInitializer& ObjectInitilizer) {
    UUID = ObjectInitilizer.UUID; Name = ObjectInitilizer.Name;
}

    UStaticMeshComponent();
    virtual ~UStaticMeshComponent();

public:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime) override;

public:
    void SetStaticMesh(UStaticMesh* InMesh);
    UStaticMesh* GetStaticMesh() const;

    virtual FRenderItem CreateRenderItem() const override;

private:
    UStaticMesh* StaticMesh = nullptr;
};
