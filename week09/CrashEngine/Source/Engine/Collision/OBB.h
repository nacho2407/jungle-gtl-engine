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

        FVector AxisX(Matrix.M[0][0], Matrix.M[0][1], Matrix.M[0][2]);
        FVector AxisY(Matrix.M[1][0], Matrix.M[1][1], Matrix.M[1][2]);
        FVector AxisZ(Matrix.M[2][0], Matrix.M[2][1], Matrix.M[2][2]);

        Extent = FVector(AxisX.Length(), AxisY.Length(), AxisZ.Length()) * 0.5f;

        AxisX = AxisX.Normalized();
        AxisY = AxisY.Normalized();
        AxisZ = AxisZ.Normalized();

        FMatrix RotMat = FMatrix::Identity;
        RotMat.M[0][0] = AxisX.X;
        RotMat.M[0][1] = AxisX.Y;
        RotMat.M[0][2] = AxisX.Z;
        RotMat.M[1][0] = AxisY.X;
        RotMat.M[1][1] = AxisY.Y;
        RotMat.M[1][2] = AxisY.Z;
        RotMat.M[2][0] = AxisZ.X;
        RotMat.M[2][1] = AxisZ.Y;
        RotMat.M[2][2] = AxisZ.Z;

        Rotation = RotMat.ToRotator();
    }


    // Decal receiver narrow phase에서 사용하는 OBB vs AABB SAT 판정입니다.
    bool IntersectOBBAABB(const FBoundingBox& AABB) const
    {
        return FMath::IntersectOBBAABB(Center, Extent, Rotation, AABB);
    }
};
