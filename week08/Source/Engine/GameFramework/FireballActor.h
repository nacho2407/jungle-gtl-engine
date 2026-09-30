// 게임 프레임워크 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once
#include "AActor.h"
#include "Platform/Paths.h"

class UDecalComponent;
class UStaticMeshComponent;

// AFireballActor는 월드에 배치되는 게임 오브젝트를 표현합니다.
class AFireballActor : public AActor
{
public:
    DECLARE_CLASS(AFireballActor, AActor);
    AFireballActor();

    void InitDefaultComponents();

private:
    UStaticMeshComponent* StaticMeshComponent = nullptr;
    UDecalComponent* DecalComponents[3] = {
        nullptr,
    }; // xyz 각 방향으로 1개씩
    const FString FireballMeshName = FPaths::ContentRelativePath("Models/_Basic/Sphere.OBJ");
    const FString LightAreaMaterialPath = FPaths::ContentRelativePath("Materials/FakeLight_LightArea.json");
};
