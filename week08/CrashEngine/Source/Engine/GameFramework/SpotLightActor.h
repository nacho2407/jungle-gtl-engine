// 게임 프레임워크 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "GameFramework/AActor.h"
#include "Platform/Paths.h"

class USpotLightComponent;
class UBillboardComponent;

// ASpotLightActor는 월드에 배치되는 게임 오브젝트를 표현합니다.
class ASpotLightActor : public AActor
{
public:
    DECLARE_CLASS(ASpotLightActor, AActor)
    ASpotLightActor();

    void InitDefaultComponents();

private:
    USpotLightComponent* SpotLightComponent = nullptr;
    UBillboardComponent* BillboardComponent = nullptr;
    FString SpotLightIconPath = FPaths::EditorRelativePath("Icons/Materials/SpotLightIcon.json");
};
