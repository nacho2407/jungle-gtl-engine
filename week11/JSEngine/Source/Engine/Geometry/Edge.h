#pragma once

#include "Core/CoreTypes.h"
#include "Math/Vector.h"
#include <functional>

// 두 FVector 정점으로 구성된 간선(Edge)을 표현하는 자료형
// {A, B}와 {B, A}는 동일한 간선으로 취급됨 (비방향 간선)
struct FEdge
{
public:
	FVector A;
	FVector B;

	//======================================//
	//				constructor				//
	//======================================//
public:
	constexpr FEdge() noexcept : A(), B() {}

	constexpr FEdge(const FVector& InA, const FVector& InB) noexcept
		: A(InA), B(InB)
	{
	}

	FEdge(const FEdge&) noexcept = default;
	FEdge(FEdge&&) noexcept = default;

	//======================================//
	//				operators				//
	//======================================//
public:
	FEdge& operator=(const FEdge&) noexcept = default;
	FEdge& operator=(FEdge&&) noexcept = default;

	// 비방향 간선: {A,B} == {B,A}
	bool operator==(const FEdge& Other) const noexcept
	{
		return (A == Other.A && B == Other.B)
			|| (A == Other.B && B == Other.A);
	}

	bool operator!=(const FEdge& Other) const noexcept
	{
		return !(*this == Other);
	}

	//======================================//
	//				  method				//
	//======================================//
public:
	// 간선의 중간 지점을 반환함
	FVector Midpoint() const noexcept
	{
		return (A + B) * 0.5f;
	}

	// 간선의 길이를 반환함
	float Length() const noexcept
	{
		return FVector::Dist(A, B);
	}

	// 간선의 길이 제곱을 반환함
	float LengthSquared() const noexcept
	{
		return FVector::DistSquared(A, B);
	}

	// 두 정점 중 더 작은 쪽이 A가 되도록 정규화된 복사본을 반환함
	FEdge Canonical() const noexcept
	{
		auto Less = [](const FVector& P, const FVector& Q) -> bool
		{
			if (P.X != Q.X) return P.X < Q.X;
			if (P.Y != Q.Y) return P.Y < Q.Y;
			return P.Z < Q.Z;
		};
		return Less(A, B) ? FEdge(A, B) : FEdge(B, A);
	}
};

namespace std
{
	template <>
	struct hash<FEdge>
	{
		size_t operator()(const FEdge& Edge) const noexcept
		{
			// 비방향 간선이므로 Canonical 형태로 정규화 후 해싱
			FEdge C = Edge.Canonical();
			auto Combine = [](size_t Seed, size_t Value) -> size_t
			{
				return Seed ^ (Value * 2654435761u + 0x9e3779b9u + (Seed << 6) + (Seed >> 2));
			};
			size_t H = std::hash<FVector>{}(C.A);
			H = Combine(H, std::hash<FVector>{}(C.B));
			return H;
		}
	};
}

// 두 정점의 인덱스(Index)로 구성된 간선(Edge)을 표현하는 자료형
// (A, B)와 (B, A)는 동일한 간선으로 취급됨 (비방향 간선)
struct FIndexEdge
{
public:
    uint32 A;
    uint32 B;

    //======================================//
    //                constructor           //
    //======================================//
public:
    constexpr FIndexEdge() noexcept : A(0), B(0) {}

    constexpr FIndexEdge(uint32 InA, uint32 InB) noexcept
        : A(InA), B(InB)
    {
    }

    FIndexEdge(const FIndexEdge&) noexcept = default;
    FIndexEdge(FIndexEdge&&) noexcept = default;

    //======================================//
    //                operators             //
    //======================================//
public:
    FIndexEdge& operator=(const FIndexEdge&) noexcept = default;
    FIndexEdge& operator=(FIndexEdge&&) noexcept = default;

    // 비방향 간선: {A,B} == {B,A}
    bool operator==(const FIndexEdge& Other) const noexcept
    {
        return (A == Other.A && B == Other.B)
            || (A == Other.B && B == Other.A);
    }

    bool operator!=(const FIndexEdge& Other) const noexcept
    {
        return !(*this == Other);
    }

    //======================================//
    //                 method               //
    //======================================//
public:
    // 두 인덱스 중 더 작은 쪽이 A가 되도록 정규화된 복사본을 반환함
    FIndexEdge Canonical() const noexcept
    {
        return A < B ? FIndexEdge(A, B) : FIndexEdge(B, A);
    }
};

namespace std
{
    template <>
    struct hash<FIndexEdge>
    {
        size_t operator()(const FIndexEdge& Edge) const noexcept
        {
            // 비방향 간선이므로 Canonical 형태로 정규화 후 해싱
            FIndexEdge C = Edge.Canonical();
            auto Combine = [](size_t Seed, size_t Value) -> size_t
            {
                return Seed ^ (Value * 2654435761u + 0x9e3779b9u + (Seed << 6) + (Seed >> 2));
            };
            
            size_t H = std::hash<uint32>{}(C.A);
            H = Combine(H, std::hash<uint32>{}(C.B));
            return H;
        }
    };
}