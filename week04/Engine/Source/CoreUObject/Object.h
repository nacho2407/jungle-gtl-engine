#pragma once

#include "ObjectFactory.h"
#include "Core/Misc/Name.h"

#define DECLARE_RTTI(Cls, ParentCls)\
  public:\
    static const void* GetClass()\
    {\
        static int Id {0};\
        return &Id;\
    } \
    static const char* GetStaticTypeName()\
    {\
        return #Cls;\
    }\
    virtual bool IsA(const void* Id) const override\
    {\
        if (GetClass() == Id)\
            return true;\
        return ParentCls::IsA(Id);\
    }\
    virtual const char* GetTypeName() const override\
    {\
        return #Cls;\
    }\
    void *operator new(size_t Size)\
    {\
        return UObject::AllocateObject(Size, #Cls);\
    }\
    void operator delete(void* Pointer, size_t Size)\
    {\
        UObject::FreeObject(Pointer, Size);\
    }

#define REGISTER_CLASS(Namespace, Cls)\
    namespace\
    {\
        struct Cls##Register\
        {\
            Cls##Register()\
            {\
                FObjectFactory::RegisterObjectType(Namespace::Cls::GetClass(),                       \
                                                   []() -> UObject *   \
                                                   { return new Namespace::Cls(); });\
            }\
        };\
        static Cls##Register Global_##Cls##Register;\
    }

class FUObjectArray;

extern FUObjectArray GUObjectArray;

class ENGINE_API UObject
{
  public:
    UObject();
    virtual ~UObject();

  public:
    static const void* GetClass()
    {
        static int Id = 0;
        return &Id;
    }
    virtual bool IsA(const void* Id) const { return GetClass() == Id; }
    virtual const char* GetTypeName() const { return "UObject"; }

    void *operator new(size_t Size);
    void  operator delete(void* Pointer, size_t Size);
    static void* AllocateObject(size_t Size, const char* InTypeName);
    static void FreeObject(void* Pointer, size_t Size);

    /**
     * @brief 본 객체가 현재 object array 상에서 유효한지 검사합니다.
     */
    bool IsValidLowLevel() const;

  public:
    uint32 UUID;
    Engine::Core::Misc::FName  Name;
    uint32 InternalIndex;
};

template <typename To, typename From> To *Cast(From* Object)
{
    if (Object && Object->IsA(To::GetClass()))
    {
        return reinterpret_cast<To*>(Object);
    }
    return nullptr;
}
