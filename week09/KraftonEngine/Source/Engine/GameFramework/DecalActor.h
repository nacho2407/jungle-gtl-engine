#pragma once

#include "GameFramework/AActor.h"
#include "Platform/Paths.h"

class UDecalComponent;
class UBillboardComponent;

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

    const FString DefaultDecalMaterialPath = FPaths::EditorRelativePath("Icons/Materials/PawnIcon.json");
    const FString DecalIconPath = FPaths::EditorRelativePath("Icons/Materials/DecalIcon.json");
};
