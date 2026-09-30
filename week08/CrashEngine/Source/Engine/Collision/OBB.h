// 충돌/피킹 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once
#include "Core/EngineTypes.h"
#include "Math/Matrix.h"
#include "Math/Intersection.h"
#include "Math/Vector.h"
#include "Math/Rotator.h"

// FOBB는 충돌/피킹 처리에 필요한 데이터를 묶는 구조체입니다.
struct FOBB
{
    FVector Center = { 0, 0, 0 };
    FVector Extent{ 0.5f, 0.5f, 0.5f };
    FRotator Rotation = { 0, 0, 0 };

    void ApplyTransform(FMatrix Matrix)
    {
        Center = Center * Matrix;

        FVector X = Rotation.GetForwardVector() * Extent.X;
        FVector Y = Rotation.GetRightVector() * Extent.Y;
        FVector Z = Rotation.GetUpVector() * Extent.Z;

        Matrix.M[3][0] = 0;
        Matrix.M[3][1] = 0;
        Matrix.M[3][2] = 0;

        X = X * Matrix;
        Y = Y * Matrix;
        Z = Z * Matrix;

        Extent.X = X.Length();
        Extent.Y = Y.Length();
        Extent.Z = Z.Length();

        FMatrix RotMat;
        RotMat.SetAxes(X.Normalized(), Y.Normalized(), Z.Normalized());
        Rotation = RotMat.ToRotator();
    }

    void UpdateAsOBB(const FMatrix& Matrix)
    {
        Center = Matrix.GetLocation();
        Rotation = Matrix.ToRotator();

        FVector Scale = Matrix.GetScale();
        Extent = Scale * 0.5f;
    }

    // Decal receiver narrow phase에서 사용하는 OBB vs AABB SAT 판정입니다.
    bool IntersectOBBAABB(const FBoundingBox& AABB) const
    {
        return FMath::IntersectOBBAABB(Center, Extent, Rotation, AABB);
    }
};
