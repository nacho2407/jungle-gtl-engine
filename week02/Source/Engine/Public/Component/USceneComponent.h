#pragma once

#include "Component/UActorComponent.h"
#include "Math/FVector.h"
#include "Math/FMatrix.h"
#include "Math/FQuat.h"

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

    // UI/기존 코드 호환용
    void SetRelativeRotation(const FVector& InRotationEuler);
    const FVector& GetRelativeRotation() const;

    // 실제 회전용
    void SetRelativeRotationQuat(const FQuat& InQuat);
    const FQuat& GetRelativeRotationQuat() const;

    void SetRelativeScale(const FVector& InScale);
    const FVector& GetRelativeScale() const;

    void SetRelativeTransformMatrix(const FMatrix& InMatrix);
    const FMatrix& GetRelativeTransformMatrix() const;

    void SetWorldTransformMatrix(const FMatrix& InMatrix);

    virtual FMatrix GetWorldTransformMatrix() const;

    void SetupAttachment(USceneComponent* InParent);
    USceneComponent* GetParentComponent() const;


protected:
    void MarkTransformDirty();
    void UpdateWorldTransformIfNeeded() const;

    void RebuildRelativeMatrixFromTRS();
    void SyncTRSCachesFromMatrixApprox();

protected:
    FVector RelativeLocation;
    FVector RelativeRotationEuler;
    FQuat RelativeRotationQuat;
    FVector RelativeScale;

    FMatrix RelativeTransformMatrix;

    mutable bool bWorldTransformDirty;
    mutable FMatrix CachedWorldTransform;

    USceneComponent* ParentComponent = nullptr;
    TArray<USceneComponent*> Children;

};