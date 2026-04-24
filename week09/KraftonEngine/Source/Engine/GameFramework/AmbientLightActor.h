// 게임 프레임워크 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "GameFramework/AActor.h"
#include "Platform/Paths.h"

class UAmbientLightComponent;
class UBillboardComponent;

// AAmbientLightActor는 월드에 배치되는 게임 오브젝트를 표현합니다.
class AAmbientLightActor : public AActor
{
public:
    DECLARE_CLASS(AAmbientLightActor, AActor)
    AAmbientLightActor();

    void InitDefaultComponents();

private:
    UAmbientLightComponent* AmbientLightComponent = nullptr;
    UBillboardComponent* BillboardComponent = nullptr;
    FString AmbientLightIconPath = FPaths::EditorRelativePath("Icons/Materials/AmbientLightIcon.json");
};
