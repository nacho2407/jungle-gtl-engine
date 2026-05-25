#include "GarbageCollection.h"

#include "Object/Object.h"
#include "Core/Types/PropertyTypes.h"
#include "Core/Property/ObjectProperty.h"
#include "Core/Property/ArrayProperty.h"
#include "Core/Property/StructProperty.h"
#include "Object/Reflection/UStruct.h"

#include <algorithm>

void FReferenceCollector::AddReferencedObject(UObject* Object)
{
    if (!IsValid(Object))
    {
        return;
    }

    if (Object->HasAnyFlags(RF_PendingKill | RF_Garbage))
    {
        return;
    }

    Stack.push_back(Object);
}

FGCObject::FGCObject()
{
    FGarbageCollector::Get().AddExternalRoot(this);
}

FGCObject::~FGCObject()
{
    FGarbageCollector::Get().RemoveExternalRoot(this);
}

void FGarbageCollector::CollectGarbage()
{
    if (bIsCollecting)
    {
        return;
    }

    bIsCollecting = true;

    MarkAllObjectsUnreachable();
    MarkRoots();
    Sweep();

    bIsCollecting = false;
}

void FGarbageCollector::MarkObject(UObject* Object)
{
    if (!IsAliveObject(Object))
    {
        return;
    }

    if (Object->HasAnyFlags(RF_PendingKill | RF_Garbage | RF_Marked))
    {
        return;
    }

    Object->SetFlags(RF_Marked);
    Object->ClearFlags(RF_Unreachable);

    FReferenceCollector Collector;
    Object->AddReferencedObjects(Collector);

    while (!Collector.Stack.empty())
    {
        UObject* ReferencedObject = Collector.Stack.back();
        Collector.Stack.pop_back();
        MarkObject(ReferencedObject);
    }
}

void FGarbageCollector::AddExternalRoot(FGCObject* Root)
{
    if (!Root)
    {
        return;
    }

    auto It = std::find(ExternalRoots.begin(), ExternalRoots.end(), Root);
    if (It == ExternalRoots.end())
    {
        ExternalRoots.push_back(Root);
    }
}

void FGarbageCollector::RemoveExternalRoot(FGCObject* Root)
{
    auto It = std::find(ExternalRoots.begin(), ExternalRoots.end(), Root);
    if (It != ExternalRoots.end())
    {
        ExternalRoots.erase(It);
    }
}

void FGarbageCollector::MarkAllObjectsUnreachable()
{
    for (UObject* Object : GUObjectArray)
    {
        if (!Object)
        {
            continue;
        }

        Object->ClearFlags(RF_Marked);
        Object->SetFlags(RF_Unreachable);
    }
}

void FGarbageCollector::MarkRoots()
{
    FReferenceCollector Collector;

    for (UObject* Object : GUObjectArray)
    {
        if (Object && Object->IsRooted() && !Object->IsPendingKill())
        {
            Collector.AddReferencedObject(Object);
        }
    }

    for (FGCObject* Root : ExternalRoots)
    {
        if (Root)
        {
            Root->AddReferencedObjects(Collector);
        }
    }

    while (!Collector.Stack.empty())
    {
        UObject* Object = Collector.Stack.back();
        Collector.Stack.pop_back();

        MarkObject(Object);
    }
}

void FGarbageCollector::Sweep()
{
    TArray<UObject*> Snapshot = GUObjectArray;
    for (UObject* Object : Snapshot)
    {
        if (!IsAliveObject(Object))
        {
            continue;
        }

        const bool bShouldDestroy = Object->HasAnyFlags(RF_Unreachable) || Object->IsPendingKill();

        if (!bShouldDestroy)
        {
            continue;
        }

        Object->SetFlags(RF_Garbage);

        if (!Object->HasAnyFlags(RF_BeginDestroy))
        {
            Object->BeginDestroy();
        }
    }

    for (UObject* Object : Snapshot)
    {
        if (!IsAliveObject(Object))
        {
            continue;
        }

        if (!Object->HasAnyFlags(RF_Garbage))
        {
            continue;
        }

        if (!Object->IsReadyForFinishDestroy())
        {
            continue;
        }

        if (!Object->HasAnyFlags(RF_FinishDestroy))
        {
            Object->FinishDestroy();
        }

        delete Object;
    }
}
