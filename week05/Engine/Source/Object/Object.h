#pragma once
#include "CoreMinimal.h"
#include "ObjectTypes.h"

#define DECLARE_RTTI(ClassName, ParentClassName) \
    public: \
        static UClass* StaticClass(); \
        \
        virtual UClass* GetClass() const override \
        { \
            return ClassName::StaticClass(); \
        } \
		ClassName() : ParentClassName("") {} \
		ClassName(const FString& InName, UObject* InOuter = nullptr) \
			: ParentClassName(InName, InOuter) {}

#define IMPLEMENT_RTTI(ClassName, ParentClassName) \
    namespace { \
        UObject* Create##ClassName##Instance(UObject* InOuter, const FString& InName) { \
             return new ClassName(InName, InOuter); \
        } \
        struct FAutoRegister_##ClassName { \
            FAutoRegister_##ClassName() { ClassName::StaticClass(); } \
        } GAutoRegister_##ClassName; \
    } \
    UClass* ClassName::StaticClass() { \
        static UClass ClassInfo = { \
            #ClassName, \
            ParentClassName::StaticClass(), \
            Create##ClassName##Instance \
        }; \
        return &ClassInfo; \
    }

class UClass;
class UObject;

struct ENGINE_API FDuplicateContext
{
	EObjectFlags DuplicateFlags = EObjectFlags::None;
	TMap<const void*, UObject*> DuplicatedObjects;

	// DuplicatedObject에서 Source를 찾는다.
	template <typename T>
	T* FindDuplicate(const T* Source) const
	{
		if (!Source)
		{
			return nullptr;
		}

		auto It = DuplicatedObjects.find(static_cast<const void*>(Source));
		if (It == DuplicatedObjects.end())
		{
			return nullptr;
		}

		return reinterpret_cast<T*>(It->second);
	}

	// Source를 key로, Duplicate를 value로 DuplicatedObjects에 등록한다.
	void Register(const UObject* Source, UObject* Duplicate)
	{
		if (Source && Duplicate)
		{
			DuplicatedObjects[static_cast<const void*>(Source)] = Duplicate;
		}
	}
};

// 엔진에서 생성한 모든 UObject를 추적하는 전역 배열이다.
// 각 객체의 InternalIndex는 이 배열 안에서 자신의 슬롯을 가리킨다.
extern ENGINE_API TArray<UObject*> GUObjectArray;

class ENGINE_API UObject
{
public:
	/** 클래스 메타데이터를 직접 넘겨 생성하는 생성자다. 주로 RTTI/팩토리 경로에서 사용된다. */
	UObject(const UClass* InClass, const FString& InName, UObject* InOuter = nullptr);
	/** 이름과 Outer만으로 객체를 생성한다. UUID 등 식별 정보는 팩토리에서 뒤이어 주입된다. */
	UObject(const FString& InName, UObject* InOuter = nullptr);
	/** 객체가 파괴될 때 전역 추적 배열과 UUID 맵에서 자신을 정리한다. */
	virtual ~UObject();

	/** 현재까지 UObject 계열이 할당한 총 바이트 수를 반환한다. */
	static int32 GetTotalBytes();
	/** 현재까지 UObject 계열이 할당한 총 객체 수를 반환한다. */
	static int32 GetTotalCounts();

	inline static uint32 TotalAllocationBytes = 0;
	inline static uint32 TotalAllocationCounts = 0;
	inline static uint32 LastNewSize = 0; // operator new에서 마지막으로 요청된 크기를 임시로 보관한다.

	// 모든 UObject는 팩토리에서 부여한 고유 식별 정보를 가진다.
	uint32 UUID = 0; // 엔진 전체에서 유일한 1-based 식별자다.
	uint32 InternalIndex = 0; // GUObjectArray에서 자신의 위치를 가리킨다.
	uint32 ObjectSize = 0; // 이 객체 인스턴스가 실제로 차지하는 메모리 크기다.

	/** 이 타입의 클래스 메타데이터를 반환한다. */
	static UClass* StaticClass();
	/** 실제 런타임 타입의 클래스 메타데이터를 반환한다. */
	virtual UClass* GetClass() const;
	/** 객체 이름만 반환한다. Outer 경로는 포함하지 않는다. */
	const FString& GetName() const;
	/** 자신을 소유하는 상위 객체를 반환한다. */
	UObject* GetOuter() const;
	/** 팩토리 생성 직후 추가 초기화가 필요할 때 파생 클래스가 오버라이드한다. */
	virtual void PostConstruct();

	/** 이 객체가 지정한 클래스이거나 그 하위 클래스인지 검사한다. */
	bool IsA(const UClass* InClass) const;

	template <typename T>
	bool IsA() const
	{
		static_assert(std::is_base_of_v<UObject, T>, "T must derive from UObject");
		return IsA(T::StaticClass());
	}

	template <typename T>
	T* GetTypedOuter() const
	{
		static_assert(std::is_base_of_v<UObject, T>, "T must derive from UObject");

		// Outer 체인을 따라 올라가며 원하는 타입의 소유자를 찾는다.
		UObject* Current = Outer;
		while (Current)
		{
			if (Current->IsA(T::StaticClass()))
			{
				return static_cast<T*>(Current);
			}
			Current = Current->GetOuter();
		}
		return nullptr;
	}

	/** Outer 경로를 포함한 전체 경로명을 반환한다. */
	FString GetPathName() const;
	/** UUID를 사람이 읽기 쉬운 문자열 형태로 반환한다. */
	FString GetUUIDString() const
	{
		return std::to_string(UUID);
	}

	/** 지정한 플래그 중 하나라도 켜져 있는지 검사한다. */
	bool HasAnyFlags(EObjectFlags InFlags) const;
	/** 지정한 플래그가 모두 켜져 있는지 검사한다. */
	bool HasAllFlags(EObjectFlags InFlags) const;
	/** 객체 플래그를 추가한다. */
	void AddFlags(EObjectFlags InFlags);
	/** 객체 플래그를 제거한다. */
	void ClearFlags(EObjectFlags InFlags);

	/** 즉시 삭제하지 않고 GC 대상임을 표시한다. */
	void MarkPendingKill();
	/** 이 객체가 삭제 예정 상태인지 확인한다. */
	bool IsPendingKill() const;

	// TODO: 아래 함수들을 모든 자식 클래스마다 일일히 선언하는건 너무 번거로울 것 같음.
	// 하지만 인터페이스 제공 목적으로 우선 첨부함.
	// 서브 오브젝트를 복제하는 함수 (하위 클래스에서 재정의 가능)
	virtual void DuplicateShallow(UObject* DuplicatedObject, FDuplicateContext& Context) const {}
	virtual void DuplicateSubObjects(UObject* DuplicatedObject, FDuplicateContext& Context) const {}
	virtual void FixupDuplicatedReferences(UObject* DuplicatedObject, const FDuplicateContext& Context) const {}
	virtual void PostDuplicate(UObject* DuplicatedObject, const FDuplicateContext& Context) const {}
	virtual UObject* Duplicate(UObject* InOuter, const FString& InName, FDuplicateContext& Context) const;

private:
	FString			Name;
	UObject*		Outer = nullptr;
	EObjectFlags	Flags = EObjectFlags::None;
};

#include "Types/ObjectPtr.h"
