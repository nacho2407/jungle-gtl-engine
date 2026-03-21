#include <Core/CoreMinimal.h>
#include "ObjectFactory.h"

/** 
 * If the class is not registered, return nullptr.
 */
UObject* FObjectFactory::ConstructObject(const void* Id)
{
	auto It = GetRegistry().find(Id);
	if (It != GetRegistry().end())
	{
		return It->second();
	}
	return nullptr;
}

void FObjectFactory::RegisterObjectType(const void* Id, std::function<UObject*()> FactoryFunction)
{
	GetRegistry()[Id] = FactoryFunction;
}
