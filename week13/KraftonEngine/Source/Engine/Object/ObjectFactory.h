#pragma once

#include <functional>
#include "Object/Object.h"
#include "Core/Singleton.h"

#define REGISTER_FACTORY(TypeName)															\
namespace {																					\
	 struct TypeName##_RegisterFactory {													\
		TypeName##_RegisterFactory() {														\
				FObjectFactory::Get().Register(												\
					#TypeName,																\
					[](UObject* InOuter)->UObject* {										\
						return UObjectManager::Get().CreateObject<TypeName>(InOuter);		\
					}																		\
				);																			\
		}																					\
	};																						\
TypeName##_RegisterFactory G##TypeName##_RegisterFactory;}

#define IMPLEMENT_CLASS(ClassName, ParentClass)                        \
    DEFINE_CLASS(ClassName, ParentClass)                               \
    REGISTER_FACTORY(ClassName)

// Add Component 목록에서만 숨길 때 사용한다. 클래스 RTTI/팩토리 등록은 IMPLEMENT_CLASS가 담당한다.
#define HIDE_FROM_COMPONENT_LIST(ClassName)                            \
namespace {                                                            \
    struct ClassName##_HideFromComponentList {                         \
        ClassName##_HideFromComponentList() {                          \
            ClassName::StaticClass()->AddClassFlags(CF_HiddenInComponentList); \
        }                                                              \
    };                                                                 \
    ClassName##_HideFromComponentList G##ClassName##_HideFromComponentList; \
}

// Different from UFactory class
class FObjectFactory : public TSingleton<FObjectFactory>
{
	friend class TSingleton<FObjectFactory>;

public:
	void Register(const char* TypeName, std::function<UObject*(UObject*)> Spawner) {
		Registry[TypeName] = Spawner;
	}

	UObject* Create(const std::string& TypeName, UObject* InOuter = nullptr) {
		auto Spawner = Registry.find(TypeName);	// Do NOT use array accessor [] here. it will insert a new key if not found.
		return (Spawner != Registry.end()) ? Spawner->second(InOuter) : nullptr;
	}

private:
	TMap<std::string, std::function<UObject*(UObject*)>> Registry;
};
