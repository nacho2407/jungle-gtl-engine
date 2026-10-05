#pragma once

#include "EngineStatics.h"
#include "Core/Containers/Array.h"
#include "Core/Containers/Map.h"
#include "Core/Containers/String.h"
#include "Core/PropertyTypes.h"
#include "Object/FName.h"
#include "Core/Singleton.h"
#include "Core/Reflection/ReflectionMacros.h"
#include "Serialization/Archive.h"
#include <cstdlib>
#include <type_traits>

class UClass;
class UFunction;
class FDebugDetailsBuilder;
struct FDuplicateContext;

enum EClassFlags : uint32_t
{
	CF_None = 0,
	CF_Actor = 1 << 0,
	CF_Component = 1 << 1,
	CF_Camera = 1 << 2,
	CF_Abstract = 1 << 3,
	CF_Placeable = 1 << 4,
	CF_SpawnableComponent = 1 << 5,
};

// 모든 실제 게임/에디터 객체의 공통 기반 클래스입니다.
// UUID, Object Name, RTTI, UPROPERTY 필드, UFUNCTION 함수, 프로퍼티 편집 후 후처리(PostEditProperty) 기능을 제공합니다.
class UObject
{
public:
	UObject();
	virtual ~UObject();
	
	// ────────────────────────────────────────────────────────────
	// UUID/InternalIndex는 UObjectManager와 GUObjectArray가 런타임에서 객체를 식별하기 위해 사용합니다.
	// ObjectName은 저장/에디터 표시용으로 사용되는 이름입니다.
	// ────────────────────────────────────────────────────────────
	uint32 GetUUID() const { return UUID; }
	uint32 GetInternalIndex() const { return InternalIndex; }
	void SetUUID(uint32 InUUID) { UUID = InUUID; }
	void SetInternalIndex(uint32 InIndex) { InternalIndex = InIndex; }

	FName GetFName() const { return ObjectName; }
	void SetFName(const FName& InName) { ObjectName = InName; }

	struct FObjectNameProxy : public FString
	{
		using FString::FString;
		FObjectNameProxy(const FString& InStr) : FString(InStr) {}
		const char* operator*() const { return c_str(); }
	};

	FObjectNameProxy GetName() const { return FObjectNameProxy(ObjectName.ToString()); }

	// ────────────────────────────────────────────────────────────
	// 런타임 타입 정보(RTTI): 리플렉션된 클래스들은 GetClass()에서 자신의 클래스를 반환합니다.
	// ────────────────────────────────────────────────────────────
	static UClass* StaticClass();
	virtual UClass* GetClass() const { return StaticClass(); }
	const char* GetClassName() const;

	bool IsA(const UClass* Class) const;

	template<typename T>
	bool IsA() const { return IsA(T::StaticClass()); }

	bool IsValidLowLevel() const { return this != nullptr; }
	
	// ────────────────────────────────────────────────────────────
	// 리플렉션된 함수 실행: ProcessEvent()에서 UFunction::Invoke()를 호출하여 리플렉션된 함수를 실행합니다.
	// ────────────────────────────────────────────────────────────
	virtual void ProcessEvent(UFunction* Function, void* Params);

	// ────────────────────────────────────────────────────────────
	// 프로퍼티 갱신 후처리: 프로퍼티가 런타임에 편집되면 PostEditProperty()가 호출됩니다.
	// ex) 객체의 에셋 경로를 수정하면, 해당 에셋을 다시 로드해서 에디터에 보여줘야 합니다.
	// ────────────────────────────────────────────────────────────
	virtual void PostEditProperty(const char* PropertyName) {}
	virtual void BuildDebugDetails(FDebugDetailsBuilder& Builder) {}

	// ────────────────────────────────────────────────────────────
	// Duplicate: UClass/CreateFunc을 통해 같은 클래스의 객체를 생성하고, 프로퍼티를 복사한 뒤 PostDuplicate()를 호출합니다.
	// PostDuplicate: 리플렉션되지 않는 필드를 처리하거나, 관계를 복구하는 용도로 사용됩니다.
    // CopyPropertiesFrom: 컨텍스트에서 이름 기준으로 리플렉션된 프로퍼티들을 복제된 객체에 다시 맵핑합니다.
	// ────────────────────────────────────────────────────────────
	virtual UObject* Duplicate(const FDuplicateContext* Context = nullptr);
	virtual void PostDuplicate(UObject* Original) { ObjectName = Original->ObjectName; }
	void CopyPropertiesFrom(UObject* Src, const FDuplicateContext* Context = nullptr);

	// ────────────────────────────────────────────────────────────
	// 직렬화: 객체의 이름(Type, ObjectName)을 기록한 뒤, SerializeProperties()로 리플렉션 프로퍼티들을 직렬화합니다.
	// ────────────────────────────────────────────────────────────
	virtual void Serialize(FArchive& Ar);
	void SerializeProperties(FArchive& Ar);

protected:
	explicit UObject(bool bRegisterObject);

	FName ObjectName;

private:
	uint32 UUID = 0;
	uint32 InternalIndex = 0;
	bool bRegisteredWithObjectArray = false;
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

// 전역 UObject 생성/삭제/조회 관리자입니다.
class UObjectManager : public TSingleton<UObjectManager>
{
	friend class TSingleton<UObjectManager>;

public:
	template<typename T>
	T* CreateObject()
	{
		static_assert(std::is_base_of<UObject, T>::value, "T must derive from UObject");
		T* Obj = new T();
		AssignObjectName(Obj, T::StaticClass());
		return Obj;
	}

	void AssignObjectName(UObject* Obj, const UClass* Class);

	void DestroyObject(UObject* Obj)
	{
		if (!Obj)
		{
			return;
		}
		delete Obj;
	}

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

	// 크래시 테스트용 결함 주입 기능입니다. 실제 게임 코드에서는 절대 사용하지 마세요.
	UObject* GetRandomObject()
	{
		if (GUObjectArray.empty())
		{
			return nullptr;
		}

		const int32 Index = rand() % static_cast<int32>(GUObjectArray.size());
		return GUObjectArray[Index];
	}

private:
	TMap<FString, uint32> NameCounters;
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
