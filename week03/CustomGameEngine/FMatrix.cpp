#include "FMatrix.h"
#include "FVector.h"
FMatrix FMatrix::ZeroMatrix()
{
	FMatrix M = IdentityMatrix();
	M[0][0] = 0;
	M[1][1] = 0;
	M[2][2] = 0;
	M[3][3] = 0;
	return M;
}

FMatrix FMatrix::TranslationMatrix(float InX, float InY, float InZ)
{
	FMatrix M = IdentityMatrix();
	M[3][0] = InX;
	M[3][1] = InY;
	M[3][2] = InZ;
	return M;
}

FMatrix FMatrix::ScaleMatrix(float InX, float InY, float InZ)
{
	FMatrix M = IdentityMatrix();
	M[0][0] = InX;
	M[1][1] = InY;
	M[2][2] = InZ;
	return M;
}

FMatrix FMatrix::RotationXMatrix(float InXDegree)
{
	FMatrix Mat = IdentityMatrix();

	float Rad = DegToRad(InXDegree);
	float C = std::cos(Rad);
	float S = std::sin(Rad);

	Mat[1][1] = C;
	Mat[1][2] = S;
	Mat[2][1] = -S;
	Mat[2][2] = C;

	return Mat;
}

FMatrix FMatrix::RotationYMatrix(float InYDegree)
{
	FMatrix Mat = IdentityMatrix();

	float Rad = DegToRad(InYDegree);
	float C = std::cos(Rad);
	float S = std::sin(Rad);

	Mat[0][0] = C;
	Mat[0][2] = -S;
	Mat[2][0] = S;
	Mat[2][2] = C;

	return Mat;
}

FMatrix FMatrix::RotationZMatrix(float InZDegree)
{
	FMatrix Mat = IdentityMatrix();

	float Rad = DegToRad(InZDegree);
	float C = std::cos(Rad);
	float S = std::sin(Rad);

	Mat[0][0] = C;
	Mat[0][1] = S;
	Mat[1][0] = -S;
	Mat[1][1] = C;

	return Mat;
}

FMatrix FMatrix::EulerRotationMatrix(float InX, float InY, float InZ)
{
	FMatrix Rx = RotationXMatrix(InX);
	FMatrix Ry = RotationYMatrix(InY);
	FMatrix Rz = RotationZMatrix(InZ);

	return Rx * Ry * Rz;
}

FMatrix FMatrix::operator*(const FMatrix& Other) const
{
	FMatrix Result = {};

	for (int Row = 0; Row < 4; ++Row)
	{
		for (int Col = 0; Col < 4; ++Col)
		{
			Result[Row][Col] =
				m[Row][0] * Other[0][Col] +
				m[Row][1] * Other[1][Col] +
				m[Row][2] * Other[2][Col] +
				m[Row][3] * Other[3][Col];
		}
	}

	return Result;
}

FMatrix FMatrix::ViewMatrix(FVector Eye, FVector Target, FVector Up)
{
	FVector F = (Target - Eye).Normalize();
	FVector R = (Cross(Up, F)).Normalize();
	FVector U = Cross(F, R);

	FMatrix M;

	M.m[0][0] = R.x;
	M.m[0][1] = R.y;
	M.m[0][2] = R.z;
	M.m[0][3] = -Dot(R, Eye);

	M.m[1][0] = U.x;
	M.m[1][1] = U.y;
	M.m[1][2] = U.z;
	M.m[1][3] = -Dot(U, Eye);

	M.m[2][0] = F.x;
	M.m[2][1] = F.y;
	M.m[2][2] = F.z;
	M.m[2][3] = -Dot(F, Eye);

	M.m[3][0] = 0;
	M.m[3][1] = 0;
	M.m[3][2] = 0;
	M.m[3][3] = 1;

	return M;
}