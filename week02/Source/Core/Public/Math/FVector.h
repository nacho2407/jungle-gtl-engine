#pragma once

struct FVector
{
	float X, Y, Z;
public:
	FVector();
	FVector(float InX, float InY, float InZ);

	static const FVector ZeroVector;
	static const FVector OneVector;
	static const FVector UpVector; 
	static const FVector RightVector;
	static const FVector ForwardVector;

public:
	float Dot(const FVector& Rhs) const;
	FVector Cross(const FVector& Rhs) const;

	float Length() const;
	float LengthSquared() const;

	void Normalize();
	FVector GetNormalized() const;

public:
	FVector operator+(const FVector& Rhs) const;
	FVector operator-(const FVector& Rhs) const;
	FVector operator*(float Scalar) const;
	FVector operator/(float Scalar) const;

	FVector& operator+=(const FVector& Rhs);
	FVector& operator-=(const FVector& Rhs);
	FVector& operator*=(float Scalar);
	FVector& operator/=(float Scalar);

	bool operator==(const FVector& Rhs) const;
	bool operator!=(const FVector& Rhs) const;

	//FVector(float _x = 0, float _y = 0, float _z = 0);
	//float Dot(const FVector& rhs);
	//FVector Cross(const FVector& rhs);
	//float Length() const;
	//FVector Normalize();

	//FVector operator+(const float rhs);
	//FVector operator-(const float rhs);
	//FVector operator*(const float rhs);
	//FVector operator/(const float rhs);
	//FVector operator-(const FVector rhs);
};