#pragma once

#include "Profiling/MemoryStats.h"
#include "Object/FName.h"
#include "Core/Singleton.h"
#include "Core/ClassTypes.h"

class FArchive;

#define DECLARE_CLASS(ClassName, ParentClass)                          \
    static const FTypeInfo s_TypeInfo;								   \
	static FClassRegistrar s_Registrar;								   \
    const FTypeInfo* GetTypeInfo() const override {                    \
        return &s_TypeInfo;                                            \
    }                                                                  

#define DEFINE_CLASS_WITH_FLAGS(ClassName, ParentClass, FlagsValue)    \
    const FTypeInfo ClassName::s_TypeInfo = {                          \
        #ClassName,                                                    \
        &ParentClass::s_TypeInfo,                                      \
        sizeof(ClassName),                                             \
        FlagsValue                                                     \
    };																   \
	FClassRegistrar ClassName::s_Registrar(&ClassName::s_TypeInfo);	   

#define DEFINE_CLASS(ClassName, ParentClass)                           \
    DEFINE_CLASS_WITH_FLAGS(ClassName, ParentClass, CF_None)

enum EClassFlags : uint32_t
{
	CF_None = 0,
	CF_Actor = 1 << 0,
	CF_Component = 1 << 1,
	CF_Camera = 1 << 2,
	// 에디터 Add Component 목록과 런타임 생성 경로에서 모두 제외할 베이스 타입.
	CF_Abstract = 1 << 3,
};

struct FTypeInfo
{
	const char* name;
	const FTypeInfo* Parent;
	size_t size;
	uint32_t ClassFlags = CF_None;

	bool IsA(const FTypeInfo* Other) const {
		for (const FTypeInfo* T = this; T; T = T->Parent) {
			if (T == Other) {
				return true;
			}
		}
		return false;
	}

	bool HasAnyClassFlags(uint32 Flags) const
	{
		return (ClassFlags & Flags) != 0;
	}
};

class UObject
{
public:
	UObject();
	virtual ~UObject();

	uint32 GetUUID() const { return UUID; }
	uint32 GetInternalIndex() const { return InternalIndex; }
	void SetUUID(uint32 InUUID) { UUID = InUUID; }
	void SetInternalIndex(uint32 InIndex) { InternalIndex = InIndex; }

	// Outer — 객체의 논리적 스코프 (소유 의미 아님). 직렬화 제외.
	UObject* GetOuter() const { return Outer; }
	void SetOuter(UObject* InOuter) { Outer = InOuter; }

	// Outer 체인을 따라 첫 번째 T를 찾는다 (UE의 GetTypedOuter<T>와 동일 시맨틱).
	template<typename T>
	T* GetTypedOuter() const
	{
		for (UObject* O = Outer; O; O = O->Outer)
		{
			if (T* Hit = Cast<T>(O))
			{
				return Hit;
			}
		}
		return nullptr;
	}

	virtual UObject* Duplicate(UObject* NewOuter = nullptr) const;
	virtual void Serialize(FArchive& Ar);                 // 있으면 활용, 없으면 도입
	virtual void PostDuplicate() {}

	static void* operator new(size_t Size)
	{
		void* Ptr = std::malloc(Size);
		if (Ptr)
		{
			MemoryStats::OnAllocated(static_cast<uint32>(Size));
		}
		return Ptr;
	}

	static void operator delete(void* Ptr, size_t Size)
	{
		if (Ptr)
		{
			MemoryStats::OnDeallocated(static_cast<uint32>(Size));
			std::free(Ptr);
		}
	}

	// FName
	FName GetFName() const { return ObjectName; }
	void SetFName(const FName& InName) { ObjectName = InName; }

	// RTTI stuffs
	virtual const FTypeInfo* GetTypeInfo() const { return &s_TypeInfo; }

	template<typename T>
	bool IsA() const { return GetTypeInfo()->IsA(&T::s_TypeInfo); }


	static const FTypeInfo s_TypeInfo;

protected:
	FName ObjectName;

private:
	uint32 UUID;
	uint32 InternalIndex;
	UObject* Outer = nullptr;
};

extern TArray<UObject*> GUObjectArray;

class UObjectManager : public TSingleton<UObjectManager>
{
	friend class TSingleton<UObjectManager>;

public:
	template<typename T>
	T* CreateObject(UObject* InOuter = nullptr)
	{
		static_assert(std::is_base_of<UObject, T>::value, "T must derive from UObject");
		T* Obj = new T();
		Obj->SetOuter(InOuter);

		const char* ClassName = T::s_TypeInfo.name;
		uint32& Counter = NameCounters[ClassName];
		FString Name = FString(ClassName) + "_" + std::to_string(Counter++);
		Obj->SetFName(FName(Name));

		return Obj;
	}

	void DestroyObject(UObject* Obj)
	{
		if (!Obj)
		{
			return;
		}
		delete Obj;
	}

private:
	TMap<FString, uint32> NameCounters;

public:
	UObject* FindByUUID(uint32 InUUID)
	{
		for (auto* Obj : GUObjectArray)
			if (Obj && Obj->GetUUID() == InUUID)
				return Obj;
		return nullptr;
	}

	UObject* FindByIndex(uint32 Index)
	{
		if (Index >= GUObjectArray.size()) return nullptr;
		return GUObjectArray[Index];
	}
};

template<typename T>
T* Cast(UObject* Obj)
{
	return (Obj && Obj->IsA<T>()) ? static_cast<T*>(Obj) : nullptr;
}

template<typename T>
const T* Cast(const UObject* Obj)
{
	return (Obj && Obj->IsA<T>()) ? static_cast<const T*>(Obj) : nullptr;
}