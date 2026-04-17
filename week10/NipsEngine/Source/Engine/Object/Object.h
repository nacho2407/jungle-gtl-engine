#pragma once

#include "EngineStatics.h"
#include "Object/FName.h"
#include "Core/Singleton.h"
#include "Core/PropertyTypes.h"
#include "Serialization/Archive.h"

#define DECLARE_CLASS(ClassName, ParentClass)                          \
    static const FTypeInfo s_TypeInfo;                                 \
    const FTypeInfo* GetTypeInfo() const override {                    \
        return &s_TypeInfo;                                            \
    }                                                                  

#define DEFINE_CLASS(ClassName, ParentClass)                           \
    const FTypeInfo ClassName::s_TypeInfo = {                          \
        #ClassName,                                                    \
        &ParentClass::s_TypeInfo,                                      \
        sizeof(ClassName)                                              \
    };

enum EClassFlags : uint32_t
{
	CF_None = 0,
	CF_Actor = 1 << 0,
	CF_Component = 1 << 1,
	CF_Camera = 1 << 2,
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
};

class UObject
{
public:
	UObject();
	virtual ~UObject();

	// -----------------------------------------------------------------------
	// 복제 시스템
	// Duplicate()     : FObjectFactory 로 같은 타입의 인스턴스를 생성한 뒤
	//                   CopyPropertiesFrom → PostDuplicate 순으로 호출합니다.
	//                   개별 클래스에서 오버라이드할 필요 없습니다.
	// PostDuplicate() : Duplicate() 내부에서 CopyPropertiesFrom 직후 호출되는 가상 훅.
	//                   프로퍼티 시스템에 노출되지 않은 필드 복사, 포인터 재연결 등
	//                   클래스별 후처리를 이곳에 구현합니다.
	//                   하위 클래스 구현 시 부모의 PostDuplicate 를 먼저 호출해야 합니다.
	// -----------------------------------------------------------------------
	virtual UObject* Duplicate();
	virtual void PostDuplicate(UObject* Original) {}

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
	virtual const FTypeInfo* GetTypeInfo() const { return &s_TypeInfo; }

	template<typename T>
	bool IsA() const { return GetTypeInfo()->IsA(&T::s_TypeInfo); }

	bool IsValidLowLevel() const { return this != nullptr; }

	// -----------------------------------------------------------------------
	// 프로퍼티 시스템 — 모든 UObject 파생 클래스가 공유합니다.
	// GetEditableProperties : 에디터에 노출할 프로퍼티 목록 반환.
	//                         하위 클래스에서 override 하여 속성 추가.
	// PostEditProperty      : 프로퍼티 값 변경 후 리소스 재로딩 등 처리.
	// CopyPropertiesFrom    : GetEditableProperties 에 노출된 프로퍼티를
	//                         이름 기반으로 매칭하여 Src → this 방향으로 복사.
	// -----------------------------------------------------------------------
	virtual void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) {}
	virtual void PostEditProperty(const char* PropertyName) {}
	void CopyPropertiesFrom(UObject* Src);

	virtual void Serialize(FArchive& Ar);

	static const FTypeInfo s_TypeInfo;

protected:
	FName ObjectName;

private:
	uint32 UUID;
	uint32 InternalIndex;
};

extern TArray<UObject*> GUObjectArray;

template <typename T>
inline T* Cast(UObject* Src)
{
	if (Src && Src->IsA<T>())
	{
		return static_cast<T*>(Src);
	}
	return nullptr;
}

template <typename T>
inline const T* Cast(const UObject* Src)
{
	if (Src && Src->IsA<T>())
	{
		return static_cast<const T*>(Src);
	}
	return nullptr;
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