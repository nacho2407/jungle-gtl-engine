#include "Quat.h"
#include "Rotator.h"
#include "Matrix.h"

const FQuat FQuat::Identity = FQuat(0.0f, 0.0f, 0.0f, 1.0f);

// 오일러(도) → 쿼터니언. 엔진 행렬 순서 Rx*Ry*Rz (row-major) 에 대응: Qz*Qy*Qx
FQuat FQuat::FromRotator(const FRotator& Rot)
{
	float RollRad  = Rot.Roll  * DEG_TO_RAD * 0.5f;
	float PitchRad = Rot.Pitch * DEG_TO_RAD * 0.5f;
	float YawRad   = Rot.Yaw   * DEG_TO_RAD * 0.5f;

	float SR = sinf(RollRad),  CR = cosf(RollRad);
	float SP = sinf(PitchRad), CP = cosf(PitchRad);
	float SY = sinf(YawRad),   CY = cosf(YawRad);

	// Qz(Yaw) * Qy(Pitch) * Qx(Roll)
	FQuat Q;
	Q.X =  SR * CP * CY - CR * SP * SY;
	Q.Y =  CR * SP * CY + SR * CP * SY;
	Q.Z =  CR * CP * SY - SR * SP * CY;
	Q.W =  CR * CP * CY + SR * SP * SY;
	return Q;
}

FRotator FQuat::ToRotator() const
{
	FRotator Rot;
	const float Rad2Deg = RAD_TO_DEG;

	// Pitch (Y축)
	float SinPitch = -2.0f * (X * Z - W * Y);
	SinPitch = Clamp(SinPitch, -1.0f, 1.0f);
	Rot.Pitch = asinf(SinPitch) * Rad2Deg;

	// Yaw (Z축)
	if (fabsf(SinPitch) > 0.9999f)
	{
		Rot.Roll = 0.0f;
		Rot.Yaw = atan2f(-2.0f * (X * Y - W * Z), 1.0f - 2.0f * (Y * Y + Z * Z)) * Rad2Deg;
	}
	else
	{
		Rot.Yaw  = atan2f(2.0f * (X * Y + W * Z), 1.0f - 2.0f * (Y * Y + Z * Z)) * Rad2Deg;
		Rot.Roll = atan2f(2.0f * (Y * Z + W * X), 1.0f - 2.0f * (X * X + Y * Y)) * Rad2Deg;
	}

	return Rot;
}

FMatrix FQuat::ToMatrix() const
{
	float X2 = X * 2.0f, Y2 = Y * 2.0f, Z2 = Z * 2.0f;
	float XX = X * X2, XY = X * Y2, XZ = X * Z2;
	float YY = Y * Y2, YZ = Y * Z2, ZZ = Z * Z2;
	float WX = W * X2, WY = W * Y2, WZ = W * Z2;

	return FMatrix(
		1.0f - (YY + ZZ),  XY + WZ,            XZ - WY,            0.0f,
		XY - WZ,            1.0f - (XX + ZZ),   YZ + WX,            0.0f,
		XZ + WY,            YZ - WX,            1.0f - (XX + YY),   0.0f,
		0.0f,               0.0f,               0.0f,               1.0f
	);
}

FQuat FQuat::FromMatrix(const FMatrix& Mat)
{
	FQuat Q;
	float Trace = Mat.M[0][0] + Mat.M[1][1] + Mat.M[2][2];

	if (Trace > 0.0f)
	{
		float S = sqrtf(Trace + 1.0f) * 2.0f;
		Q.W = 0.25f * S;
		Q.X = (Mat.M[1][2] - Mat.M[2][1]) / S;
		Q.Y = (Mat.M[2][0] - Mat.M[0][2]) / S;
		Q.Z = (Mat.M[0][1] - Mat.M[1][0]) / S;
	}
	else if (Mat.M[0][0] > Mat.M[1][1] && Mat.M[0][0] > Mat.M[2][2])
	{
		float S = sqrtf(1.0f + Mat.M[0][0] - Mat.M[1][1] - Mat.M[2][2]) * 2.0f;
		Q.W = (Mat.M[1][2] - Mat.M[2][1]) / S;
		Q.X = 0.25f * S;
		Q.Y = (Mat.M[0][1] + Mat.M[1][0]) / S;
		Q.Z = (Mat.M[0][2] + Mat.M[2][0]) / S;
	}
	else if (Mat.M[1][1] > Mat.M[2][2])
	{
		float S = sqrtf(1.0f + Mat.M[1][1] - Mat.M[0][0] - Mat.M[2][2]) * 2.0f;
		Q.W = (Mat.M[2][0] - Mat.M[0][2]) / S;
		Q.X = (Mat.M[0][1] + Mat.M[1][0]) / S;
		Q.Y = 0.25f * S;
		Q.Z = (Mat.M[1][2] + Mat.M[2][1]) / S;
	}
	else
	{
		float S = sqrtf(1.0f + Mat.M[2][2] - Mat.M[0][0] - Mat.M[1][1]) * 2.0f;
		Q.W = (Mat.M[0][1] - Mat.M[1][0]) / S;
		Q.X = (Mat.M[0][2] + Mat.M[2][0]) / S;
		Q.Y = (Mat.M[1][2] + Mat.M[2][1]) / S;
		Q.Z = 0.25f * S;
	}

	Q.Normalize();
	return Q;
}
