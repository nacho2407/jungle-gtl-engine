#pragma once

#include <functional>
#include "Object/Object.h"
#include "Core/Singleton.h"

#define REGISTER_FACTORY(TypeName)                                                                                   \
    namespace                                                                                                        \
    {                                                                                                                \
    struct TypeName##_RegisterFactory                                                                                \
    {                                                                                                                \
        TypeName##_RegisterFactory()                                                                                 \
        {                                                                                                            \
            FObjectFactory::Get().Register(                                                                          \
                #TypeName,                                                                                           \
                [](UObject* InOuter) -> UObject* { return UObjectManager::Get().CreateObject<TypeName>(InOuter); }); \
        }                                                                                                            \
    };                                                                                                               \
    TypeName##_RegisterFactory G##TypeName##_RegisterFactory;                                                        \
    }

#define IMPLEMENT_CLASS(ClassName, ParentClass) \
    DEFINE_CLASS(ClassName, ParentClass)        \
    REGISTER_FACTORY(ClassName)

#define IMPLEMENT_ABSTRACT_CLASS(ClassName, ParentClass)         \
    DEFINE_CLASS_WITH_FLAGS(ClassName, ParentClass, CF_Abstract) \
    REGISTER_FACTORY(ClassName)

// Different from UFactory class
class FObjectFactory : public TSingleton<FObjectFactory>
{
    friend class TSingleton<FObjectFactory>;

public:
    void Register(const char* TypeName, std::function<UObject*(UObject*)> Spawner)
    {
        Registry[TypeName] = Spawner;
    }

    UObject* Create(const std::string& TypeName, UObject* InOuter = nullptr)
    {
        for (UClass* Cls : UClass::GetAllClasses())
        {
            if (Cls && TypeName == Cls->GetName() && Cls->HasAnyClassFlags(CF_Abstract))
            {
                return nullptr;
            }
        }

        auto Spawner = Registry.find(TypeName); // Do NOT use array accessor [] here. it will insert a new key if not found.
        return (Spawner != Registry.end()) ? Spawner->second(InOuter) : nullptr;
    }

private:
    TMap<std::string, std::function<UObject*(UObject*)>> Registry;
};