#pragma once

#include "ObjectFactory.h"

#define DECLARE_RTTI(Cls, ParentCls)\
  public:\
    static const void* GetClass()\
    {\
        static int Id = 0;\
        return &Id;\
    } \
    virtual bool IsA(const void* Id) const override\
    {\
        if (GetClass() == Id)\
            return true;\
        return ParentCls::IsA(Id);\
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

    void *operator new(size_t Size);
    void  operator delete(void* Pointer, size_t Size);

  public:
    uint32 UUID;
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
