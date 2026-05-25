#include "ReferenceCollector.h"
#include "Object.h"

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
