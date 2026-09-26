#pragma once
#include "EngineAPI.h"
#include "Types/Map.h"
#include <cstdint>
#include <type_traits>

class UObject;

// UUID -> UObject* 역방향 조회 맵 (ObjectFactory.cpp에서 정의)
extern ENGINE_API TMap<uint32_t, UObject*> GUUIDToObjectMap;

// Forward-declaration-safe UUID extraction (defined in Object.cpp)
// void*를 사용하여 T가 불완전 타입이어도 동작
ENGINE_API uint32_t ExtractUObjectUUID(const void* Ptr);

// UUID 기반 간접 참조 스마트 포인터
// GC에 의해 raw pointer가 무효화되어도 UUID를 통해 안전하게 접근 가능
template <typename T>
class TObjectPtr
{
public:
	TObjectPtr() = default;

	TObjectPtr(std::nullptr_t)
		: ObjectUUID(0), CachedPtr(nullptr)
	{
	}

	TObjectPtr(T* InPtr)
		: ObjectUUID(ExtractUObjectUUID(InPtr)), CachedPtr(InPtr)
	{
	}

	TObjectPtr(const TObjectPtr& Other) = default;

	TObjectPtr(TObjectPtr&& Other) noexcept
		: ObjectUUID(Other.ObjectUUID), CachedPtr(Other.CachedPtr)
	{
		Other.ObjectUUID = 0;
		Other.CachedPtr = nullptr;
	}

	// 업캐스트 지원: TObjectPtr<Derived> -> TObjectPtr<Base>
	template <typename U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
	TObjectPtr(const TObjectPtr<U>& Other)
		: ObjectUUID(Other.GetUUID()), CachedPtr(Other.GetCachedPtr())
	{
	}

	TObjectPtr& operator=(std::nullptr_t)
	{
		ObjectUUID = 0;
		CachedPtr = nullptr;
		return *this;
	}

	TObjectPtr& operator=(T* InPtr)
	{
		ObjectUUID = ExtractUObjectUUID(InPtr);
		CachedPtr = InPtr;
		return *this;
	}

	TObjectPtr& operator=(const TObjectPtr& Other) = default;

	TObjectPtr& operator=(TObjectPtr&& Other) noexcept
	{
		if (this != &Other)
		{
			ObjectUUID = Other.ObjectUUID;
			CachedPtr = Other.CachedPtr;
			Other.ObjectUUID = 0;
			Other.CachedPtr = nullptr;
		}
		return *this;
	}

	// 업캐스트 대입
	template <typename U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
	TObjectPtr& operator=(const TObjectPtr<U>& Other)
	{
		ObjectUUID = Other.GetUUID();
		CachedPtr = Other.GetCachedPtr();
		return *this;
	}

	// 핵심: UUID 기반 안전한 포인터 반환
	// UUID 맵을 통해 검증 후 CachedPtr 반환 (delete된 객체 접근 방지)
	// T가 불완전 타입이어도 동작 (static_cast 불필요)
	T* Get() const
	{
		if (ObjectUUID == 0)
		{
			CachedPtr = nullptr;
			return nullptr;
		}

		auto It = GUUIDToObjectMap.find(ObjectUUID);
		if (It == GUUIDToObjectMap.end() || It->second == nullptr)
		{
			CachedPtr = nullptr;
			return nullptr;
		}

		if (It->second->IsPendingKill())
		{
			CachedPtr = nullptr;
			return nullptr;
		}

		// UUID 맵으로 객체 생존 확인 완료. CachedPtr은 생성/대입 시 올바르게 설정됨.
		// 단조 증가 UUID이므로 재사용 없음 → CachedPtr은 항상 유효.
		return CachedPtr;
	}

	T* operator->() const { return Get(); }
	T& operator*() const { return *Get(); }

	// raw pointer로 암시적 변환 — 비교 연산도 이 변환을 통해 처리됨
	operator T*() const { return Get(); }

	explicit operator bool() const { return Get() != nullptr; }

	uint32_t GetUUID() const { return ObjectUUID; }
	T* GetCachedPtr() const { return CachedPtr; }

	bool IsValid() const { return Get() != nullptr; }
	bool IsNull() const { return ObjectUUID == 0; }
	bool IsPending() const
	{
		if (ObjectUUID == 0) return false;
		auto It = GUUIDToObjectMap.find(ObjectUUID);
		return It != GUUIDToObjectMap.end() && It->second && It->second->IsPendingKill();
	}

	void Reset()
	{
		ObjectUUID = 0;
		CachedPtr = nullptr;
	}

private:
	uint32_t ObjectUUID = 0;
	mutable T* CachedPtr = nullptr;
};
