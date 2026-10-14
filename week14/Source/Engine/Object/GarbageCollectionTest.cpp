#include "GarbageCollectionTest.h"

#include "GarbageCollector.h"
#include "Object.h"
#include "ReferenceCollector.h"
#include "Object/Ptr/WeakObjectPtr.h"
#include "Core/Logging/Log.h"

namespace
{
	class FScopedGCReferenceProvider : public IGCReferenceProvider
	{
	public:
		FScopedGCReferenceProvider()
		{
			for (UObject* Object : GUObjectArray)
			{
				if (IsValid(Object))
				{
					ExistingObjects.push_back(Object);
				}
			}

			FGCReferenceRegistry::Register(this);
		}

		~FScopedGCReferenceProvider()
		{
			FGCReferenceRegistry::Unregister(this);
		}

		void SetExtraRoot(UObject* Object)
		{
			ExtraRoot = Object;
		}

		void AddReferencedObjects(FReferenceCollector& Collector) override
		{
			for (UObject* Object : ExistingObjects)
			{
				if (IsValid(Object))
				{
					Collector.AddReferencedObject(Object);
				}
			}

			if (IsValid(ExtraRoot))
			{
				Collector.AddReferencedObject(ExtraRoot);
			}
		}

	private:
		TArray<UObject*> ExistingObjects;
		UObject* ExtraRoot = nullptr;
	};

	void LogCheck(const char* Name, bool bPassed, bool& bAllPassed)
	{
		UE_LOG("[GC Test] %s: %s", bPassed ? "PASS" : "FAIL", Name);
		bAllPassed = bAllPassed && bPassed;
	}
}

bool FGarbageCollectionTest::RunAll()
{
	UE_LOG("[GC Test] Begin");

	bool bAllPassed = true;
	FScopedGCReferenceProvider ScopedRoots;

	UObject* RootedObject = UObjectManager::Get().CreateObject<UObject>();
	UObject* UnreachableObject = UObjectManager::Get().CreateObject<UObject>();

	ScopedRoots.SetExtraRoot(RootedObject);

	const uint32 UnreachableIndex = UnreachableObject->GetInternalIndex();
	const uint32 UnreachableSerial = UnreachableObject->GetSerialNumber();
	FWeakObjectPtr WeakUnreachable(UnreachableObject);

	FGarbageCollector GC;
	GC.CollectGarbage();

	LogCheck("registered provider keeps rooted object alive", IsValid(RootedObject) && !RootedObject->IsPendingKill(), bAllPassed);
	LogCheck("unreferenced object becomes PendingKill", UnreachableObject->IsPendingKill(), bAllPassed);
	LogCheck("weak pointer rejects PendingKill object", !WeakUnreachable.IsValid(), bAllPassed);

	UObjectManager::Get().DestroyObject(UnreachableObject);

	const bool bSlotWasCleared =
		UnreachableIndex < GUObjectSlots.size()
		&& GUObjectSlots[UnreachableIndex].Object == nullptr
		&& GUObjectSlots[UnreachableIndex].SerialNumber != UnreachableSerial;
	LogCheck("destroy clears object slot and bumps serial", bSlotWasCleared, bAllPassed);
	LogCheck("weak pointer stays invalid after destroy", !WeakUnreachable.IsValid(), bAllPassed);

	UObject* ReusedObject = UObjectManager::Get().CreateObject<UObject>();
	const bool bReusedSlotSafely =
		ReusedObject->GetInternalIndex() != UnreachableIndex
		|| ReusedObject->GetSerialNumber() != UnreachableSerial;
	LogCheck("slot reuse does not validate stale weak pointer", bReusedSlotSafely && !WeakUnreachable.IsValid(), bAllPassed);

	UObjectManager::Get().DestroyObject(ReusedObject);
	UObjectManager::Get().DestroyObject(RootedObject);

	UE_LOG("[GC Test] End: %s", bAllPassed ? "PASS" : "FAIL");
	return bAllPassed;
}
