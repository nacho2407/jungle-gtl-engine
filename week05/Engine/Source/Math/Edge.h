#pragma once
#include <cstdint>
#include <algorithm>

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

struct FEdge
{
private:
	uint32_t A;
	uint32_t B;

public:
	FEdge() = default;

	FEdge(uint32_t InA, uint32_t InB)
		: A(std::min(InA, InB))
		, B(std::max(InA, InB))
	{
	}

	uint32_t GetA() const { return A; }
	uint32_t GetB() const { return B; }

	bool operator<(const FEdge& Other) const
	{
		if (A != Other.A) return A < Other.A;
		return B < Other.B;
	}

	bool operator==(const FEdge& Other) const
	{
		return A == Other.A && B == Other.B;
	}

	bool operator!=(const FEdge& Other) const
	{
		return !(*this == Other);
	}
};

template<>
struct std::hash<FEdge>
{
	size_t operator()(const FEdge& E) const noexcept
	{
		const uint64_t Key = (static_cast<uint64_t>(E.GetA()) << 32)
			| static_cast<uint64_t>(E.GetB());
		return std::hash<uint64_t>()(Key);
	}
};