#pragma once

#include "Core/CoreTypes.h" // TArray가 정의된 곳
#include <type_traits>
#include <string>

// 언리얼 엔진의 핵심 직렬화 베이스 클래스
class FArchive
{
protected:
	bool bIsLoading = false;
	bool bIsSaving = false;

public:
	virtual ~FArchive() = default;

	inline bool IsLoading() const { return bIsLoading; }
	inline bool IsSaving() const { return bIsSaving; }

	// 핵심 순수 가상 함수: 파생 클래스(Writer/Reader)가 구현해야 할 실제 입출력 로직
	// Data 포인터부터 Num 바이트만큼을 읽거나 씁니다.
	virtual void Serialize(void* Data, size_t Num) = 0;

	// ----------------------------------------------------
	// 마법의 연산자 오버로딩 (기본 자료형: int, float, 구조체 등)
	// ----------------------------------------------------
	template<typename T>
	FArchive& operator<<(T& Value)
	{
		// 포인터가 아닌 기본 데이터 타입이나 메모리 복사가 가능한 구조체만 허용
		static_assert(std::is_standard_layout<T>::value, "T must be standard layout");
		this->Serialize(&Value, sizeof(T));
		return *this;
	}
};

inline FArchive& operator<<(FArchive& Ar, FString& Str)
{
	uint32 Length = static_cast<uint32>(Str.size());
	Ar << Length;

	if (Ar.IsLoading()) Str.resize(Length);
	if (Length > 0) Ar.Serialize(Str.data(), Length);

	return Ar;
}

// ----------------------------------------------------
// 🪄 마법의 연산자 특수화 (TArray 지원)
// ----------------------------------------------------
template<typename T>
FArchive& operator<<(FArchive& Ar, TArray<T>& Array)
{
	// 1. 배열의 총 개수(Length)를 먼저 직렬화합니다.
	uint32 ArrayNum = static_cast<uint32>(Array.size());
	Ar << ArrayNum;

	if (Ar.IsLoading()) Array.resize(ArrayNum);
	if (ArrayNum > 0)
	{
		// T가 객체(struct/class)일 경우 내부 멤버의 Serialize를 순회 호출
		if constexpr (!std::is_standard_layout<T>::value)
		{
			for (auto& Item : Array) { Ar << Item; }
		}
		else // 기본 자료형(정점 등)은 통째로 메모리 복사 (엄청난 속도 향상)
		{
			Ar.Serialize(Array.data(), ArrayNum * sizeof(T));
		}
	}

	return Ar;
}