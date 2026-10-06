#pragma once

#include <functional>
#include "Object/Object.h"
#include "Core/Singleton.h"

// Add Component 목록에서만 숨길 때 사용한다.
#define HIDE_FROM_COMPONENT_LIST(ClassName)                            \
namespace {                                                            \
    struct ClassName##_HideFromComponentList {                         \
        ClassName##_HideFromComponentList() {                          \
            FClassFlagRegistry::AddClassFlags(#ClassName, CF_HiddenInComponentList); \
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
