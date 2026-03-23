#pragma once

#include "Core/EngineAPI.h"
#include "MathUtility.h"

struct ENGINE_API FVector4
{
  public:
  public:
    union
    {
        struct
        {
            float X;
            float Y;
            float Z;
            float W;
        };

        float XYZW[4];
    };

    static constexpr FVector4 Zero() { return {0.0f, 0.0f, 0.0f, 0.0f}; }
    static constexpr FVector4 Up() { return {0.0f, 0.0f, 1.0f, 0.0f}; }
    static constexpr FVector4 Right() { return {0.0f, 1.0f, 0.0f, 0.0f}; }
    static constexpr FVector4 Forward() { return {1.0f, 0.0f, 0.0f, 0.0f}; }
    static constexpr FVector4 Point() { return {0.0f, 0.0f, 0.0f, 1.0f}; }

    //======================================//
    //				constructor				//
    //======================================//
    constexpr FVector4() noexcept : X(0.f), Y(0.f), Z(0.f), W(0.f) {}

    constexpr FVector4(const float InX, const float InY, const float InZ, const float InW) noexcept
        : X(InX), Y(InY), Z(InZ), W(InW)
    {
    }

    explicit FVector4(const DirectX::XMFLOAT4& InFloat4) noexcept
        : X(InFloat4.x), Y(InFloat4.y), Z(InFloat4.z), W(InFloat4.w)
    {
    }

    explicit FVector4(DirectX::FXMVECTOR InVector) noexcept
    {
        DirectX::XMFLOAT4 Temp;
        DirectX::XMStoreFloat4(&Temp, InVector);
        X = Temp.x;
        Y = Temp.y;
        Z = Temp.z;
        W = Temp.w;
    }

    FVector4(const FVector4&) noexcept = default;
    FVector4(FVector4&&) noexcept = default;
    ~FVector4() = default;

    //======================================//
    //				operators				//
    //======================================//
    FVector4& operator=(const FVector4&) noexcept = default;
    FVector4& operator=(FVector4&&) noexcept = default;

    float& operator[](int32_t Index) noexcept
    {
        assert(Index >= 0 && Index < 4);
        return XYZW[Index];
    }

    const float& operator[](int32_t Index) const noexcept
    {
        assert(Index >= 0 && Index < 4);
        return XYZW[Index];
    }

    constexpr bool operator==(const FVector4& Other) const noexcept
    {
        return X == Other.X && Y == Other.Y && Z == Other.Z && W == Other.W;
    }

    constexpr bool operator!=(const FVector4& Other) const noexcept { return !(*this == Other); }

    constexpr FVector4 operator-() const noexcept { return {-X, -Y, -Z, W}; }

    constexpr FVector4 operator+(const FVector4& Other) const noexcept
    {
        assert(abs(W - 1.0f) > FMath::Epsilon || abs(Other.W - 1.0f) > FMath::Epsilon);
        return {X + Other.X, Y + Other.Y, Z + Other.Z, W + Other.W};
    }

    constexpr FVector4 operator-(const FVector4& Other) const noexcept
    {
        return {X - Other.X, Y - Other.Y, Z - Other.Z, W};
    }

    constexpr FVector4 operator*(float Scalar) const noexcept
    {
        return {X * Scalar, Y * Scalar, Z * Scalar, W};
    }

    constexpr FVector4 operator/(float Scalar) const noexcept
    {
        assert(Scalar != 0.f);
        return {X / Scalar, Y / Scalar, Z / Scalar, W};
    }

    FVector4& operator+=(const FVector4& Other) noexcept
    {
        X += Other.X;
        Y += Other.Y;
        Z += Other.Z;
        return *this;
    }

    FVector4& operator-=(const FVector4& Other) noexcept
    {
        X -= Other.X;
        Y -= Other.Y;
        Z -= Other.Z;
        return *this;
    }

    FVector4& operator*=(float Scalar) noexcept
    {
        X *= Scalar;
        Y *= Scalar;
        Z *= Scalar;
        return *this;
    }

    FVector4& operator/=(float Scalar) noexcept
    {
        assert(Scalar != 0.f);
        X /= Scalar;
        Y /= Scalar;
        Z /= Scalar;
        return *this;
    }

    [[nodiscard]] float Dot(const FVector4& Other) const
    {
        return X * Other.X + Y * Other.Y + Z * Other.Z;
    }

    [[nodiscard]] FVector4 Cross(const FVector4& Other) const
    {
        return {Y * Other.Z - Z * Other.Y, Z * Other.X - X * Other.Z, X * Other.Y - Y * Other.X , 0.f};
    }

    [[nodiscard]] FVector4 Normalize() const;
    [[nodiscard]] float    Length() const;

    [[nodiscard]] bool IsNearlyEqual(const FVector4& Other) const;
    bool               operator==(const FVector4& Other) const;

    [[nodiscard]] bool IsPoint() const;
    [[nodiscard]] bool IsVector() const;

    constexpr FVector4 operator*(FMatrix Mat) const noexcept
    {
        FVector4 V;

        for (int32_t Col = 0; Col < 4; ++Col)
        {
            V[Col] = X * Mat[0][Col] + Y * Mat[1][Col] + Z * Mat[2][Col] + W * Mat[3][Col];
        }
        return V;
    }
};
