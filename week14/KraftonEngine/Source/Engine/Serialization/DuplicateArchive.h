#pragma once

#include "Serialization/MemoryArchive.h"

class FDuplicateArchiveContext
{
public:
	void AddObjectMapping(uint32 SourceUUID, UObject* Duplicate)
	{
		if (SourceUUID != 0 && Duplicate)
		{
			ObjectMap[SourceUUID] = Duplicate;
		}
	}

	UObject* ResolveObjectReference(uint32 SourceUUID) const
	{
		auto It = ObjectMap.find(SourceUUID);
		return It != ObjectMap.end() ? It->second : nullptr;
	}

	void AddObjectReferenceFixup(uint32 SourceUUID, std::function<void(UObject*)> Fixup)
	{
		if (SourceUUID != 0 && Fixup)
		{
			PendingFixups.push_back({ SourceUUID, Fixup });
		}
	}

	void ResolveObjectReferenceFixups()
	{
		for (const FObjectReferenceFixup& Fixup : PendingFixups)
		{
			if (UObject* Duplicate = ResolveObjectReference(Fixup.SourceUUID))
			{
				Fixup.Apply(Duplicate);
			}
		}
		PendingFixups.clear();
	}

private:
	struct FObjectReferenceFixup
	{
		uint32 SourceUUID = 0;
		std::function<void(UObject*)> Apply;
	};

	TMap<uint32, UObject*> ObjectMap;
	TArray<FObjectReferenceFixup> PendingFixups;
};

class FDuplicateDataWriter : public FMemoryArchive
{
public:
	FDuplicateDataWriter()
		: FMemoryArchive(/*bInIsSaving=*/true)
	{
	}
};

class FDuplicateDataReader : public FMemoryArchive
{
public:
	FDuplicateDataReader(const TArray<uint8>& InBuffer, FDuplicateArchiveContext& InDuplicateContext)
		: FMemoryArchive(InBuffer, /*bInIsSaving=*/false)
		, DuplicateContext(InDuplicateContext)
	{
	}

	bool IsObjectReferenceRemapping() const override { return true; }
	UObject* ResolveObjectReference(uint32 SourceUUID) const override
	{
		return DuplicateContext.ResolveObjectReference(SourceUUID);
	}
	void AddObjectReferenceFixup(uint32 SourceUUID, std::function<void(UObject*)> Fixup) override
	{
		DuplicateContext.AddObjectReferenceFixup(SourceUUID, Fixup);
	}

private:
	FDuplicateArchiveContext& DuplicateContext;
};
