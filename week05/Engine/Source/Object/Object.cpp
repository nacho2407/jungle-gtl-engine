#include "Object/Object.h"
#include "Object/Class.h"

// 조건 1: 전역 오브젝트 배열 정의
TArray<UObject*> GUObjectArray;

// TObjectPtr에서 사용: void*를 통해 forward-declared T의 UUID를 안전하게 추출
uint32_t ExtractUObjectUUID(const void* Ptr)
{
	return Ptr ? static_cast<const UObject*>(Ptr)->UUID : 0;
}

// ─────────────────────────────────────────────────────────────
//  생성 / 소멸
// ─────────────────────────────────────────────────────────────

// 기존 코드베이스와 호환되기 위한 더미 생성자
UObject::UObject(const UClass* InClass, const FString& InName, UObject* InOuter)
	: UObject(InName, InOuter)
{
}

UObject::UObject(const FString& InName, UObject* InOuter)
	: Name(std::move(InName)), Outer(InOuter)
{
	// UUID, InternalIndex는 FObjectFactory::ConstructObject에서 주입
	ObjectSize = LastNewSize;
}

UObject::~UObject() 
{
	// UUID 맵에서 제거
	if (UUID != 0)
	{
		auto It = GUUIDToObjectMap.find(UUID);
		if (It != GUUIDToObjectMap.end() && It->second == this)
		{
			GUUIDToObjectMap.erase(It);
		}
	}

	// 조건 1: 소멸 시 GUObjectArray 슬롯을 nullptr로 마킹
	if (InternalIndex < static_cast<uint32>(GUObjectArray.size()))
	{
		GUObjectArray[static_cast<int32>(InternalIndex)] = nullptr;
	}
}

// ─────────────────────────────────────────────────────────────
//  조건 2: 메모리 통계
// ─────────────────────────────────────────────────────────────

int32 UObject::GetTotalBytes()
{
	return static_cast<int32>(UObject::TotalAllocationBytes);
}

int32 UObject::GetTotalCounts()
{
	return static_cast<int32>(UObject::TotalAllocationCounts);
}

// ─────────────────────────────────────────────────────────────
//  조건 4: RTTI
// ─────────────────────────────────────────────────────────────

namespace
{
	UObject* CreateUObjectInstance(UObject* InOuter, const FString& InName)
	{
		return new UObject(UObject::StaticClass(), InName, InOuter);
	}
}

UClass* UObject::StaticClass()
{
	static UClass ClassInfo("UObject", nullptr, &CreateUObjectInstance);
	return &ClassInfo;
}

UClass* UObject::GetClass() const
{
	return UObject::StaticClass();
}

bool UObject::IsA(const UClass* InClass) const
{
	return InClass && GetClass()->IsChildOf(InClass);
}

// ─────────────────────────────────────────────────────────────
//  오브젝트 정보
// ─────────────────────────────────────────────────────────────

const FString& UObject::GetName() const
{
	return Name;
}

UObject* UObject::GetOuter() const
{
	return Outer;
}

FString UObject::GetPathName() const
{
	if (Outer == nullptr)
	{
		return Name;
	}

	return Outer->GetPathName() + "." + Name;
}

// ─────────────────────────────────────────────────────────────
//  플래그
// ─────────────────────────────────────────────────────────────

bool UObject::HasAnyFlags(EObjectFlags InFlags) const
{
	return static_cast<uint32>(Flags & InFlags) != 0;
}

bool UObject::HasAllFlags(EObjectFlags InFlags) const
{
	return (static_cast<uint32>(Flags & InFlags) == static_cast<uint32>(InFlags));
}

void UObject::AddFlags(EObjectFlags InFlags)
{
	Flags |= InFlags;
}

void UObject::ClearFlags(EObjectFlags InFlags)
{
	Flags = static_cast<EObjectFlags>(static_cast<uint32>(Flags) & ~static_cast<uint32>(InFlags));
}

void UObject::MarkPendingKill()
{
	if (UUID != 0)
	{
		GUUIDToObjectMap.erase(UUID);
	}
	AddFlags(EObjectFlags::PendingKill);
}

bool UObject::IsPendingKill() const
{
	return HasAnyFlags(EObjectFlags::PendingKill);
}