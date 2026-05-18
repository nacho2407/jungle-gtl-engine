#pragma once

#include "UClass.h"

#include <cstddef>

// UE 의 TSubclassOf<T> 모방.
//   - 멤버 자체가 "이 슬롯은 어떤 베이스의 자식 UClass" 인지 자기설명.
//   - 대입 시 IsA(T::StaticClass()) 가드 — 잘못된 클래스는 nullptr 로 흡수.
//   - 단일 UClass* 멤버 표준 레이아웃 → reinterpret_cast<UClass**>(ptr) 안전 (Property 시스템 직렬화용).
//   - implicit conversion 으로 기존 UClass* 받는 코드 변경 없이 호환.
template<typename T>
struct TSubclassOf
{
	UClass* Class = nullptr;

	TSubclassOf() = default;
	TSubclassOf(std::nullptr_t) {}
	TSubclassOf(UClass* InClass)            { Assign(InClass); }

	TSubclassOf& operator=(UClass* InClass) { Assign(InClass); return *this; }
	TSubclassOf& operator=(std::nullptr_t)  { Class = nullptr; return *this; }

	UClass*  Get() const                    { return Class; }
	operator UClass*() const                { return Class; }
	UClass*  operator->() const             { return Class; }
	explicit operator bool() const          { return Class != nullptr; }

	// 베이스 추출 — Property 시스템이 자식 enumerate 시 사용.
	static UClass* StaticClass()            { return T::StaticClass(); }

private:
	void Assign(UClass* In)
	{
		Class = (In && In->IsA(T::StaticClass())) ? In : nullptr;
	}
};
