#pragma once

#include "Core/Types/CoreTypes.h"
#include "Core/Singleton.h"

class UObject;

class FReferenceCollector
{
public:
    void AddReferencedObject(UObject* Object);

    template<typename T>
    void AddReferencedObjects(const TArray<T*>& Objects)
    {
        for (T* Object : Objects)
        {
            AddReferencedObject(Object);
        }
    }

private:
    friend class FGarbageCollector;
    TArray<UObject*> Stack;
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

private:
    FGarbageCollector() = default;

    void MarkAllObjectsUnreachable();
    void MarkRoots();
    void Sweep();

private:
    TArray<FGCObject*> ExternalRoots;
    bool               bIsCollecting = false;
};
