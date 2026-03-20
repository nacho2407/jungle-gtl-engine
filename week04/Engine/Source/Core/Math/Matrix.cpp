#include "../CoreMinimal.h"

FMatrix FMatrix::TranslationMatrix(const FVector &InVec)
{
    return TranslationMatrix(InVec.x, InVec.y, InVec.z);
}

FMatrix FMatrix::TranslationMatrix(float InX, float InY, float InZ)
{
    FMatrix M = IdentityMatrix();
    M[3][0] = InX;
    M[3][1] = InY;
    M[3][2] = InZ;
    return M;
}

FMatrix FMatrix::ScaleMatrix(const FVector &InVec)
{
    return ScaleMatrix(InVec.x, InVec.y, InVec.z);
}

FMatrix FMatrix::ScaleMatrix(float InX, float InY, float InZ)
{
    FMatrix M = IdentityMatrix();
    M[0][0] = InX;
    M[1][1] = InY;
    M[2][2] = InZ;
    return M;
}

FVector FMatrix::TransformPoint(const FVector &V) const
{
    return FVector(V.x * m[0][0] + V.y * m[1][0] + V.z * m[2][0] + m[3][0],
                   V.x * m[0][1] + V.y * m[1][1] + V.z * m[2][1] + m[3][1],
                   V.x * m[0][2] + V.y * m[1][2] + V.z * m[2][2] + m[3][2]);
}

FVector FMatrix::TransformVector(const FVector &V) const
{
    return FVector(V.x * m[0][0] + V.y * m[1][0] + V.z * m[2][0],
                   V.x * m[0][1] + V.y * m[1][1] + V.z * m[2][1],
                   V.x * m[0][2] + V.y * m[1][2] + V.z * m[2][2]);
}

FMatrix FMatrix::RotationXMatrix(float InRoll)
{
    FMatrix Mat = IdentityMatrix();

    float Rad = MathHelper::DegToRad(InRoll);
    float C = std::cos(Rad);
    float S = std::sin(Rad);

    Mat[1][1] = C;
    Mat[1][2] = S;
    Mat[2][1] = -S;
    Mat[2][2] = C;

    return Mat;
}

FMatrix FMatrix::RotationYMatrix(float InPitch)
{
    FMatrix Mat = IdentityMatrix();

    float Rad = MathHelper::DegToRad(InPitch);
    float C = std::cos(Rad);
    float S = std::sin(Rad);

    Mat[0][0] = C;
    Mat[0][2] = -S;
    Mat[2][0] = S;
    Mat[2][2] = C;

    return Mat;
}

FMatrix FMatrix::RotationZMatrix(float InYaw)
{
    FMatrix Mat = IdentityMatrix();

    float Rad = MathHelper::DegToRad(InYaw);
    float C = std::cos(Rad);
    float S = std::sin(Rad);

    Mat[0][0] = C;
    Mat[0][1] = S;
    Mat[1][0] = -S;
    Mat[1][1] = C;

    return Mat;
}

FMatrix FMatrix::EulerRotationMatrix(const FRotator &InRot)
{
    return EulerRotationMatrix(InRot.Roll, InRot.Pitch, InRot.Yaw);
}

FMatrix FMatrix::EulerRotationMatrix(float InRoll, float InPitch, float InYaw)
{
    FMatrix Rx = RotationXMatrix(InRoll);
    FMatrix Ry = RotationYMatrix(InPitch);
    FMatrix Rz = RotationZMatrix(InYaw);

    return Rx * Ry * Rz;
}

FMatrix FMatrix::operator*(const FMatrix &Other) const
{
    FMatrix Result = {};

    for (int Row = 0; Row < 4; ++Row)
    {
        for (int Col = 0; Col < 4; ++Col)
        {
            Result[Row][Col] = m[Row][0] * Other[0][Col] + m[Row][1] * Other[1][Col] +
                               m[Row][2] * Other[2][Col] + m[Row][3] * Other[3][Col];
        }
    }
    return Result;
}

FMatrix FMatrix::ViewMatrix(FVector Eye, FVector Target, FVector Up)
{
    FVector F = (Target - Eye).Normalize(); // Forward
    FVector R = Up.Cross(F).Normalize();    // Right
    FVector U = F.Cross(R);                 // Up

    FMatrix M = IdentityMatrix();

    M.m[0][0] = R.x;
    M.m[1][0] = R.y;
    M.m[2][0] = R.z;
    M.m[3][0] = -(R.Dot(Eye));

    M.m[0][1] = U.x;
    M.m[1][1] = U.y;
    M.m[2][1] = U.z;
    M.m[3][1] = -(U.Dot(Eye));

    M.m[0][2] = F.x;
    M.m[1][2] = F.y;
    M.m[2][2] = F.z;
    M.m[3][2] = -(F.Dot(Eye));

    M.m[0][3] = 0.0f;
    M.m[1][3] = 0.0f;
    M.m[2][3] = 0.0f;
    M.m[3][3] = 1.0f;

    return M;
}

FMatrix FMatrix::PerspectiveMatrix(float FovDegree, float Aspect, float Near, float Far)
{
    FMatrix M = ZeroMatrix();

    float Rad = MathHelper::DegToRad(FovDegree);
    float ScaleY = 1.0f / std::tan(Rad * 0.5f);
    float ScaleX = ScaleY / Aspect;

    M.m[0][0] = ScaleX;
    M.m[1][1] = ScaleY;

    // Z축 정규화 (0 ~ 1 범위로 변환)
    M.m[2][2] = -Near / (Far - Near);
    M.m[2][3] = 1.0f;
    M.m[3][2] = (Far * Near) / (Far - Near);
    M.m[3][3] = 0.0f;

    return M;
}

FMatrix FMatrix::Inverse() const
{
    float n11 = m[0][0], n12 = m[0][1], n13 = m[0][2], n14 = m[0][3];
    float n21 = m[1][0], n22 = m[1][1], n23 = m[1][2], n24 = m[1][3];
    float n31 = m[2][0], n32 = m[2][1], n33 = m[2][2], n34 = m[2][3];
    float n41 = m[3][0], n42 = m[3][1], n43 = m[3][2], n44 = m[3][3];

    // 사루스 공식(3x3 행렬식 구하기)
    // https://blockchainstudy.tistory.com/124
    float t11 = n23 * n34 * n42 - n24 * n33 * n42 + n24 * n32 * n43 - n22 * n34 * n43 -
                n23 * n32 * n44 + n22 * n33 * n44;
    float t12 = n14 * n33 * n42 - n13 * n34 * n42 - n14 * n32 * n43 + n12 * n34 * n43 +
                n13 * n32 * n44 - n12 * n33 * n44;
    float t13 = n13 * n24 * n42 - n14 * n23 * n42 + n14 * n22 * n43 - n12 * n24 * n43 -
                n13 * n22 * n44 + n12 * n23 * n44;
    float t14 = n14 * n23 * n32 - n13 * n24 * n32 - n14 * n22 * n33 + n12 * n24 * n33 +
                n13 * n22 * n34 - n12 * n23 * n34;

    // 라플라스 전개를 통한 행렬식(Determinant) 계산
    // 한 (열,행)을  잡고 그에 대한 여인수(cofactor)들을 구해서 행렬식을 계산하는 방법
    // https://m.blog.naver.com/bosstudyroom/221732083775
    float Det = n11 * t11 + n21 * t12 + n31 * t13 + n41 * t14;

    // 행렬식이 0에 가까우면 역행렬이 존재하지 않음
    // 행렬이 차원을 축소시키는 경우
    if (std::abs(Det) < 1e-6f)
    {
        return ZeroMatrix();
    }

    float InvDet = 1.0f / Det;

    // 각 행렬 원소들의 여인수를 구하고 InvDet로 나누어서 역행렬을 계산
    FMatrix Result;
    Result.m[0][0] = t11 * InvDet;
    Result.m[0][1] = t12 * InvDet;
    Result.m[0][2] = t13 * InvDet;
    Result.m[0][3] = t14 * InvDet;

    Result.m[1][0] = (n24 * n33 * n41 - n23 * n34 * n41 - n24 * n31 * n43 + n21 * n34 * n43 +
                      n23 * n31 * n44 - n21 * n33 * n44) *
                     InvDet;
    Result.m[1][1] = (n13 * n34 * n41 - n14 * n33 * n41 + n14 * n31 * n43 - n11 * n34 * n43 -
                      n13 * n31 * n44 + n11 * n33 * n44) *
                     InvDet;
    Result.m[1][2] = (n14 * n23 * n41 - n13 * n24 * n41 - n14 * n21 * n43 + n11 * n24 * n43 +
                      n13 * n21 * n44 - n11 * n23 * n44) *
                     InvDet;
    Result.m[1][3] = (n13 * n24 * n31 - n14 * n23 * n31 + n14 * n21 * n33 - n11 * n24 * n33 -
                      n13 * n21 * n34 + n11 * n23 * n34) *
                     InvDet;

    Result.m[2][0] = (n22 * n34 * n41 - n24 * n32 * n41 + n24 * n31 * n42 - n21 * n34 * n42 -
                      n22 * n31 * n44 + n21 * n32 * n44) *
                     InvDet;
    Result.m[2][1] = (n14 * n32 * n41 - n12 * n34 * n41 - n14 * n31 * n42 + n11 * n34 * n42 +
                      n12 * n31 * n44 - n11 * n32 * n44) *
                     InvDet;
    Result.m[2][2] = (n12 * n24 * n41 - n14 * n22 * n41 + n14 * n21 * n42 - n11 * n24 * n42 -
                      n12 * n21 * n44 + n11 * n22 * n44) *
                     InvDet;
    Result.m[2][3] = (n14 * n22 * n31 - n12 * n24 * n31 - n14 * n21 * n32 + n11 * n24 * n32 +
                      n12 * n21 * n34 - n11 * n22 * n34) *
                     InvDet;

    Result.m[3][0] = (n23 * n32 * n41 - n22 * n33 * n41 - n23 * n31 * n42 + n21 * n33 * n42 +
                      n22 * n31 * n43 - n21 * n32 * n43) *
                     InvDet;
    Result.m[3][1] = (n12 * n33 * n41 - n13 * n32 * n41 + n13 * n31 * n42 - n11 * n33 * n42 -
                      n12 * n31 * n43 + n11 * n32 * n43) *
                     InvDet;
    Result.m[3][2] = (n13 * n22 * n41 - n12 * n23 * n41 - n13 * n21 * n42 + n11 * n23 * n42 +
                      n12 * n21 * n43 - n11 * n22 * n43) *
                     InvDet;
    Result.m[3][3] = (n12 * n23 * n31 - n13 * n22 * n31 + n13 * n21 * n32 - n11 * n23 * n32 -
                      n12 * n21 * n33 + n11 * n22 * n33) *
                     InvDet;

    return Result;
}
