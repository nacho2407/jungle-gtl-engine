#pragma once

#include "Profiling/MemoryStats.h"
#include "Object/FName.h"
#include "Core/Singleton.h"
#include "Core/PropertyTypes.h"
#include "Object/UClass.h"

class FArchive;

// ---------------------------------------------------------------------------
// RTTI Macros
// ---------------------------------------------------------------------------

#define DECLARE_CLASS(ClassName, ParentClass)                               \
    using Super = ParentClass;                                             \
    static UClass StaticClassInstance;                                      \
    static FClassRegistrar s_Registrar;                                    \
    static UClass* StaticClass() { return &StaticClassInstance; }           \
    UClass* GetClass() const override { return StaticClass(); }

#define DEFINE_CLASS_WITH_FLAGS(ClassName, ParentClass, FlagsValue)         \
    UClass ClassName::StaticClassInstance(                                  \
        #ClassName,                                                        \
        &ParentClass::StaticClassInstance,                                  \
        sizeof(ClassName),                                                 \
        FlagsValue                                                         \
    );                                                                     \
    FClassRegistrar ClassName::s_Registrar(&ClassName::StaticClassInstance);

#define DEFINE_CLASS(ClassName, ParentClass)                                \
    DEFINE_CLASS_WITH_FLAGS(ClassName, ParentClass, CF_None)

// ---------------------------------------------------------------------------

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
	virtual void Serialize(FArchive& Ar);
	virtual void PostDuplicate() {}

	virtual void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps);
	virtual void PostEditProperty(const char* PropertyName);

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
	FString GetName() const { return ObjectName.ToString(); }
	void SetFName(const FName& InName) { ObjectName = InName; }

	// RTTI
	virtual UClass* GetClass() const { return StaticClass(); }

	template<typename T>
	bool IsA() const { return GetClass()->IsA(T::StaticClass()); }

	static UClass StaticClassInstance;
	static UClass* StaticClass() { return &StaticClassInstance; }

protected:
	FName ObjectName;

private:
	uint32 UUID;
	uint32 InternalIndex;
	UObject* Outer = nullptr;
};

extern TArray<UObject*> GUObjectArray;
// 살아있는 UObject 포인터를 O(1) 로 조회하기 위한 set. UObject ctor/dtor 가 자동 유지.
// dangling pointer 도 hash 만 계산하므로(deref 없음) 안전.
extern TSet<UObject*> GUObjectSet;

// 포인터가 현재 살아있는 UObject 를 가리키는지 확인. dangling/freed 포인터가 들어와도
// 해시 테이블 조회만 하므로 deref 안 함 — 안전.
inline bool IsValid(const UObject* Object)
{
	return Object && GUObjectSet.find(const_cast<UObject*>(Object)) != GUObjectSet.end();
}

inline bool IsAliveObject(const UObject* Object)
{
	return IsValid(Object);
}

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

		const char* ClassName = T::StaticClass()->GetName();
		uint32& Counter = NameCounters[ClassName];
		FString Name = FString(ClassName) + "_" + std::to_string(Counter++);
		Obj->SetFName(FName(Name));

		return Obj;
	}

	// 즉시 destroy — 호출자가 lifetime 을 안전하게 보장한 경우에만 사용 (예: ~AActor 가
	// 자기 컴포넌트들을 정리하는 destructor 흐름, 단발성 transient material 해제 등).
	// gameplay 코드가 액터를 destroy 하려는 경우엔 World::DestroyActor 를 통해 MarkPendingKill
	// 경로를 타야 함 — 그 쪽은 frame 끝에 일괄 delete.
	void DestroyObject(UObject* Obj)
	{
		if (!Obj) return;
		delete Obj;
	}

	// PendingKill 큐에 추가만 — 실제 delete 는 UEngine::Tick 끝의 FlushPendingKill 시점.
	// PhysX onContact 콜백 / TickManager 순회 / 자기 callback 안에서의 self-destroy 등
	// "delete this" 가 호출 stack 위의 코드를 dangling 으로 만드는 사고를 방지하는 용도.
	void MarkPendingKill(UObject* Obj)
	{
		if (!Obj) return;
		if (std::find(PendingKill.begin(), PendingKill.end(), Obj) != PendingKill.end()) return;
		PendingKill.push_back(Obj);
	}

	bool IsPendingKill(const UObject* Obj) const
	{
		return Obj && std::find(PendingKill.begin(), PendingKill.end(), Obj) != PendingKill.end();
	}

	// 매 frame UEngine::Tick 끝에서 호출 — 중간 작업이 새 PendingKill 을 추가할 수도 있어
	// (예: Actor 소멸 시 ~AActor 가 RemoveComponent → DestroyObject) swap-and-process 패턴으로
	// cascade 처리. 처리 도중 vector 가 reallocate 돼도 안전.
	void FlushPendingKill()
	{
		while (!PendingKill.empty())
		{
			TArray<UObject*> Local;
			std::swap(Local, PendingKill);
			for (UObject* Obj : Local)
			{
				if (Obj) delete Obj;
			}
		}
	}

private:
	TMap<FString, uint32> NameCounters;
	TArray<UObject*> PendingKill;

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
