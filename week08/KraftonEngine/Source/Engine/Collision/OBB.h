// 충돌/피킹 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once
#include "Core/EngineTypes.h"
#include "Math/Matrix.h"
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
        const FVector AABBCenter = AABB.GetCenter();
        const FVector AABBExtent = AABB.GetExtent();
        const FVector OBBAxes[3] = {
            Rotation.GetForwardVector().Normalized(),
            Rotation.GetRightVector().Normalized(),
            Rotation.GetUpVector().Normalized()
        };
        const FVector AABBAxes[3] = {
            FVector(1.0f, 0.0f, 0.0f),
            FVector(0.0f, 1.0f, 0.0f),
            FVector(0.0f, 0.0f, 1.0f)
        };

        float R[3][3];
        float AbsR[3][3];
        for (int i = 0; i < 3; ++i)
        {
            for (int j = 0; j < 3; ++j)
            {
                R[i][j] = OBBAxes[i].Dot(AABBAxes[j]);
                AbsR[i][j] = std::abs(R[i][j]) + 1e-6f;
            }
        }

        const FVector Translation = AABBCenter - Center;
        const FVector T(
            Translation.Dot(OBBAxes[0]),
            Translation.Dot(OBBAxes[1]),
            Translation.Dot(OBBAxes[2]));

        float ra = 0.0f;
        float rb = 0.0f;

        for (int i = 0; i < 3; ++i)
        {
            ra = Extent.Data[i];
            rb = AABBExtent.X * AbsR[i][0] + AABBExtent.Y * AbsR[i][1] + AABBExtent.Z * AbsR[i][2];
            if (std::abs(T.Data[i]) > ra + rb)
            {
                return false;
            }
        }

        for (int j = 0; j < 3; ++j)
        {
            ra = Extent.X * AbsR[0][j] + Extent.Y * AbsR[1][j] + Extent.Z * AbsR[2][j];
            rb = AABBExtent.Data[j];
            const float Distance = std::abs(T.X * R[0][j] + T.Y * R[1][j] + T.Z * R[2][j]);
            if (Distance > ra + rb)
            {
                return false;
            }
        }

        for (int i = 0; i < 3; ++i)
        {
            for (int j = 0; j < 3; ++j)
            {
                ra = Extent.Data[(i + 1) % 3] * AbsR[(i + 2) % 3][j] + Extent.Data[(i + 2) % 3] * AbsR[(i + 1) % 3][j];
                rb = AABBExtent.Data[(j + 1) % 3] * AbsR[i][(j + 2) % 3] + AABBExtent.Data[(j + 2) % 3] * AbsR[i][(j + 1) % 3];
                const float Distance = std::abs(
                    T.Data[(i + 2) % 3] * R[(i + 1) % 3][j] - T.Data[(i + 1) % 3] * R[(i + 2) % 3][j]);
                if (Distance > ra + rb)
                {
                    return false;
                }
            }
        }

        return true;
    }
};
