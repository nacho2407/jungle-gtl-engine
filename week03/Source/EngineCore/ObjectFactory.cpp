#include "ObjectFactory.h"

/** 
 * If the class is not registered, return nullptr.
 */
UObject* FObjectFactory::ConstructObject(const void* id)
{
	auto it = GetRegistry().find(id);
	if (it != GetRegistry().end())
	{
		return it->second();
	}
	return nullptr;
}

void FObjectFactory::RegisterObjectType(const void* id, std::function<UObject*()> func)
{
	GetRegistry()[id] = func;
}