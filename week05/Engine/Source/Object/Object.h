#pragma once
#include "CoreMinimal.h"
#include "ObjectTypes.h"

template <typename T>
concept HasInitialize = requires(T t) {
	{ t.Initialize() } -> std::same_as<void>;
};

template <typename T>
inline void InitializeIfAble(T* obj) {
	if constexpr (HasInitialize<T>) {
		obj->Initialize();
	}
}

#define DECLARE_RTTI(ClassName, ParentClassName) \
    public: \
        static UClass* StaticClass(); \
        \
        virtual UClass* GetClass() const override \
        { \
            return ClassName::StaticClass(); \
        } \
		ClassName() : ParentClassName("") { \
			InitializeIfAble(this); \
		} \
		ClassName(const FString& InName, UObject* InOuter = nullptr) \
			: ParentClassName(InName, InOuter) \
		{ \
			InitializeIfAble(this); \
		}

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
// 조건 1: 엔진에서 생성되는 모든 UObject를 관리하는 전역 배열
// InternalIndex가 이 배열의 인덱스와 대응됨

extern ENGINE_API 	TArray<UObject*> GUObjectArray;


class ENGINE_API UObject
{
public:
	UObject(const UClass* InClass, const FString& InName, UObject* InOuter = nullptr);
	UObject(const FString& InName, UObject* InOuter = nullptr);
	virtual ~UObject();  // GUObjectArray 슬롯 nullptr 마킹

	// 조건 2: new/delete 오버로딩으로 메모리 통계 추적
	static int32 GetTotalBytes();
	static int32 GetTotalCounts();

	inline static uint32 TotalAllocationBytes = 0;
	inline static uint32 TotalAllocationCounts = 0;
	inline static uint32 LastNewSize = 0; // operator new에서 생성자로 크기 전달용

	// 조건 1: 모든 UObject가 갖는 고유 식별자 (FObjectFactory가 주입)
	uint32 UUID = 0; // 엔진 전체 고유 ID (1-based, 단조 증가)
	uint32 InternalIndex = 0; // GUObjectArray 내 인덱스
	uint32 ObjectSize = 0; // 이 오브젝트의 할당 크기 (bytes)

	// 조건 4: RTTI
	static UClass* StaticClass();
	virtual UClass* GetClass() const;
	const FString& GetName() const;
	UObject* GetOuter() const;

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

	FString GetPathName() const;
	FString GetUUIDString() const
	{
		return std::to_string(UUID);
	}

	bool HasAnyFlags(EObjectFlags InFlags) const;
	bool HasAllFlags(EObjectFlags InFlags) const;
	void AddFlags(EObjectFlags InFlags);
	void ClearFlags(EObjectFlags InFlags);

	void MarkPendingKill();
	bool IsPendingKill() const;

private:
	FString      Name;
	UObject* Outer = nullptr;
	EObjectFlags Flags = EObjectFlags::None;
};

#include "Types/ObjectPtr.h"
