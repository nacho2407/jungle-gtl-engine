#pragma once

#include "Core/Types/CoreTypes.h"
#include "Core/Singleton.h"
#include "Ptr/ObjectPtr.h"

class UObject;

struct FGCReferenceEdge
{
    UObject*    Object = nullptr;
    UObject*    Referencer = nullptr;
    const char* ReferenceName = nullptr;
};

class FReferenceCollector
{
public:
    explicit FReferenceCollector(UObject* InReferencer = nullptr)
        : Referencer(InReferencer)
    {}

    void AddReferencedObject(UObject* Object, const char* ReferenceName = nullptr);

    template <typename T>
    void AddReferencedObject(const TObjectPtr<T>& ObjectPtr, const char* ReferenceName = nullptr)
    {
        AddReferencedObject(ObjectPtr.Get(), ReferenceName);
    }

    template <typename T>
    void AddReferencedObjects(const TArray<T*>& Objects, const char* ReferenceName = nullptr)
    {
        for (T* Object : Objects)
        {
            AddReferencedObject(Object, ReferenceName);
        }
    }

    template <typename T>
    void AddReferencedObjects(const TArray<TObjectPtr<T>>& Objects, const char* ReferenceName = nullptr)
    {
        for (const TObjectPtr<T>& ObjectPtr : Objects)
        {
            AddReferencedObject(ObjectPtr.Get(), ReferenceName);
        }
    }

private:
    friend class FGarbageCollector;
    friend class FScopedReferenceName;

    UObject* Referencer = nullptr;
    const char* CurrentReferenceName = nullptr;
    TArray<FGCReferenceEdge> Stack;
};

class FScopedReferenceName
{
public:
    FScopedReferenceName(FReferenceCollector& InCollector, const char* InReferenceName)
        : Collector(InCollector)
        , PreviousReferenceName(InCollector.CurrentReferenceName)
    {
        Collector.CurrentReferenceName = InReferenceName;
    }

    ~FScopedReferenceName()
    {
        Collector.CurrentReferenceName = PreviousReferenceName;
    }

private:
    FReferenceCollector& Collector;
    const char* PreviousReferenceName = nullptr;
};

class FGCObject
{
public:
    FGCObject();
    virtual ~FGCObject();

    virtual void AddReferencedObjects(FReferenceCollector& Collector) = 0;
};

class FGarbageCollector : public TSingleton<FGarbageCollector>
{
    friend class TSingleton<FGarbageCollector>;

public:
    void CollectGarbage();
    void MarkObject(UObject* Object);
    void AddExternalRoot(FGCObject* Root);
    void RemoveExternalRoot(FGCObject* Root);
    bool GetLastReferenceChain(UObject* Object, TArray<FString>& OutChain) const;

private:
    FGarbageCollector() = default;

    void MarkAllObjectsUnreachable();
    void MarkRoots();
    void MarkObjectFromEdge(const FGCReferenceEdge& Edge);
    void Sweep();

private:
    TArray<FGCObject*> ExternalRoots;
    TMap<UObject*, FGCReferenceEdge> LastReferenceEdges;
    bool               bIsCollecting = false;
};
