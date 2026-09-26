#pragma once

#include "EngineAPI.h"
#include "Math/Vector.h"
#include "Math/Vector4.h"

enum class EAxis : uint8_t
{
	X, Y, Z
};

struct ENGINE_API FMatrix
{
public:
	alignas(16) float M[4][4];

	static const FMatrix Identity;

	//======================================//
	//				constructor				//
	//======================================//
public:
	constexpr FMatrix() noexcept
		: M{ { 1.f, 0.f, 0.f, 0.f },
			{ 0.f, 1.f, 0.f, 0.f },
			{ 0.f, 0.f, 1.f, 0.f },
			{ 0.f, 0.f, 0.f, 1.f } }
	{
	}

	constexpr FMatrix(
		float M00, float M01, float M02, float M03,
		float M10, float M11, float M12, float M13,
		float M20, float M21, float M22, float M23,
		float M30, float M31, float M32, float M33) noexcept
		: M{ { M00, M01, M02, M03 },
			{ M10, M11, M12, M13 },
			{ M20, M21, M22, M23 },
			{ M30, M31, M32, M33 } }
	{
	}

	constexpr FMatrix(
		const FVector4& Row0,
		const FVector4& Row1,
		const FVector4& Row2,
		const FVector4& Row3) noexcept
		: M{ { Row0.X, Row0.Y, Row0.Z, Row0.W },
			{ Row1.X, Row1.Y, Row1.Z, Row1.W },
			{ Row2.X, Row2.Y, Row2.Z, Row2.W },
			{ Row3.X, Row3.Y, Row3.Z, Row3.W } }
	{
	}

	explicit FMatrix(CXMMatrix InMatrix) noexcept
	{
		Float4X4 Temp;
		DirectX::XMStoreFloat4x4(&Temp, InMatrix);

		M[0][0] = Temp._11; M[0][1] = Temp._12; M[0][2] = Temp._13; M[0][3] = Temp._14;
		M[1][0] = Temp._21; M[1][1] = Temp._22; M[1][2] = Temp._23; M[1][3] = Temp._24;
		M[2][0] = Temp._31; M[2][1] = Temp._32; M[2][2] = Temp._33; M[2][3] = Temp._34;
		M[3][0] = Temp._41; M[3][1] = Temp._42; M[3][2] = Temp._43; M[3][3] = Temp._44;
	}

	FMatrix(const FMatrix&) noexcept = default;
	FMatrix(FMatrix&&) noexcept = default;
	FMatrix& operator=(const FMatrix&) noexcept = default;
	FMatrix& operator=(FMatrix&&) noexcept = default;

public:
	float* operator[](int32 Row) noexcept
	{
		assert(Row >= 0 && Row < 4);
		return M[Row];
	}
	const float* operator[](int32 Row) const noexcept
	{
		assert(Row >= 0 && Row < 4);
		return M[Row];
	}

	// operator==는 부동소수점 정확 비교입니다.
	// 계산 결과 비교에는 Equals(Tolerance)를 사용하는 것을 권장합니다.
	bool operator==(const FMatrix& Other) const noexcept
	{
		for (int32 Row = 0; Row < 4; ++Row)
		{
			const XMVector ThisRow = DirectX::XMVectorSet(M[Row][0], M[Row][1], M[Row][2], M[Row][3]);
			const XMVector OtherRow = DirectX::XMVectorSet(Other.M[Row][0], Other.M[Row][1], Other.M[Row][2], Other.M[Row][3]);
			if (!DirectX::XMVector4Equal(ThisRow, OtherRow))
			{
				return false;
			}
		}
		return true;
	}

	bool operator!=(const FMatrix& Other) const noexcept
	{
		return !(*this == Other);
	}

	FMatrix operator-() const noexcept
	{
		return FMatrix(
			-M[0][0], -M[0][1], -M[0][2], -M[0][3],
			-M[1][0], -M[1][1], -M[1][2], -M[1][3],
			-M[2][0], -M[2][1], -M[2][2], -M[2][3],
			-M[3][0], -M[3][1], -M[3][2], -M[3][3]
		);
	}

	FMatrix operator+(const FMatrix& Other) const noexcept
	{
		FMatrix Result;
		for (int32_t Row = 0; Row < 4; ++Row)
		{
			for (int32_t Col = 0; Col < 4; ++Col)
			{
				Result.M[Row][Col] = M[Row][Col] + Other.M[Row][Col];
			}
		}
		return Result;
	}

	FMatrix operator-(const FMatrix& Other) const noexcept
	{
		FMatrix Result;
		for (int32_t Row = 0; Row < 4; ++Row)
		{
			for (int32_t Col = 0; Col < 4; ++Col)
			{
				Result.M[Row][Col] = M[Row][Col] - Other.M[Row][Col];
			}
		}
		return Result;
	}

	FMatrix operator*(float Scalar) const noexcept
	{
		FMatrix Result;
		for (int32_t Row = 0; Row < 4; ++Row)
		{
			for (int32_t Col = 0; Col < 4; ++Col)
			{
				Result.M[Row][Col] = M[Row][Col] * Scalar;
			}
		}
		return Result;
	}

	FMatrix operator/(float Scalar) const noexcept
	{
		assert(Scalar != 0.f);

		FMatrix Result;
		for (int32_t Row = 0; Row < 4; ++Row)
		{
			for (int32_t Col = 0; Col < 4; ++Col)
			{
				Result.M[Row][Col] = M[Row][Col] / Scalar;
			}
		}
		return Result;
	}

	FMatrix& operator+=(const FMatrix& Other) noexcept
	{
		for (int32_t Row = 0; Row < 4; ++Row)
		{
			for (int32_t Col = 0; Col < 4; ++Col)
			{
				M[Row][Col] += Other.M[Row][Col];
			}
		}
		return *this;
	}

	FMatrix& operator-=(const FMatrix& Other) noexcept
	{
		for (int32_t Row = 0; Row < 4; ++Row)
		{
			for (int32_t Col = 0; Col < 4; ++Col)
			{
				M[Row][Col] -= Other.M[Row][Col];
			}
		}
		return *this;
	}

	FMatrix& operator*=(float Scalar) noexcept
	{
		for (int32_t Row = 0; Row < 4; ++Row)
		{
			for (int32_t Col = 0; Col < 4; ++Col)
			{
				M[Row][Col] *= Scalar;
			}
		}
		return *this;
	}

	FMatrix& operator/=(float Scalar) noexcept
	{
		assert(Scalar != 0.f);

		for (int32_t Row = 0; Row < 4; ++Row)
		{
			for (int32_t Col = 0; Col < 4; ++Col)
			{
				M[Row][Col] /= Scalar;
			}
		}
		return *this;
	}

	FMatrix operator*(const FMatrix& Other) const noexcept
	{
		return FMatrix(DirectX::XMMatrixMultiply(ToXMMatrix(), Other.ToXMMatrix()));
	}

	FMatrix& operator*=(const FMatrix& Other) noexcept
	{
		*this = FMatrix(DirectX::XMMatrixMultiply(ToXMMatrix(), Other.ToXMMatrix()));
		return *this;
	}

	//======================================//
	//				  method				//
	//======================================//
public:
	XMMatrix ToXMMatrix() const noexcept
	{
		return DirectX::XMMATRIX(
			M[0][0], M[0][1], M[0][2], M[0][3],
			M[1][0], M[1][1], M[1][2], M[1][3],
			M[2][0], M[2][1], M[2][2], M[2][3],
			M[3][0], M[3][1], M[3][2], M[3][3]
		);
	}

	// 두 행렬이 허용 오차(Tolerance) 범위 내에서 같은지 비교함
	bool Equals(const FMatrix& Other, float Tolerance = 1.e-6f) const noexcept
	{
		const XMVector ToleranceVector = DirectX::XMVectorReplicate(Tolerance);
		for (int32_t Row = 0; Row < 4; ++Row)
		{
			const XMVector ThisRow = DirectX::XMVectorSet(M[Row][0], M[Row][1], M[Row][2], M[Row][3]);
			const XMVector OtherRow = DirectX::XMVectorSet(Other.M[Row][0], Other.M[Row][1], Other.M[Row][2], Other.M[Row][3]);
			if (!DirectX::XMVector4NearEqual(ThisRow, OtherRow, ToleranceVector))
			{
				return false;
			}
		}
		return true;
	}

	// 전치 행렬(Transpose Matrix)을 반환함
	FMatrix GetTransposed() const noexcept
	{
		return FMatrix(DirectX::XMMatrixTranspose(ToXMMatrix()));
	}

	// 방향 벡터를 현재 행렬로 변환함
	// 이동(Translation)은 적용하지 않음
	FVector TransformVector(const FVector& V) const noexcept
	{
		return FVector(DirectX::XMVector3TransformNormal(V.ToXMVector(), ToXMMatrix()));
	}

	// 위치 벡터를 현재 행렬로 변환함
	// 이동(Translation)을 포함하여 적용함
	FVector TransformPosition(const FVector& V) const noexcept
	{
		return FVector(DirectX::XMVector3TransformCoord(V.ToXMVector(), ToXMMatrix()));
	}

	// 현재 행렬의 이동(Translation) 성분을 반환함
	FVector GetOrigin() const noexcept
	{
		return FVector(M[3][0], M[3][1], M[3][2]);
	}

	// 현재 행렬의 이동(Translation) 성분을 설정함
	void SetOrigin(const FVector& Origin) noexcept
	{
		M[3][0] = Origin.X;
		M[3][1] = Origin.Y;
		M[3][2] = Origin.Z;
	}

	// 현재 행렬에서 스케일이 포함된 축 벡터를 반환함
	FVector GetScaledAxis(EAxis Axis) const noexcept
	{
		switch (Axis)
		{
		case EAxis::X:
			return FVector(M[0][0], M[0][1], M[0][2]);
		case EAxis::Y:
			return FVector(M[1][0], M[1][1], M[1][2]);
		case EAxis::Z:
			return FVector(M[2][0], M[2][1], M[2][2]);
		default:
			return FVector::ZeroVector;
		}
	}

	// 현재 행렬에서 정규화된 축 벡터를 반환함
	FVector GetUnitAxis(EAxis Axis) const noexcept
	{
		return GetScaledAxis(Axis).GetSafeNormal();
	}

	// 현재 행렬에서 이동(Translation) 성분을 제거함
	void RemoveTranslation() noexcept
	{
		M[3][0] = 0.f;
		M[3][1] = 0.f;
		M[3][2] = 0.f;
	}

	// 이동(Translation) 성분이 제거된 새 행렬을 반환함
	FMatrix GetMatrixWithoutTranslation() const noexcept
	{
		FMatrix Result = *this;
		Result.RemoveTranslation();
		return Result;
	}

	// 현재 행렬에서 스케일을 제거한 새 행렬을 반환함
	FMatrix GetMatrixWithoutScale(float Tolerance = 1.e-8f) const noexcept
	{
		const FVector XAxis = GetScaledAxis(EAxis::X).GetSafeNormal(Tolerance);
		const FVector YAxis = GetScaledAxis(EAxis::Y).GetSafeNormal(Tolerance);
		const FVector ZAxis = GetScaledAxis(EAxis::Z).GetSafeNormal(Tolerance);

		FMatrix Result = *this;
		Result.M[0][0] = XAxis.X; Result.M[0][1] = XAxis.Y; Result.M[0][2] = XAxis.Z;
		Result.M[1][0] = YAxis.X; Result.M[1][1] = YAxis.Y; Result.M[1][2] = YAxis.Z;
		Result.M[2][0] = ZAxis.X; Result.M[2][1] = ZAxis.Y; Result.M[2][2] = ZAxis.Z;

		return Result;
	}

	// 현재 행렬에 포함된 스케일 값을 반환함
	FVector GetScaleVector() const noexcept
	{
		const XMVector XAxis = DirectX::XMVectorSet(M[0][0], M[0][1], M[0][2], 0.0f);
		const XMVector YAxis = DirectX::XMVectorSet(M[1][0], M[1][1], M[1][2], 0.0f);
		const XMVector ZAxis = DirectX::XMVectorSet(M[2][0], M[2][1], M[2][2], 0.0f);

		return FVector(
			DirectX::XMVectorGetX(DirectX::XMVector3Length(XAxis)),
			DirectX::XMVectorGetX(DirectX::XMVector3Length(YAxis)),
			DirectX::XMVectorGetX(DirectX::XMVector3Length(ZAxis))
		);
	}

	// 현재 행렬이 Identity Matrix와 같은지 확인함
	bool IsIdentity(float Tolerance = 1.e-6f) const noexcept
	{
		return Equals(Identity, Tolerance);
	}

	// 현재 행렬의 행렬식(Determinant)을 구함
	// Determinant가 0이면 역행렬  없음
	// 0에 매우 가까우면 수치적으로 불안정
	// Inverse 전에 판단할 때 중요한 함수
	float Determinant() const noexcept
	{
		const DirectX::XMMATRIX XM = ToXMMatrix();
		const DirectX::XMVECTOR Det = DirectX::XMMatrixDeterminant(XM);
		return DirectX::XMVectorGetX(Det);
	}

	// 현재 행렬의 역행렬(Inverse Matrix)을 반환함
	// 역행렬이 존재하지 않으면 Identity를 반환함
	// 현재는 역행렬이 없을 때 Identity를 반환/대입하는 정책입니다.
	// 디버깅 투명성을 높이려면 원본 유지 + false 반환 정책도 고려할 수 있습니다.
	FMatrix GetInverse(float Tolerance = 1.e-8f) const noexcept
	{
		const DirectX::XMMATRIX XM = ToXMMatrix();

		DirectX::XMVECTOR Det;
		const DirectX::XMMATRIX Inv = DirectX::XMMatrixInverse(&Det, XM);

		const float DeterminantValue = DirectX::XMVectorGetX(Det);
		if (std::fabs(DeterminantValue) <= Tolerance)
		{
			// 역행렬이 없을 때 원본으로 반환하는 정책으로 변경 가능
#ifndef NDEBUG
			assert("FMatrix::GetInverse() failed: matrix is singular or invalid.");
#endif
			return Identity;
		}

		return FMatrix(Inv);
	}

	// 현재 행렬을 역행렬로 변환함
	// 역행렬이 존재하지 않으면 Identity로 설정함
	// 현재는 역행렬이 없을 때 Identity를 반환/대입하는 정책입니다.
	// 디버깅 투명성을 높이려면 원본 유지 + false 반환 정책도 고려할 수 있습니다.
	[[nodiscard]] bool Inverse(float Tolerance = 1.e-8f) noexcept
	{
		const DirectX::XMMATRIX XM = ToXMMatrix();

		DirectX::XMVECTOR Det;
		const DirectX::XMMATRIX Inv = DirectX::XMMatrixInverse(&Det, XM);

		const float DeterminantValue = DirectX::XMVectorGetX(Det);
		if (std::fabs(DeterminantValue) <= Tolerance)
		{
			// 역행렬이 없을 때 원본으로 반환하는 정책으로 변경 가능
			*this = Identity;
			return false;
		}

		*this = FMatrix(Inv);
		return true;
	}

	// 현재 행렬이 역행렬을 가질 수 있는지 확인함
	bool IsInvertible(float Tolerance = 1.e-8f) const noexcept
	{
		return std::fabs(Determinant()) > Tolerance;
	}

	// 현재 행렬에 스케일을 적용한 새 행렬을 반환함
	FMatrix ApplyScale(const FVector& Scale) const noexcept
	{
		return MakeScale(Scale) * *this;
	}

	// 현재 행렬에 균일 스케일을 적용한 새 행렬을 반환함
	FMatrix ApplyScale(float Scale) const noexcept
	{
		return ApplyScale(FVector(Scale, Scale, Scale));
	}

	// 현재 행렬에서 순수 회전 행렬을 반환함
	FMatrix GetRotationMatrix(float Tolerance = 1.e-8f) const noexcept
	{
		return GetMatrixWithoutTranslation().GetMatrixWithoutScale(Tolerance);
	}

	// 현재 행렬의 Forward 방향 벡터를 반환함
	FVector GetForwardVector() const noexcept
	{
		return GetUnitAxis(EAxis::X);
	}

	// 현재 행렬의 Right 방향 벡터를 반환함
	FVector GetRightVector() const noexcept
	{
		return GetUnitAxis(EAxis::Y);
	}

	// 현재 행렬의 Up 방향 벡터를 반환함
	FVector GetUpVector() const noexcept
	{
		return GetUnitAxis(EAxis::Z);
	}

	// 축 벡터와 위치를 이용하여 현재 행렬을 설정함
	void SetAxes(
		const FVector& XAxis,
		const FVector& YAxis,
		const FVector& ZAxis,
		const FVector& Origin = FVector::ZeroVector) noexcept
	{
		M[0][0] = XAxis.X; M[0][1] = XAxis.Y; M[0][2] = XAxis.Z; M[0][3] = 0.f;
		M[1][0] = YAxis.X; M[1][1] = YAxis.Y; M[1][2] = YAxis.Z; M[1][3] = 0.f;
		M[2][0] = ZAxis.X; M[2][1] = ZAxis.Y; M[2][2] = ZAxis.Z; M[2][3] = 0.f;
		M[3][0] = Origin.X; M[3][1] = Origin.Y; M[3][2] = Origin.Z; M[3][3] = 1.f;
	}

	// 현재 행렬을 위치, 회전 행렬, 스케일로 분해함
	// 분해에 실패하면 false를 반환함
	bool Decompose(FVector& OutTranslation, FMatrix& OutRotation, FVector& OutScale, float Tolerance = 1.e-8f) const noexcept
	{
		// 1차 버전의 함수
		// 다음 경우까지 완벽하게 다루지 않음
		// - negative scale
		// - reflection
		// - shear
		// - 축이 서로 완전히 직교하지 않은 행렬
		// 수학적으로 보든 4x4 행렬을 완벽하게 분해하는 구현은 아님.
		OutTranslation = GetOrigin();

		const FVector XAxis = GetScaledAxis(EAxis::X);
		const FVector YAxis = GetScaledAxis(EAxis::Y);
		const FVector ZAxis = GetScaledAxis(EAxis::Z);

		OutScale = FVector(XAxis.Size(), YAxis.Size(), ZAxis.Size());

		if (OutScale.X <= Tolerance || OutScale.Y <= Tolerance || OutScale.Z <= Tolerance)
		{
			OutRotation = Identity;
			return false;
		}

		const FVector UnitX = XAxis / OutScale.X;
		const FVector UnitY = YAxis / OutScale.Y;
		const FVector UnitZ = ZAxis / OutScale.Z;

		OutRotation = Identity;
		OutRotation.SetAxes(UnitX, UnitY, UnitZ, FVector::ZeroVector);

		return true;
	}

	// 현재 행렬의 위치(Translation) 성분을 반환함
	FVector GetTranslation() const noexcept
	{
		return GetOrigin();
	}

	// 현재 행렬의 위치(Translation) 성분을 설정함
	void SetTranslation(const FVector& Translation) noexcept
	{
		SetOrigin(Translation);
	}

	// 이동(Translation) 행렬을 생성함
	static FMatrix MakeTranslation(const FVector& Translation) noexcept
	{
		return FMatrix(
			1.f, 0.f, 0.f, 0.f,
			0.f, 1.f, 0.f, 0.f,
			0.f, 0.f, 1.f, 0.f,
			Translation.X, Translation.Y, Translation.Z, 1.f
		);
	}

	// 스케일(Scale) 행렬을 생성함
	static FMatrix MakeScale(const FVector& Scale) noexcept
	{
		return FMatrix(
			Scale.X, 0.f, 0.f, 0.f,
			0.f, Scale.Y, 0.f, 0.f,
			0.f, 0.f, Scale.Z, 0.f,
			0.f, 0.f, 0.f, 1.f
		);
	}

	// X축 기준 회전 행렬을 생성함
	// AngleRad는 라디안 단위 각도임
	static FMatrix MakeRotationX(float AngleRad) noexcept
	{
		const float CosAngle = std::cos(AngleRad);
		const float SinAngle = std::sin(AngleRad);

		return FMatrix(
			1.f, 0.f, 0.f, 0.f,
			0.f, CosAngle, SinAngle, 0.f,
			0.f, -SinAngle, CosAngle, 0.f,
			0.f, 0.f, 0.f, 1.f
		);
	}

	// Y축 기준 회전 행렬을 생성함
	// AngleRad는 라디안 단위 각도임
	static FMatrix MakeRotationY(float AngleRad) noexcept
	{
		const float CosAngle = std::cos(AngleRad);
		const float SinAngle = std::sin(AngleRad);

		return FMatrix(
			CosAngle, 0.f, -SinAngle, 0.f,
			0.f, 1.f, 0.f, 0.f,
			SinAngle, 0.f, CosAngle, 0.f,
			0.f, 0.f, 0.f, 1.f
		);
	}

	// Z축 방향 벡터를 기준으로 직교 기저 행렬을 생성함
	// 이 방향을 Z축(Up/Normal)으로 사용하는 함수임
	static FMatrix MakeRotationZ(float AngleRad) noexcept
	{
		const float CosAngle = std::cos(AngleRad);
		const float SinAngle = std::sin(AngleRad);

		return FMatrix(
			CosAngle, SinAngle, 0.f, 0.f,
			-SinAngle, CosAngle, 0.f, 0.f,
			0.f, 0.f, 1.f, 0.f,
			0.f, 0.f, 0.f, 1.f
		);
	}

	// 단일 스칼라 값으로 균일 스케일 행렬을 생성함
	static FMatrix MakeScale(float Scale) noexcept
	{
		return MakeScale(FVector(Scale, Scale, Scale));
	}

	// X축 방향 벡터를 기준으로 직교 기저 행렬을 생성함
	// 이 방향이 앞으로 향하는 X축이다. 라고 정하는 함수임
	static FMatrix MakeFromX(const FVector& XAxis) noexcept
	{
		const FVector X = XAxis.GetSafeNormal();
		if (X.IsNearlyZero())
		{
			return Identity;
		}

		const FVector UpCandidate =
			(std::fabs(X.Z) < 0.999f) ? FVector::UpVector : FVector::RightVector;

		const FVector Y = FVector::CrossProduct(UpCandidate, X).GetSafeNormal();
		const FVector Z = FVector::CrossProduct(X, Y).GetSafeNormal();

		return FMatrix(
			X.X, X.Y, X.Z, 0.f,
			Y.X, Y.Y, Y.Z, 0.f,
			Z.X, Z.Y, Z.Z, 0.f,
			0.f, 0.f, 0.f, 1.f
		);
	}

	// Y축 방향 벡터를 기준으로 직교 기저 행렬을 생성함
	// 이 방향이 앞으로 향하는 Y축이다. 라고 정하는 함수임
	static FMatrix MakeFromY(const FVector& YAxis) noexcept
	{
		const FVector Y = YAxis.GetSafeNormal();
		if (Y.IsNearlyZero())
		{
			return Identity;
		}

		const FVector UpCandidate =
			(std::fabs(Y.Z) < 0.999f) ? FVector::UpVector : FVector::ForwardVector;

		const FVector X = FVector::CrossProduct(Y, UpCandidate).GetSafeNormal();
		const FVector Z = FVector::CrossProduct(X, Y).GetSafeNormal();

		return FMatrix(
			X.X, X.Y, X.Z, 0.f,
			Y.X, Y.Y, Y.Z, 0.f,
			Z.X, Z.Y, Z.Z, 0.f,
			0.f, 0.f, 0.f, 1.f
		);
	}

	// Z축 방향 벡터를 기준으로 직교 기저 행렬을 생성함
	// 이 방향이 앞으로 향하는 Z축이다. 라고 정하는 함수임
	static FMatrix MakeFromZ(const FVector& ZAxis) noexcept
	{
		const FVector Z = ZAxis.GetSafeNormal();
		if (Z.IsNearlyZero())
		{
			return Identity;
		}

		const FVector ForwardCandidate =
			(std::fabs(Z.X) < 0.999f) ? FVector::ForwardVector : FVector::RightVector;

		const FVector Y = FVector::CrossProduct(Z, ForwardCandidate).GetSafeNormal();
		const FVector X = FVector::CrossProduct(Y, Z).GetSafeNormal();

		return FMatrix(
			X.X, X.Y, X.Z, 0.f,
			Y.X, Y.Y, Y.Z, 0.f,
			Z.X, Z.Y, Z.Z, 0.f,
			0.f, 0.f, 0.f, 1.f
		);
	}

	// Eye 위치에서 Target 위치를 바라보는 LookAt 행렬을 생성함
	static FMatrix MakeLookAt(const FVector& Eye, const FVector& Target, const FVector& Up = FVector::UpVector) noexcept
	{
		const FVector Forward = (Target - Eye).GetSafeNormal();
		if (Forward.IsNearlyZero())
		{
			return Identity;
		}

		const FVector Right = FVector::CrossProduct(Up, Forward).GetSafeNormal();
		if (Right.IsNearlyZero())
		{
			return Identity;
		}

		const FVector NewUp = FVector::CrossProduct(Forward, Right).GetSafeNormal();

		return FMatrix(
			Forward.X, Forward.Y, Forward.Z, 0.f,
			Right.X, Right.Y, Right.Z, 0.f,
			NewUp.X, NewUp.Y, NewUp.Z, 0.f,
			Eye.X, Eye.Y, Eye.Z, 1.f
		);
	}

	// Left-Handed 기준 원근 투영 행렬을 생성함
	static FMatrix MakePerspectiveFovLH(float FovYRad, float AspectRatio, float NearZ, float FarZ) noexcept
	{
		assert(AspectRatio != 0.f);
		assert(FarZ != NearZ);

		const float YScale = 1.0f / std::tan(FovYRad * 0.5f);
		const float XScale = YScale / AspectRatio;

		return FMatrix(
			0.f, 0.f, FarZ / (FarZ - NearZ), 1.f,
			XScale, 0.f, 0.f, 0.f,
			0.f, YScale, 0.f, 0.f,
			0.f, 0.f, -NearZ * FarZ / (FarZ - NearZ), 0.f
		);
	}

	// Left-Handed 기준 직교 투영 행렬을 생성함
	static FMatrix MakeOrthographicLH(float ViewWidth, float ViewHeight, float NearZ, float FarZ) noexcept
	{
		assert(ViewWidth != 0.f);
		assert(ViewHeight != 0.f);
		assert(FarZ != NearZ);

		return FMatrix(
			0.f, 0.f, 1.f / (FarZ - NearZ), 0.f,
			2.f / ViewWidth, 0.f, 0.f, 0.f,
			0.f, 2.f / ViewHeight, 0.f, 0.f,
			0.f, 0.f, -NearZ / (FarZ - NearZ), 1.f
		);
	}

	// Left-Handed 기준 View LookAt 행렬을 생성함
	static FMatrix MakeViewLookAtLH(const FVector& Eye, const FVector& Target, const FVector& Up = FVector::UpVector) noexcept
	{
		const FVector Forward = (Target - Eye).GetSafeNormal();
		if (Forward.IsNearlyZero())
		{
			return Identity;
		}

		const FVector Right = FVector::CrossProduct(Up, Forward).GetSafeNormal();
		if (Right.IsNearlyZero())
		{
			return Identity;
		}

		const FVector NewUp = FVector::CrossProduct(Forward, Right).GetSafeNormal();

		return FMatrix(
			Forward.X, Right.X, NewUp.X, 0.f,
			Forward.Y, Right.Y, NewUp.Y, 0.f,
			Forward.Z, Right.Z, NewUp.Z, 0.f,
			-FVector::DotProduct(Eye, Forward),
			-FVector::DotProduct(Eye, Right),
			-FVector::DotProduct(Eye, NewUp),
			1.f
		);
	}

	// 지정한 위치에서 카메라를 바라보는 Billboard 행렬을 생성함
	static FMatrix MakeBillboard(const FVector& Position, const FVector& CameraPosition, const FVector& Up = FVector::UpVector) noexcept
	{
		const FVector Forward = (CameraPosition - Position).GetSafeNormal();
		if (Forward.IsNearlyZero())
		{
			return MakeTranslation(Position);
		}

		const FVector Right = FVector::CrossProduct(Up, Forward).GetSafeNormal();
		if (Right.IsNearlyZero())
		{
			return MakeTranslation(Position);
		}

		const FVector NewUp = FVector::CrossProduct(Forward, Right).GetSafeNormal();
		return FMatrix(
			Forward.X, Forward.Y, Forward.Z, 0.f,
			-Right.X, -Right.Y, -Right.Z, 0.f,
			NewUp.X, NewUp.Y, NewUp.Z, 0.f,
			Position.X, Position.Y, Position.Z, 1.f
		);
	}

	// 위치, 회전 행렬, 스케일을 이용하여 월드 행렬을 생성함
	// TODO : FRotator, FQuat를 만들면 수정해야함.
	static FMatrix MakeWorld(const FVector& Translation, const FMatrix& RotationMatrix, const FVector& Scale) noexcept
	{
		FMatrix Result = RotationMatrix;

		Result.M[0][0] *= Scale.X;
		Result.M[0][1] *= Scale.X;
		Result.M[0][2] *= Scale.X;

		Result.M[1][0] *= Scale.Y;
		Result.M[1][1] *= Scale.Y;
		Result.M[1][2] *= Scale.Y;

		Result.M[2][0] *= Scale.Z;
		Result.M[2][1] *= Scale.Z;
		Result.M[2][2] *= Scale.Z;

		Result.M[3][0] = Translation.X;
		Result.M[3][1] = Translation.Y;
		Result.M[3][2] = Translation.Z;
		Result.M[3][3] = 1.f;

		return Result;
	}

	// 위치(Translation), 회전(Rotation), 스케일(Scale)로 행렬을 생성함
	static FMatrix MakeTRS(const FVector& Translation, const FMatrix& RotationMatrix, const FVector& Scale) noexcept
	{
		return MakeWorld(Translation, RotationMatrix, Scale);
	}

	static FMatrix Abs(const FMatrix& InMatrix) noexcept
	{
		FMatrix Result = InMatrix;

		for (int i = 0; i < 4; i++)
		{
			for (int j = 0; j < 4; j++)
			{
				Result[i][j] = abs(Result[i][j]);
			}
		}

		return Result;
	}
};

inline FMatrix operator*(float Scalar, const FMatrix& Matrix) noexcept
{
	return Matrix * Scalar;
}
