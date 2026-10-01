// 게임 프레임워크 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "GameFramework/AActor.h"
#include "Platform/Paths.h"

class UStaticMeshComponent;
class UCylindricalBillboardComponent;
class UDecalComponent;

// AFakeLightActor는 월드에 배치되는 게임 오브젝트를 표현합니다.
class AFakeLightActor : public AActor
{
public:
    DECLARE_CLASS(AFakeLightActor, AActor)
    AFakeLightActor();

    void InitDefaultComponents();

private:
    UStaticMeshComponent* StaticMeshComponent = nullptr;
    UCylindricalBillboardComponent* BillboardComponent = nullptr;
    UDecalComponent* DecalComponent = nullptr;

    // TODO: Remove Magic Numbers
    FString LampMeshDir = FPaths::ContentRelativePath("Models/Retro-light/RetroLight.OBJ");
    FString LampshadeMaterialPath = FPaths::ContentRelativePath("Materials/Lampshade.json");
    FString DecalMaterialPath = FPaths::ContentRelativePath("Materials/FakeLight_LightArea.json");
};
