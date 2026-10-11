#include "ReferenceCollector.h"
#include "Object.h"
#include <algorithm>

TArray<IGCReferenceProvider*> FGCReferenceRegistry::Providers;

void FReferenceCollector::AddReferencedObject(UObject* Object)
{
	if (!IsValid(Object))
	{
		return;
	}

	if (Object->IsGarbageMarked())
	{
		return;
	}

	Object->SetGarbageMarked(true);
	MarkStack.push_back(Object);
}

UObject* FReferenceCollector::Pop()
{

	if (MarkStack.empty())
	{
		return nullptr;
	}

	UObject* Object = MarkStack.back();
	MarkStack.pop_back();
	return Object;
}



void FGCReferenceRegistry::Register(IGCReferenceProvider* Provider)
{
	if (!Provider)
	{
		return;
	}

	auto It = std::find(Providers.begin(), Providers.end(), Provider);
	if (It != Providers.end())
	{
		return;
	}

	Providers.push_back(Provider);
}

void FGCReferenceRegistry::Unregister(IGCReferenceProvider* Provider)
{
	if (!Provider)
	{
		return;
	}

	auto It = std::find(Providers.begin(), Providers.end(), Provider);
	if (It != Providers.end())
	{
		Providers.erase(It);
	}
}

void FGCReferenceRegistry::AddReferencedObjects(FReferenceCollector& Collector)
{
	for (IGCReferenceProvider* Provider : Providers)
	{
		if (Provider)
		{
			Provider->AddReferencedObjects(Collector);
		}
	}
}

