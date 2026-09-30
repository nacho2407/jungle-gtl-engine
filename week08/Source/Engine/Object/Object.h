// 오브젝트 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Profiling/MemoryStats.h"
#include "Object/FName.h"
#include "Core/Singleton.h"
#include "Object/UClass.h"

class FArchive;

// ---------------------------------------------------------------------------
// RTTI Macros
// ---------------------------------------------------------------------------

#define DECLARE_CLASS(ClassName, ParentClass) \
    using Super = ParentClass;                \
    static UClass StaticClassInstance;        \
    static FClassRegistrar s_Registrar;       \
    static UClass* StaticClass()              \
    {                                         \
        return &StaticClassInstance;          \
    }                                         \
    UClass* GetClass() const override         \
    {                                         \
        return StaticClass();                 \
    }

#define DEFINE_CLASS_WITH_FLAGS(ClassName, ParentClass, FlagsValue) \
    UClass ClassName::StaticClassInstance(                          \
        #ClassName,                                                 \
        &ParentClass::StaticClassInstance,                          \
        sizeof(ClassName),                                          \
        FlagsValue);                                                \
    FClassRegistrar ClassName::s_Registrar(&ClassName::StaticClassInstance);

#define DEFINE_CLASS(ClassName, ParentClass) \
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
    template <typename T>
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

    // RTTI
    virtual UClass* GetClass() const { return StaticClass(); }

    template <typename T>
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

// UObjectManager는 관련 객체의 생성, 조회, 수명 관리를 담당합니다.
class UObjectManager : public TSingleton<UObjectManager>
{
    friend class TSingleton<UObjectManager>;

public:
    template <typename T>
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
        if (Index >= GUObjectArray.size())
            return nullptr;
        return GUObjectArray[Index];
    }
};

template <typename T>
T* Cast(UObject* Obj)
{
    return (Obj && Obj->IsA<T>()) ? static_cast<T*>(Obj) : nullptr;
}

template <typename T>
const T* Cast(const UObject* Obj)
{
    return (Obj && Obj->IsA<T>()) ? static_cast<const T*>(Obj) : nullptr;
}
