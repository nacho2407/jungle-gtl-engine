#include "FMatrix.h"

FMatrix4x4 FMatrix4x4::ZeroMatrix()
{
	FMatrix4x4 M = IdentityMatrix();
	M[0][0] = 0;
	M[1][1] = 0;
	M[2][2] = 0;
	M[3][3] = 0;
	return M;
}

FMatrix4x4 FMatrix4x4::TranslationMatrix(float InX, float InY, float InZ)
{
	FMatrix4x4 M = IdentityMatrix();
	M[3][0] = InX;
	M[3][1] = InY;
	M[3][2] = InZ;
	return M;
}

FMatrix4x4 FMatrix4x4::ScaleMatrix(float InX, float InY, float InZ)
{
	FMatrix4x4 M = IdentityMatrix();
	M[0][0] = InX;
	M[1][1] = InY;
	M[2][2] = InZ;
	return M;
}

FMatrix4x4 FMatrix4x4::RotationXMatrix(float InXDegree)
{
	FMatrix4x4 Mat = IdentityMatrix();

	float Rad = DegToRad(InXDegree);
	float C = std::cos(Rad);
	float S = std::sin(Rad);

	Mat[1][1] = C;
	Mat[1][2] = S;
	Mat[2][1] = -S;
	Mat[2][2] = C;

	return Mat;
}

FMatrix4x4 FMatrix4x4::RotationYMatrix(float InYDegree)
{
	FMatrix4x4 Mat = IdentityMatrix();

	float Rad = DegToRad(InYDegree);
	float C = std::cos(Rad);
	float S = std::sin(Rad);

	Mat[0][0] = C;
	Mat[0][2] = -S;
	Mat[2][0] = S;
	Mat[2][2] = C;

	return Mat;
}

FMatrix4x4 FMatrix4x4::RotationZMatrix(float InZDegree)
{
	FMatrix4x4 Mat = IdentityMatrix();

	float Rad = DegToRad(InZDegree);
	float C = std::cos(Rad);
	float S = std::sin(Rad);

	Mat[0][0] = C;
	Mat[0][1] = S;
	Mat[1][0] = -S;
	Mat[1][1] = C;

	return Mat;
}

FMatrix4x4 FMatrix4x4::EulerRotationMatrix(float InX, float InY, float InZ)
{
	FMatrix4x4 Rx = RotationXMatrix(InX);
	FMatrix4x4 Ry = RotationYMatrix(InY);
	FMatrix4x4 Rz = RotationZMatrix(InZ);

	return Rx * Ry * Rz;
}

FMatrix4x4 FMatrix4x4::operator*(const FMatrix4x4& Other) const
{
	FMatrix4x4 Result = {};

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