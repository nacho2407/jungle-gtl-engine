// 게임 프레임워크 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "GameFramework/AActor.h"
#include "Platform/Paths.h"

class UDecalComponent;
class UBillboardComponent;

// ADecalActor는 월드에 배치되는 게임 오브젝트를 표현합니다.
class ADecalActor : public AActor
{
public:
    DECLARE_CLASS(ADecalActor, AActor)

    ADecalActor();

    void InitDefaultComponents();

    UDecalComponent* GetDecalComponent() const { return DecalComponent; }

private:
    UDecalComponent* DecalComponent;
    UBillboardComponent* BillboardComponent = nullptr;

    const FString DefaultDecalMaterialPath = FPaths::AssetRelativePath("Content/Materials/DefaultDecal.json");
    const FString DecalIconPath = FPaths::EditorRelativePath("Icons/Materials/DecalIcon.json");
};
