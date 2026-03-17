#pragma once

#include <vector>
#include "UObject.h"

class AActor;
class UActorComponent;
class UPrimitiveComponent;
class FScene;
class APrimitiveActor;

class UWorld : public UObject
{
public:
    UWorld();
    ~UWorld();

public:
    void AddActor(AActor* InActor);

    void BeginPlay();
    void Tick(float DeltaTime);

    void BuildScene(FScene& OutScene) const;

    const TArray<AActor*>& GetActors() const;

private:
    TArray<AActor*> Actors; // 실제 Actor와 에디터용 객체(기즈모, 월드 축, 카메라 등) 저장하는 리스트 분리 예정
    bool bHasBegunPlay;
};