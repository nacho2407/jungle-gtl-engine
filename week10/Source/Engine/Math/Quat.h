#pragma once

#include "Vector.h"
#include "Core/EngineTypes.h"

struct FMatrix;
struct FRotator;

struct FQuat
{
	float X = 0.0f;
	float Y = 0.0f;
	float Z = 0.0f;
	float W = 1.0f;

	// 항등 쿼터니언입니다. 회전이 없는 상태를 나타냅니다.
	static const FQuat Identity;

	//======================================//
	//				constructor				//
	//======================================//
	constexpr FQuat() noexcept = default;

	// 원시 XYZW 성분으로부터 쿼터니언을 생성합니다.
	constexpr FQuat(float InX, float InY, float InZ, float InW) noexcept
		: X(InX), Y(InY), Z(InZ), W(InW)
	{
	}

	// {X, Y, Z, W}를 담고 있는 DirectX 벡터로부터 쿼터니언을 생성합니다.
	explicit FQuat(FXMVector InVector) noexcept;
	// 도 단위의 엔진 로테이터로부터 쿼터니언을 생성합니다.
	explicit FQuat(const FRotator& InRotator) noexcept;
	// 회전 행렬로부터 쿼터니언을 생성합니다. 입력 행렬은 순수 회전 행렬이라고 가정합니다.
	explicit FQuat(const FMatrix& InMatrix) noexcept;
	// 단위 축과 라디안 각도로부터 쿼터니언을 생성합니다.
	FQuat(const FVector& Axis, float AngleRad) noexcept;

	// 엔진 오일러 각도 {Roll, Pitch, Yaw} 도 단위 값으로부터 쿼터니언을 생성합니다.
	static FQuat MakeFromEuler(const FVector& InEulerDegrees) noexcept;
	// 두 쿼터니언 사이의 4차원 내적을 반환합니다.
	static float DotProduct(const FQuat& A, const FQuat& B) noexcept;
	// 최단 호 기준의 구면 선형 보간으로 두 쿼터니언 사이를 보간합니다.
	static FQuat Slerp(const FQuat& A, const FQuat& B, float Alpha) noexcept;

	// 각 성분이 완전히 같은지 반환합니다.
	bool operator==(const FQuat& Other) const noexcept;
	// 각 성분 중 하나라도 다르면 true를 반환합니다.
	bool operator!=(const FQuat& Other) const noexcept;
	// 부호가 반전된 쿼터니언을 반환합니다. -Q는 Q와 같은 회전을 나타냅니다.
	FQuat operator-() const noexcept;
	// 두 쿼터니언을 성분별로 더합니다.
	FQuat operator+(const FQuat& Other) const noexcept;
	// 두 쿼터니언을 성분별로 뺍니다.
	FQuat operator-(const FQuat& Other) const noexcept;
	// 모든 성분에 스칼라를 곱합니다.
	FQuat operator*(float Scale) const noexcept;
	// 모든 성분을 스칼라로 나눕니다.
	FQuat operator/(float Scale) const noexcept;
	// 엔진의 row-vector 규약 기준으로 두 회전을 합성합니다.
	FQuat operator*(const FQuat& Other) const noexcept;
	// 이 쿼터니언으로 벡터를 회전시킵니다.
	FVector operator*(const FVector& InVector) const noexcept;

	FQuat& operator+=(const FQuat& Other) noexcept;
	// 다른 쿼터니언을 성분별로 현재 값에서 뺍니다.
	FQuat& operator-=(const FQuat& Other) noexcept;
	// 모든 성분에 스칼라를 제자리에서 곱합니다.
	FQuat& operator*=(float Scale) noexcept;
	// 모든 성분을 스칼라로 제자리에서 나눕니다.
	FQuat& operator/=(float Scale) noexcept;
	// 다른 회전을 현재 쿼터니언에 제자리에서 합성합니다.
	FQuat& operator*=(const FQuat& Other) noexcept;

	// 다른 쿼터니언과의 4차원 내적을 반환합니다.
	float operator|(const FQuat& Other) const noexcept;

	//======================================//
	//				  method				//
	//======================================//
	// 이 쿼터니언을 {X, Y, Z, W} 형태의 DirectX 벡터로 변환합니다.
	XMVector ToXMVector() const noexcept;
	// 허용 오차 내에서 회전적으로 같은지 반환합니다. Q와 -Q는 같은 회전으로 취급합니다.
	bool Equals(const FQuat& Other, float Tolerance = 1.e-6f) const noexcept;
	// 이 쿼터니언이 항등 회전에 가깝다면 true를 반환합니다.
	bool IsIdentity(float Tolerance = 1.e-6f) const noexcept;
	// 성분 중 하나라도 NaN 또는 무한대이면 true를 반환합니다.
	bool ContainsNaN() const noexcept;
	// 쿼터니언 크기의 제곱을 반환합니다.
	float SizeSquared() const noexcept;
	// 쿼터니언의 크기를 반환합니다.
	float Size() const noexcept;
	// 쿼터니언 크기가 1에 가깝다면 true를 반환합니다.
	bool IsNormalized(float Tolerance = 1.e-4f) const noexcept;
	// 이 쿼터니언을 정규화합니다. 길이가 너무 작으면 항등 쿼터니언이 됩니다.
	void Normalize(float Tolerance = 1.e-8f) noexcept;
	// 정규화된 복사본을 반환합니다. 길이가 너무 작으면 항등 쿼터니언을 반환합니다.
	FQuat GetNormalized(float Tolerance = 1.e-8f) const noexcept;
	// 켤레 쿼터니언 {-X, -Y, -Z, W}를 반환합니다.
	FQuat Conjugate() const noexcept;
	// 역 쿼터니언을 반환합니다. 길이가 너무 작으면 항등 쿼터니언을 반환합니다.
	FQuat Inverse() const noexcept;
	// 이 쿼터니언으로 벡터를 회전시킵니다.
	FVector RotateVector(const FVector& InVector) const noexcept;
	// 이 쿼터니언의 역회전으로 벡터를 회전시킵니다.
	FVector UnrotateVector(const FVector& InVector) const noexcept;
	// 회전 각도를 라디안 단위로 반환합니다.
	float GetAngle() const noexcept;
	// 단위 회전축을 반환합니다. 축이 퇴화한 경우 전방 축으로 대체합니다.
	FVector GetRotationAxis(float Tolerance = 1.e-8f) const noexcept;
	// 엔진 오일러 각도 {Roll, Pitch, Yaw}를 도 단위로 반환합니다.
	FVector Euler() const noexcept;
	// 회전된 로컬 X축을 반환합니다.
	FVector GetAxisX() const noexcept;
	// 회전된 로컬 Y축을 반환합니다.
	FVector GetAxisY() const noexcept;
	// 회전된 로컬 Z축을 반환합니다.
	FVector GetAxisZ() const noexcept;
	// 회전된 전방 벡터(+X)를 반환합니다.
	FVector GetForwardVector() const noexcept;
	// 회전된 오른쪽 벡터(+Y)를 반환합니다.
	FVector GetRightVector() const noexcept;
	// 회전된 위쪽 벡터(+Z)를 반환합니다.
	FVector GetUpVector() const noexcept;
	// 다른 쿼터니언까지의 최소 각거리(rad)를 반환합니다.
	float AngularDistance(const FQuat& Other) const noexcept;
	// Other와의 관계에서 최단 호를 유지하도록 필요하면 부호를 뒤집습니다.
	void EnforceShortestArcWith(const FQuat& Other) noexcept;
	// 이 쿼터니언을 4x4 회전 행렬로 변환합니다.
	FMatrix ToMatrix() const noexcept;

	// 이 쿼터니언을 도 단위의 엔진 로테이터로 변환합니다.
	FRotator Rotator() const noexcept;
};

// 스칼라가 왼쪽에 오는 형태로 모든 성분에 스칼라를 곱합니다.
inline FQuat operator*(float Scale, const FQuat& Quat) noexcept
{
	return Quat * Scale;
}
