// 게임 프레임워크 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "GameFramework/AActor.h"
#include "Platform/Paths.h"

class UBillboardComponent;
class UCameraBoomComponent;
class UCameraComponent;

// ACameraActor는 게임에서 사용할 카메라 리그 액터입니다.
class ACameraActor : public AActor
{
public:
    DECLARE_CLASS(ACameraActor, AActor)

    ACameraActor();

    void InitDefaultComponents();
    void BeginPlay() override;

    UCameraBoomComponent* GetCameraBoomComponent() const { return CameraBoomComponent; }
    UCameraComponent* GetCameraComponent() const { return CameraComponent; }

private:
    UCameraBoomComponent* CameraBoomComponent = nullptr;
    UCameraComponent* CameraComponent = nullptr;
    UBillboardComponent* BillboardComponent = nullptr;
    FString CameraActorIconPath = FPaths::EditorRelativePath("Icons/Materials/CameraActorIcon.json");
};
