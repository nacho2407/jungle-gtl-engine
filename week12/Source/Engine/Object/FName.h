#pragma once

#include "Core/CoreMinimal.h"
#include "Core/Singleton.h"

// ============================================================
// FName — 문자열 풀 기반 이름 시스템
//
// - 문자열은 전역 테이블에 한 번만 저장, FName은 인덱스만 보관
// - 비교 시 대소문자 무시 (ComparisonIndex 기반)
// - DisplayIndex는 원본 대소문자 유지 (표시용)
//
// 파트 D가 구현, 모든 파트에서 사용
// ============================================================
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

	// 풀에 등록된 고유 문자열(엔트리) 수 반환
	uint32 GetEntryCount() const { return static_cast<uint32>(Entries.size()); }

	// 풀에 등록된 모든 엔트리 목록 반환
	const TArray<FString>& GetEntries() const { return Entries; }

	// 풀이 차지하는 총 문자열 바이트 크기 반환 (각 FString의 문자 데이터 합산)
	size_t GetTotalBytes() const
	{
		size_t Total = 0;
		for (const FString& S : Entries) Total += S.size();
		return Total;
	}

private:
	FNamePool() = default;

	TArray<FString> Entries;
	TMap<FString, uint32> LookupMap;
};

