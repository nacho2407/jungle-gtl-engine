#pragma once

#include "Component/UActorComponent.h"
#include "Math/FVector.h"
#include "Math/FMatrix.h"

class USceneComponent : public UActorComponent
{
public:
    USceneComponent();
    virtual ~USceneComponent();

public:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime) override;

public:
    void SetRelativeLocation(const FVector& InLocation);
    const FVector& GetRelativeLocation() const;

    void SetRelativeRotation(const FVector& InRotation);
    const FVector& GetRelativeRotation() const;

    void SetRelativeScale(const FVector& InScale);
    const FVector& GetRelativeScale() const;

    virtual FMatrix GetWorldTransformMatrix() const;

protected:
    FVector RelativeLocation;
    FVector RelativeRotation;
    FVector RelativeScale;
};