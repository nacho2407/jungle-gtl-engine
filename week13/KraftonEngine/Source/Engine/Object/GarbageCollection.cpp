#include "GarbageCollection.h"

#include "Object/Object.h"
#include "Core/Types/PropertyTypes.h"
#include "Core/Property/ObjectProperty.h"
#include "Core/Property/ArrayProperty.h"
#include "Core/Property/StructProperty.h"
#include "Object/Reflection/UStruct.h"

#include <algorithm>

namespace
{
    FString DescribeObjectForGC(const UObject* Object)
    {
        if (!Object)
        {
            return FString("<null>");
        }

        FString Result;
        if (Object->GetClass() && Object->GetClass()->GetName())
        {
            Result += Object->GetClass()->GetName();
        }
        else
        {
            Result += "UObject";
        }
        Result += " ";
        Result += Object->GetName();
        return Result;
    }
}

void FReferenceCollector::AddReferencedObject(UObject* Object, const char* ReferenceName)
{
    if (!IsValid(Object))
    {
        return;
    }

    if (Object->HasAnyFlags(RF_PendingKill | RF_Garbage))
    {
        return;
    }

    Stack.push_back(FGCReferenceEdge{
        Object,
        Referencer,
        ReferenceName ? ReferenceName : CurrentReferenceName
    });
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
    LastReferenceEdges.clear();

    MarkAllObjectsUnreachable();
    MarkRoots();
    Sweep();

    bIsCollecting = false;
}

void FGarbageCollector::MarkObject(UObject* Object)
{
    MarkObjectFromEdge(FGCReferenceEdge{ Object, nullptr, "DirectMark" });
}

void FGarbageCollector::MarkObjectFromEdge(const FGCReferenceEdge& Edge)
{
    UObject* Object = Edge.Object;
    if (!IsAliveObject(Object))
    {
        return;
    }

    if (Object->HasAnyFlags(RF_PendingKill | RF_Garbage | RF_Marked))
    {
        return;
    }

    LastReferenceEdges[Object] = Edge;

    Object->SetFlags(RF_Marked);
    Object->ClearFlags(RF_Unreachable);

    FReferenceCollector Collector(Object);
    Object->AddReferencedObjects(Collector);

    while (!Collector.Stack.empty())
    {
        FGCReferenceEdge ReferencedEdge = Collector.Stack.back();
        Collector.Stack.pop_back();
        MarkObjectFromEdge(ReferencedEdge);
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
            Collector.AddReferencedObject(Object, "RootSet");
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
        FGCReferenceEdge Edge = Collector.Stack.back();
        Collector.Stack.pop_back();

        if (!Edge.ReferenceName)
        {
            Edge.ReferenceName = "ExternalRoot";
        }

        MarkObjectFromEdge(Edge);
    }
}

bool FGarbageCollector::GetLastReferenceChain(UObject* Object, TArray<FString>& OutChain) const
{
    OutChain.clear();

    if (!Object)
    {
        return false;
    }

    auto It = LastReferenceEdges.find(Object);
    if (It == LastReferenceEdges.end())
    {
        return false;
    }

    TArray<FString> ReverseChain;
    TSet<UObject*> Visited;
    UObject* Current = Object;

    while (Current)
    {
        if (Visited.find(Current) != Visited.end())
        {
            ReverseChain.push_back(FString("<cycle in GC reference chain>"));
            break;
        }
        Visited.insert(Current);

        auto EdgeIt = LastReferenceEdges.find(Current);
        if (EdgeIt == LastReferenceEdges.end())
        {
            ReverseChain.push_back(DescribeObjectForGC(Current));
            break;
        }

        const FGCReferenceEdge& Edge = EdgeIt->second;
        FString Line = DescribeObjectForGC(Current);
        if (Edge.ReferenceName && Edge.ReferenceName[0])
        {
            Line += " <= ";
            Line += Edge.ReferenceName;
        }
        ReverseChain.push_back(Line);

        Current = Edge.Referencer;
    }

    for (auto RevIt = ReverseChain.rbegin(); RevIt != ReverseChain.rend(); ++RevIt)
    {
        OutChain.push_back(*RevIt);
    }

    return !OutChain.empty();
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
