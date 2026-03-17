#pragma once

#include "Component/UActorComponent.h"
#include "Math/FVector.h"
#include "Math/FMatrix.h"

// @@@@ 현재 USceneComponent에 Attachment 하는 게 없는거 아님??
class USceneComponent : public UActorComponent
{
public:
    DECLARE_UClass(USceneComponent, UActorComponent)

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
    void MarkTransformDirty();
    void UpdateWorldTransformIfNeeded() const;

protected:
    FVector RelativeLocation;
    FVector RelativeRotation;
    FVector RelativeScale;

    mutable bool bWorldTransformDirty;
    mutable FMatrix CachedWorldTransform;
};