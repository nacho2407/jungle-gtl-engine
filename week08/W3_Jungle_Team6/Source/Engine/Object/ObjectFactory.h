#pragma once

#include <iostream>
#include <functional>
#include "Object/Object.h"
#include "Core/Singleton.h"

#define REGISTER_FACTORY(TypeName)															\
namespace {																					\
	 struct TypeName##_RegisterFactory {													\
		TypeName##_RegisterFactory() {														\
				FObjectFactory::Get().Register(												\
					#TypeName,																\
					[]()->UObject* {return UObjectManager::Get().CreateObject<TypeName>();} \
				);																			\
		}																					\
	};																						\
TypeName##_RegisterFactory G##TypeName##_RegisterFactory;} 																												

// Different from UFactory class
class FObjectFactory : public TSingleton<FObjectFactory>
{
	friend class TSingleton<FObjectFactory>;

public:
	void Register(const char* TypeName, std::function<UObject*()> Spawner) {
		Registry[TypeName] = Spawner;
	}

	UObject* Create(const std::string& TypeName) {
		auto Spawner = Registry.find(TypeName);	// Do NOT use array accessor [] here. it will insert a new key if not found.
		return (Spawner != Registry.end()) ? Spawner->second() : nullptr;
	}

private:
	TMap<std::string, std::function<UObject*()>> Registry;
};