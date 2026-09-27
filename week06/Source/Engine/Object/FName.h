#pragma once

#include "Core/CoreTypes.h"
#include "Core/Singleton.h"

class FName
{
public:
	FName();
	FName(const char* InName);
	FName(const FString& InName);

	// 비교 (대소문자 무시)
	bool operator==(const FName& Other) const;
	bool operator!=(const FName& Other) const;

	// 해시 지원 (TMap/TSet 키로 사용 가능)
	struct Hash
	{
		size_t operator()(const FName& Name) const;
	};

	// 원본 대소문자 유지된 표시용 문자열 반환
	FString ToString() const;

	// 유효 여부
	bool IsValid() const;

	// None 이름
	static const FName None;

private:
	uint32 ComparisonIndex;	// 소문자 변환된 문자열의 풀 인덱스 (비교용)
	uint32 DisplayIndex;	// 원본 문자열의 풀 인덱스 (표시용)
};

// ============================================================
// FNamePool — 전역 문자열 풀 (싱글턴)
// ============================================================
class FNamePool : public TSingleton<FNamePool>
{
	friend class TSingleton<FNamePool>;

public:
	// 문자열을 풀에 등록하고 인덱스 반환 (이미 있으면 기존 인덱스)
	uint32 Store(const FString& InString);

	// 인덱스로 문자열 조회
	const FString& Resolve(uint32 Index) const;

private:
	FNamePool() = default;

	TArray<FString> Entries;
	TMap<FString, uint32> LookupMap;
};
