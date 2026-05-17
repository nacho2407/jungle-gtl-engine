#pragma once

#include "EngineStatics.h"
#include "Object/FName.h"
#include "Core/Singleton.h"
#include "Core/Reflection/ReflectionMacros.h"
#include "Serialization/Archive.h"
#include "Object/Class.h"
#include <type_traits>

class UClass;



enum EClassFlags : uint32_t
{
	CF_None = 0,
	CF_Actor = 1 << 0,
	CF_Component = 1 << 1,
	CF_Camera = 1 << 2,
	CF_Abstract = 1 << 3,
};

class UObject
{
public:
	UObject();
	virtual ~UObject();

	// -----------------------------------------------------------------------
	// 복제 시스템
	// Duplicate()     : UClass/CreateFunc로 같은 타입의 인스턴스를 생성한 뒤
	//                   CopyPropertiesFrom → PostDuplicate 순으로 호출합니다.
	//                   개별 클래스에서 오버라이드할 필요 없습니다.
	// PostDuplicate() : Duplicate() 내부에서 CopyPropertiesFrom 직후 호출되는 가상 훅.
	//                   프로퍼티 시스템에 노출되지 않은 필드 복사, 포인터 재연결 등
	//                   클래스별 후처리를 이곳에 구현합니다.
	//                   하위 클래스 구현 시 부모의 PostDuplicate 를 먼저 호출해야 합니다.
	// -----------------------------------------------------------------------
	virtual UObject* Duplicate();
	virtual void PostDuplicate(UObject* Original) 
	{
		ObjectName = Original->ObjectName;
	}

	uint32 GetUUID() const { return UUID; }
	uint32 GetInternalIndex() const { return InternalIndex; }
	void SetUUID(uint32 InUUID) { UUID = InUUID; }
	void SetInternalIndex(uint32 InIndex) { InternalIndex = InIndex; }

	// FName
	FName GetFName() const { return ObjectName; }
	void SetFName(const FName& InName) { ObjectName = InName; }

	struct FObjectNameProxy : public FString
	{
		using FString::FString;
		FObjectNameProxy(const FString& InStr) : FString(InStr) {}
		const char* operator*() const { return c_str(); }
	};

	FObjectNameProxy GetName() const { return FObjectNameProxy(ObjectName.ToString()); }

	// RTTI stuffs
	static UClass* StaticClass();
	virtual UClass* GetClass() const { return StaticClass(); }
	const char* GetClassName() const
	{
		UClass* Class = GetClass();
		return Class ? Class->GetName() : "UObject";
	}

	bool IsA(const UClass* Class) const;

	template<typename T>
	bool IsA() const { return IsA(T::StaticClass()); }

	bool IsValidLowLevel() const { return this != nullptr; }

	// -----------------------------------------------------------------------
	// 프로퍼티 시스템 — 런타임 UClass/FProperty 메타데이터를 공유합니다.
	// PostEditChangeProperty: 프로퍼티 값 변경 후 리소스 재로딩 등 처리.
	// CopyPropertiesFrom    : FProperty에 노출된 프로퍼티를 이름 기반으로 복사.
	// -----------------------------------------------------------------------
	virtual void PostEditChangeProperty(const FPropertyChangedEvent& Event) { PostEditProperty(Event.PropertyName); }
	virtual void PostEditProperty(const char* PropertyName) {}
	void CopyPropertiesFrom(UObject* Src);

	virtual void Serialize(FArchive& Ar);
	void SerializeReflectedProperties(FArchive& Ar);

protected:
	FName ObjectName;

private:
	uint32 UUID;
	uint32 InternalIndex;
};

extern TArray<UObject*> GUObjectArray;

UObject* NewObject(UClass* Class);

template <typename T>
inline T* Cast(UObject* Src)
{
	return Src && Src->IsA(T::StaticClass()) ? static_cast<T*>(Src) : nullptr;
}

template <typename T>
inline const T* Cast(const UObject* Src)
{
	return Src && Src->IsA(T::StaticClass()) ? static_cast<const T*>(Src) : nullptr;
}

template <typename T>
inline T* NewObject()
{
	return Cast<T>(NewObject(T::StaticClass()));
}

class UObjectManager : public TSingleton<UObjectManager>
{
	friend class TSingleton<UObjectManager>;

public:
	template<typename T>
	T* CreateObject()
	{
		static_assert(std::is_base_of<UObject, T>::value, "T must derive from UObject");
		T* Obj = new T();

		const UClass* Class = T::StaticClass();
		const char* ClassName = Class ? Class->GetName() : "UObject";
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

	bool ContainsObject(const UObject* InObject)
	{
		if (!InObject)
		{
			return false;
		}

		for (const UObject* Obj : GUObjectArray)
		{
			if (Obj == InObject)
			{
				return true;
			}
		}
		return false;
	}

	UObject* FindByIndex(uint32 Index)
	{
		if (Index >= GUObjectArray.size()) return nullptr;
		return GUObjectArray[Index];
	}
};

template<typename T>
inline UObject* CreateReflectedObject()
{
	static_assert(std::is_base_of_v<UObject, T>, "T must derive from UObject");
	if constexpr (std::is_abstract_v<T>)
	{
		return nullptr;
	}
	else
	{
		return UObjectManager::Get().CreateObject<T>();
	}
}
