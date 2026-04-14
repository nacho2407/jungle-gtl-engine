#pragma once

#include "Core/CoreMinimal.h"
#include "Engine/Geometry/Ray.h"
#include "Engine/Geometry/Frustum.h"

enum class EViewportProjectionType
{
    Perspective,
    Orthographic
};

class FViewportCamera
{
  public:
    FViewportCamera() = default;
    ~FViewportCamera() = default;

    void SetLocation(const FVector& InLocation);
    void SetRotation(const FQuat& InRotation);
    void SetRotation(const FRotator& InRotation);

    const FVector& GetLocation() const { return Location; }
    const FQuat&   GetRotation() const { return Rotation; }

    FVector GetForwardVector() const;
    FVector GetRightVector() const;
    FVector GetUpVector() const;

    // 직교 뷰의 Custom LookDir 를 반영한 실제 화면 오른쪽/위 벡터
    // Pan / 기타 입력에서 올바른 세계 축을 구하기 위해 사용합니다.
    FVector GetEffectiveForward() const;
    FVector GetEffectiveRight() const;
    FVector GetEffectiveUp() const;

    FMatrix         GetViewMatrix() const;
    FMatrix         GetProjectionMatrix() const;
    FMatrix         GetViewProjectionMatrix() const;
    FRay            DeprojectScreenToWorld(float ScreenX, float ScreenY, float ScreenWidth, float ScreenHeight) const;
    const FFrustum& GetFrustum() const;

    void                    SetProjectionType(EViewportProjectionType InType);
    EViewportProjectionType GetProjectionType() const { return ProjectionType; }
    bool                    IsOrthographic() const { return ProjectionType == EViewportProjectionType::Orthographic; }

    void SetFOV(float InFOV);
    void SetNearPlane(float InNear);
    void SetFarPlane(float InFar);
    void SetOrthoHeight(float InHeight);
    void SetLookAt(const FVector& Target);

    float GetFOV() const { return FOV; }
    float GetNearPlane() const { return NearPlane; }
    float GetFarPlane() const { return FarPlane; }
    float GetOrthoHeight() const { return OrthoHeight; }

    void OnResize(uint32 InWidth, uint32 InHeight);

    uint32 GetWidth() const { return Width; }
    uint32 GetHeight() const { return Height; }
    float  GetAspectRatio() const { return AspectRatio; }

    /*
     * 직교 뷰 카메라 방향 직접 지정.
     * LookAt 계산 시 FQuat::GetForwardVector() 대신 InLookDir 를 사용하고,
     * View Up 벡터도 InViewUp 으로 고정합니다.
     * Top 뷰처럼 Forward 와 기본 Up (0,0,1) 이 평행한 경우 필수.
     */
    void SetCustomLookDir(const FVector& InLookDir, const FVector& InViewUp);
    void ClearCustomLookDir();

    const FVector& GetViewUp() const { return ViewUp; }

  private:
    void MarkViewDirty()
    {
        bIsViewDirty = true;
        bIsFrustumDirty = true;
    }
    void MarkProjectionDirty()
    {
        bIsProjectionDirty = true;
        bIsFrustumDirty = true;
    }

  private:
    FVector Location = FVector::ZeroVector;
    FQuat   Rotation = FQuat::Identity;

    // 직교 뷰 방향 고정용 (SetCustomLookDir 로 설정)
    FVector ViewUp = FVector(0.f, 0.f, 1.f);
    bool    bHasCustomLookDir = false;
    FVector CustomLookDir = FVector(1.f, 0.f, 0.f);

    EViewportProjectionType ProjectionType = EViewportProjectionType::Perspective;

    uint32 Width = 1920;
    uint32 Height = 1080;
    float  AspectRatio = 16.0f / 9.0f;

    float FOV = 3.14159265358979f * 90.0f / 180.0f;
    float NearPlane = 0.1f;
    float FarPlane = 2000.0f;

    // OrthoWidth 대신 높이를 기준으로 관리
    float OrthoHeight = 10.0f;

    mutable FMatrix CachedViewMatrix = FMatrix::Identity;
    mutable FMatrix CachedProjectionMatrix = FMatrix::Identity;
    mutable bool    bIsViewDirty = true;
    mutable bool    bIsProjectionDirty = true;

    mutable FFrustum CachedFrustum;
    mutable bool     bIsFrustumDirty = true;
};

// PIE 시작 전 카메라 상태를 저장하고 정지 시 복원하기 위한 스냅샷
struct FCameraSnapshot
{
    FVector Location  = FVector::ZeroVector;
    FQuat   Rotation  = FQuat::Identity;
    float   FOV       = 3.14159265358979f * 90.0f / 180.0f;
    float   NearPlane = 0.1f;
    float   FarPlane  = 2000.0f;
};
