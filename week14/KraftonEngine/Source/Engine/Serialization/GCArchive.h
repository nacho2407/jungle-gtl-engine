#pragma once

#include "Serialization/Archive.h"
#include "Object/ReferenceCollector.h"

class FGCArchive : public FArchive
{
public:
	explicit FGCArchive(FReferenceCollector& InCollector)
		: Collector(InCollector)
	{
		bIsSaving = true;
		bIsLoading = false;
	}

	bool UsesCustomObjectReferenceSerialization() const override
	{
		return true;
	}

	bool IsGarbageCollecting() const override
	{
		return true;
	}

	void SerializeObjectReference(UObject*& Object) override
	{
		Collector.AddReferencedObject(Object);
	}

	void Serialize(void* /*Data*/, size_t /*Num*/) override
	{
		// GC is only interested in UObject references.
	}

private:
	FReferenceCollector& Collector;
};
