#include "Math/FMatrix.h"
#include <cmath>

const FMatrix FMatrix::Identity = FMatrix::MakeIdentity();

FMatrix::FMatrix()
{
	for (int Row = 0; Row < 4; Row++)
	{
		for (int Col = 0; Col < 4; Col++)
		{
			M[Row][Col] = 0.0f;
		}
	}
}

FMatrix FMatrix::MakeIdentity()
{
	FMatrix Result;

	Result.M[0][0] = 1.0f;
	Result.M[1][1] = 1.0f;
	Result.M[2][2] = 1.0f;
	Result.M[3][3] = 1.0f;

	return Result;
}

FMatrix FMatrix::MakeTranslation(const FVector& Translation)
{
	FMatrix Result = FMatrix::MakeIdentity();

	Result.M[3][0] = Translation.X;
	Result.M[3][1] = Translation.Y;
	Result.M[3][2] = Translation.Z;

	return Result;
}

FMatrix FMatrix::MakeScale(const FVector& Scale)
{
	FMatrix Result = FMatrix::MakeIdentity();

	Result.M[0][0] = Scale.X;
	Result.M[1][1] = Scale.Y;
	Result.M[2][2] = Scale.Z;

	return Result;
}

FMatrix FMatrix::MakeRotationX(float Radians)
{
	FMatrix Result = FMatrix::MakeIdentity();

	const float C = cosf(Radians);
	const float S = sinf(Radians);

	Result.M[1][1] = C;
	Result.M[1][2] = S;
	Result.M[2][1] = -S;
	Result.M[2][2] = C;

	return Result;
}

FMatrix FMatrix::MakeRotationY(float Radians)
{
	FMatrix Result = FMatrix::MakeIdentity();

	const float C = cosf(Radians);
	const float S = sinf(Radians);

	Result.M[0][0] = C;
	Result.M[0][2] = -S;
	Result.M[2][0] = S;
	Result.M[2][2] = C;

	return Result;
}

FMatrix FMatrix::MakeRotationZ(float Radians)
{
	FMatrix Result = FMatrix::MakeIdentity();

	const float C = cosf(Radians);
	const float S = sinf(Radians);

	Result.M[0][0] = C;
	Result.M[0][1] = S;
	Result.M[1][0] = -S;
	Result.M[1][1] = C;

	return Result;
}

FMatrix FMatrix::MakeRotationXYZ(const FVector& RadiansXYZ)
{
	FMatrix RX = MakeRotationX(RadiansXYZ.X);
	FMatrix RY = MakeRotationY(RadiansXYZ.Y);
	FMatrix RZ = MakeRotationZ(RadiansXYZ.Z);

	return RX * RY * RZ;
}

FMatrix FMatrix::MakeViewMatrix(const FVector& CameraLocation, const FVector& CameraRotation)
{
	FMatrix InvTranslation = MakeTranslation(FVector(-CameraLocation.X, -CameraLocation.Y, -CameraLocation.Z));

	FMatrix InvRotZ = MakeRotationZ(-CameraRotation.Z);
	FMatrix InvRotY = MakeRotationY(-CameraRotation.Y);
	FMatrix InvRotX = MakeRotationX(-CameraRotation.X);

	return InvTranslation * InvRotZ * InvRotY * InvRotX;
}

FMatrix FMatrix::MakePerspectiveMatrix(float FOVDegrees, float AspectRatio, float NearClip, float FarClip)
{
	const float FOVRadians = FOVDegrees * 3.14159265358979323846f / 180.0f;
	const float YScale = 1.0f / std::tan(FOVRadians * 0.5f);
	const float XScale = YScale / AspectRatio;

	FMatrix Mat = {};

	Mat.M[0][0] = XScale;
	Mat.M[0][1] = 0.0f;
	Mat.M[0][2] = 0.0f;
	Mat.M[0][3] = 0.0f;

	Mat.M[1][0] = 0.0f;
	Mat.M[1][1] = YScale;
	Mat.M[1][2] = 0.0f;
	Mat.M[1][3] = 0.0f;

	Mat.M[2][0] = 0.0f;
	Mat.M[2][1] = 0.0f;
	Mat.M[2][2] = FarClip / (FarClip - NearClip);
	Mat.M[2][3] = 1.0f;

	Mat.M[3][0] = 0.0f;
	Mat.M[3][1] = 0.0f;
	Mat.M[3][2] = (-NearClip * FarClip) / (FarClip - NearClip);
	Mat.M[3][3] = 0.0f;

	return Mat;
}

FMatrix FMatrix::MakeOrthogonalMatrix(float OrthoWidth, float AspectRatio, float NearClip, float FarClip)
{
	const float OrthoHeight = OrthoWidth / AspectRatio;

	FMatrix Mat = FMatrix::MakeIdentity();

	Mat.M[0][0] = 2.0f / OrthoWidth;
	Mat.M[1][1] = 2.0f / OrthoHeight;
	Mat.M[2][2] = 1.0f / (FarClip - NearClip);
	Mat.M[3][2] = -NearClip / (FarClip - NearClip);

	return Mat;
}

FMatrix FMatrix::operator*(const FMatrix& Rhs) const
{
	FMatrix Result;

	for (int Row = 0; Row < 4; Row++)
	{
		for (int Col = 0; Col < 4; Col++)
		{
			Result.M[Row][Col] =
				M[Row][0] * Rhs.M[0][Col] +
				M[Row][1] * Rhs.M[1][Col] +
				M[Row][2] * Rhs.M[2][Col] +
				M[Row][3] * Rhs.M[3][Col];
		}
	}

	return Result;
}

FVector4 FMatrix::operator*(const FVector4& Vec) const
{
	return FVector4(
		M[0][0] * Vec.X + M[1][0] * Vec.Y + M[2][0] * Vec.Z + M[3][0] * Vec.W,
		M[0][1] * Vec.X + M[1][1] * Vec.Y + M[2][1] * Vec.Z + M[3][1] * Vec.W,
		M[0][2] * Vec.X + M[1][2] * Vec.Y + M[2][2] * Vec.Z + M[3][2] * Vec.W,
		M[0][3] * Vec.X + M[1][3] * Vec.Y + M[2][3] * Vec.Z + M[3][3] * Vec.W
	);
}

FMatrix FMatrix::Transpose(const FMatrix& Mat)
{
	FMatrix Result;

	for (int Row = 0; Row < 4; ++Row)
	{
		for (int Col = 0; Col < 4; ++Col)
		{
			Result.M[Row][Col] = Mat.M[Col][Row];
		}
	}

	return Result;
}

FMatrix FMatrix::InverseAffine(const FMatrix& Mat)
{
	const float A00 = Mat.M[0][0];
	const float A01 = Mat.M[0][1];
	const float A02 = Mat.M[0][2];

	const float A10 = Mat.M[1][0];
	const float A11 = Mat.M[1][1];
	const float A12 = Mat.M[1][2];

	const float A20 = Mat.M[2][0];
	const float A21 = Mat.M[2][1];
	const float A22 = Mat.M[2][2];

	const float Det =
		A00 * (A11 * A22 - A12 * A21)
		- A01 * (A10 * A22 - A12 * A20)
		+ A02 * (A10 * A21 - A11 * A20);

	if (std::fabs(Det) < 0.000001f)
	{
		return FMatrix::Identity;
	}

	const float InvDet = 1.0f / Det;

	FMatrix Inv = FMatrix::MakeIdentity();

	Inv.M[0][0] = (A11 * A22 - A12 * A21) * InvDet;
	Inv.M[0][1] = -(A01 * A22 - A02 * A21) * InvDet;
	Inv.M[0][2] = (A01 * A12 - A02 * A11) * InvDet;

	Inv.M[1][0] = -(A10 * A22 - A12 * A20) * InvDet;
	Inv.M[1][1] = (A00 * A22 - A02 * A20) * InvDet;
	Inv.M[1][2] = -(A00 * A12 - A02 * A10) * InvDet;

	Inv.M[2][0] = (A10 * A21 - A11 * A20) * InvDet;
	Inv.M[2][1] = -(A00 * A21 - A01 * A20) * InvDet;
	Inv.M[2][2] = (A00 * A11 - A01 * A10) * InvDet;

	const float Tx = Mat.M[3][0];
	const float Ty = Mat.M[3][1];
	const float Tz = Mat.M[3][2];

	Inv.M[3][0] = -(Tx * Inv.M[0][0] + Ty * Inv.M[1][0] + Tz * Inv.M[2][0]);
	Inv.M[3][1] = -(Tx * Inv.M[0][1] + Ty * Inv.M[1][1] + Tz * Inv.M[2][1]);
	Inv.M[3][2] = -(Tx * Inv.M[0][2] + Ty * Inv.M[1][2] + Tz * Inv.M[2][2]);

	return Inv;
}

FVector FMatrix::TransformPosition(const FVector& Vec) const
{
	const FVector4 R = (*this) * FVector4(Vec, 1.0f);
	return FVector(R.X, R.Y, R.Z);
}

FVector FMatrix::TransformVector(const FVector& Vec) const
{
	const FVector4 R = (*this) * FVector4(Vec, 0.0f);
	return FVector(R.X, R.Y, R.Z);
}

FVector FMatrix::GetTranslation() const
{
	return FVector(M[3][0], M[3][1], M[3][2]);
}

void FMatrix::SetTranslation(const FVector& Translation)
{
	M[3][0] = Translation.X;
	M[3][1] = Translation.Y;
	M[3][2] = Translation.Z;
}