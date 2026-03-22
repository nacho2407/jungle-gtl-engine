#pragma once

#include "EngineStatics.h"
#include "Object/FName.h"

#define DECLARE_CLASS(ClassName, ParentClass)                          \
    static const FTypeInfo s_TypeInfo;                                 \
    const FTypeInfo* GetTypeInfo() const override {                    \
        return &s_TypeInfo;                                            \
    }                                                                  \
    static ClassName* Cast(UObject* Obj) {                             \
        return Obj ? Obj->Cast<ClassName>() : nullptr;                 \
    }

#define DEFINE_CLASS(ClassName, ParentClass)                           \
    const FTypeInfo ClassName::s_TypeInfo = {                          \
        #ClassName,                                                    \
        &ParentClass::s_TypeInfo,                                      \
        sizeof(ClassName)                                              \
    };

enum EClassFlags : uint32_t {
	CF_None		 = 0,
	CF_Actor	 = 1 << 0,
	CF_Component = 1 << 1,
	CF_Camera	 = 1 << 2,
};

struct FTypeInfo {
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
	uint32 UUID;
	uint32 InternalIndex;
	bool bPendingKill;

	UObject();
	virtual ~UObject();

	static void* operator new(size_t Size)
	{
		void* Ptr = std::malloc(Size);
		if (Ptr)
		{
			EngineStatics::OnAllocated(static_cast<uint32>(Size));
		}
		return Ptr;
	}

	static void operator delete(void* Ptr, size_t Size)
	{
		if (Ptr)
		{
			EngineStatics::OnDeallocated(static_cast<uint32>(Size));
			std::free(Ptr);
;		}
	}

	// FName
	FName GetFName() const { return ObjectName; }
	void SetFName(const FName& InName) { ObjectName = InName; }

	// RTTI stuffs
	virtual const FTypeInfo* GetTypeInfo() const { return &s_TypeInfo; }

	template<typename T>
	bool IsA() const { return GetTypeInfo()->IsA(&T::s_TypeInfo); }

	template<typename T>
	T* Cast() { return IsA<T>() ? static_cast<T*>(this) : nullptr; }

	template<typename T>
	const T* Cast() const { return IsA<T>() ? static_cast<const T*>(this) : nullptr; }

	static const FTypeInfo s_TypeInfo;

protected:
	FName ObjectName;
};

extern TArray<UObject*> GUObjectArray;


class UObjectManager {
public:
	// Singleton
	static UObjectManager& Get()
	{
		static UObjectManager instance;
		return instance;
	}

	template<typename T>
	T* CreateObject() {
		T* Obj = new T();
		//GUObjectArray.push_back(Obj);
		return Obj;
	}

	// Does not detroy the target object right away. Only marks for death
	void DestroyObject(UObject* Obj) {
		if (!Obj) {
			return;
		}
		Obj->bPendingKill = true;
	}

	void CollectGarbage() {
		for (int i = 0; i < GUObjectArray.size(); i++) {
			if (GUObjectArray[i] && GUObjectArray[i]->bPendingKill) {
				delete GUObjectArray[i];
				GUObjectArray[i] = nullptr;
			}
		}
	}

	UObject* FindByUUID(uint32 UUID)
	{
		for (auto* Obj : GUObjectArray)
			if (Obj && Obj->UUID == UUID)
				return Obj;
		return nullptr;
	}

	UObject* FindByIndex(uint32 Index)
	{
		if (Index >= GUObjectArray.size()) return nullptr;
		return GUObjectArray[Index];   // may be null if destroyed
	}

	// Used to kill the current rendering scene (i.e for loading a new savefile)
	void PurgeScene();

private:
	UObjectManager() = default;
	~UObjectManager() { CollectGarbage(); }

	//TArray<UObject*> GUObjectArray;
};