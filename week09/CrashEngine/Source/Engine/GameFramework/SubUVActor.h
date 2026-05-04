// 게임 프레임워크 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "GameFramework/AActor.h"

class USubUVComponent;

class ASubUVActor : public AActor
{
public:
    DECLARE_CLASS(ASubUVActor, AActor)

    ASubUVActor();

    void InitDefaultComponents();
    USubUVComponent* GetSubUVComponent() const { return SubUVComponent; }

private:
    USubUVComponent* SubUVComponent = nullptr;
};
