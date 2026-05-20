#pragma once

#include "Core/CoreTypes.h" // TArray가 정의된 곳
#include "Object/Object.h"
#include "Object/FName.h"
#include "Math/Rotator.h"
#include "Math/Vector.h"
#include <type_traits>
#include <string>
#include <functional>

class UObject;

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
	virtual bool IsObjectReferenceRemapping() const { return false; }
	virtual UObject* ResolveObjectReference(uint32 /*SourceUUID*/) const { return nullptr; }
	virtual void AddObjectReferenceFixup(uint32 /*SourceUUID*/, std::function<void(UObject*)> /*Fixup*/) {}
	virtual bool UsesCustomObjectReferenceSerialization() const { return false; }
	virtual void SerializeObjectReference(UObject*& Object);

	virtual void BeginObject() {}
	virtual void EndObject() {}
	virtual bool HasProperty(const char* /*Name*/) const { return true; }
	virtual void BeginProperty(const char* /*Name*/) {}
	virtual void EndProperty() {}
	virtual bool BeginArray(uint32& /*Num*/) { return false; }
	virtual void BeginArrayElement(uint32 /*Index*/) {}
	virtual void EndArrayElement() {}
	virtual void EndArray() {}

	// 핵심 순수 가상 함수: 파생 클래스(Writer/Reader)가 구현해야 할 실제 입출력 로직
	// Data 포인터부터 Num 바이트만큼을 읽거나 씁니다.
	virtual void Serialize(void* Data, size_t Num) = 0;

	virtual void SerializeBool(bool& Value) { Serialize(&Value, sizeof(Value)); }
	virtual void SerializeInt32(int32& Value) { Serialize(&Value, sizeof(Value)); }
	virtual void SerializeUInt32(uint32& Value) { Serialize(&Value, sizeof(Value)); }
	virtual void SerializeFloat(float& Value) { Serialize(&Value, sizeof(Value)); }
	virtual void SerializeString(FString& Str);
	virtual void SerializeName(FName& Name);
	virtual void SerializeVector(FVector& Value) { Serialize(Value.Data, sizeof(float) * 3); }
	virtual void SerializeVector4(FVector4& Value) { Serialize(Value.Data, sizeof(float) * 4); }
	virtual void SerializeRotator(FRotator& Value) { Serialize(&Value, sizeof(Value)); }

	FArchive& operator<<(bool& Value) { SerializeBool(Value); return *this; }
	FArchive& operator<<(int32& Value) { SerializeInt32(Value); return *this; }
	FArchive& operator<<(uint32& Value) { SerializeUInt32(Value); return *this; }
	FArchive& operator<<(float& Value) { SerializeFloat(Value); return *this; }
	FArchive& operator<<(FString& Value) { SerializeString(Value); return *this; }
	FArchive& operator<<(FName& Value) { SerializeName(Value); return *this; }
	FArchive& operator<<(FVector& Value) { SerializeVector(Value); return *this; }
	FArchive& operator<<(FVector4& Value) { SerializeVector4(Value); return *this; }
	FArchive& operator<<(FRotator& Value) { SerializeRotator(Value); return *this; }

	// ----------------------------------------------------
	// 마법의 연산자 오버로딩 (기본 자료형: int, float, 구조체 등)
	// ----------------------------------------------------
	template<typename T>
	FArchive& operator<<(T& Value)
	{
		// 포인터가 아닌 기본 데이터 타입이나 메모리 복사가 가능한 구조체만 허용
		static_assert(std::is_trivially_copyable<T>::value, "Error: T is not trivially copyable! You need a custom operator<< for this type.");
		this->Serialize(&Value, sizeof(T));
		return *this;
	}
};

inline void FArchive::SerializeString(FString& Str)
{
	uint32 Length = static_cast<uint32>(Str.size());
	*this << Length;

	if (IsLoading()) Str.resize(Length);
	if (Length > 0) Serialize(Str.data(), Length * sizeof(char));
}

// FName은 풀 인덱스가 프로세스/환경마다 다르므로 문자열로 왕복.
inline void FArchive::SerializeName(FName& Name)
{
	FString Str = IsSaving() ? Name.ToString() : FString();
	*this << Str;
	if (IsLoading())
	{
		Name = FName(Str);
	}
}

inline void FArchive::SerializeObjectReference(UObject*& Object)
{
	uint32 UUID = 0;
	if (IsSaving())
	{
		UUID = Object ? Object->GetUUID() : 0;
	}

	*this << UUID;

	if (IsLoading())
	{
		UObject* ResolvedObject = ResolveObjectReference(UUID);
		Object = ResolvedObject ? ResolvedObject : (UUID != 0 ? UObjectManager::Get().FindByUUID(UUID) : nullptr);
	}
}

// ----------------------------------------------------
// 마법의 연산자 특수화 (TArray 지원)
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
		// FNormalVertex처럼 완벽한 숫자 덩어리일 때만 O(1) 고속 복사를 수행합니다.
		if constexpr (std::is_trivially_copyable<T>::value)
		{
			Ar.Serialize(Array.data(), ArrayNum * sizeof(T));
		}
		else
		{
			// FStaticMeshSection처럼 안에 FString이 들어있으면,
			// 느리더라도 반드시 한 요소씩 돌면서 안전하게(Deep Copy) 직렬화합니다.
			for (auto& Item : Array)
			{
				Ar << Item;
			}
		}
	}

	return Ar;
}
