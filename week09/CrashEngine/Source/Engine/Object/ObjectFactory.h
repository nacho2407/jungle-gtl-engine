// 오브젝트 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include <functional>
#include "Object/Object.h"
#include "Core/Singleton.h"

#define REGISTER_FACTORY(TypeName)                                                                                   \
    namespace                                                                                                        \
    {                                                                                                                \
    /* TypeName registers a factory for this UObject-derived type. */                                         \
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

#define IMPLEMENT_COMPONENT_CLASS(ClassName, ParentClass, EditorCategoryValue) \
    DEFINE_CLASS_WITH_FLAGS_AND_CATEGORY(ClassName, ParentClass, CF_Component, EditorCategoryValue) \
    REGISTER_FACTORY(ClassName)

#define IMPLEMENT_HIDDEN_COMPONENT_CLASS(ClassName, ParentClass) \
    DEFINE_CLASS_WITH_FLAGS_AND_CATEGORY(ClassName, ParentClass, CF_Component, EEditorComponentCategory::Hidden) \
    REGISTER_FACTORY(ClassName)

#define IMPLEMENT_ABSTRACT_CLASS(ClassName, ParentClass)         \
    DEFINE_CLASS_WITH_FLAGS(ClassName, ParentClass, CF_Abstract) \
    REGISTER_FACTORY(ClassName)

#define IMPLEMENT_ABSTRACT_COMPONENT_CLASS(ClassName, ParentClass) \
    DEFINE_CLASS_WITH_FLAGS_AND_CATEGORY(ClassName, ParentClass, CF_Component | CF_Abstract, EEditorComponentCategory::Hidden) \
    REGISTER_FACTORY(ClassName)

// FObjectFactory는 오브젝트 영역의 핵심 동작을 담당합니다.
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
