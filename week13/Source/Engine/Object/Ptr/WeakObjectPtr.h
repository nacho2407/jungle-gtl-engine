#pragma once

#include "Object/Object.h"

// Weak UObject pointer with serial-number validation.
// The typed pointer is cached separately so Get()/operator-> can be used from
// headers that only forward declare T, while the UObject* is used for liveness
// and serial checks.
template <typename T>
class TWeakObjectPtr
{
public:
    TWeakObjectPtr() = default;

    TWeakObjectPtr(T* InObject)
    {
        Reset(InObject);
    }

    TWeakObjectPtr& operator=(T* InObject)
    {
        Reset(InObject);
        return *this;
    }

    void Reset(T* InObject = nullptr)
    {
        UObject* LiveObject = GetAliveObjectFromAddress(InObject);
        if (!LiveObject)
        {
            TypedObject = nullptr;
            Object = nullptr;
            SerialNumber = 0;
            return;
        }

        TypedObject = InObject;
        Object = LiveObject;
        SerialNumber = LiveObject->GetSerialNumber();
    }

    T* Get() const
    {
        UObject* LiveObject = GetAliveObjectFromAddress(Object);
        if (!LiveObject)
        {
            return nullptr;
        }

        if (LiveObject->GetSerialNumber() != SerialNumber)
        {
            return nullptr;
        }

        if (LiveObject->HasAnyFlags(RF_PendingKill | RF_Garbage))
        {
            return nullptr;
        }

        return TypedObject;
    }

    T* GetEvenIfPendingKill() const
    {
        UObject* LiveObject = GetAliveObjectFromAddress(Object);
        if (!LiveObject)
        {
            return nullptr;
        }

        if (LiveObject->GetSerialNumber() != SerialNumber)
        {
            return nullptr;
        }

        return TypedObject;
    }

    T* GetAlive() const
    {
        return GetEvenIfPendingKill();
    }

    bool IsValid() const
    {
        return Get() != nullptr;
    }

    bool bValid() const
    {
        return IsValid();
    }

    explicit operator bool() const
    {
        return IsValid();
    }

    operator T*() const
    {
        return Get();
    }

    T* operator->() const
    {
        return Get();
    }


private:
    T*      TypedObject = nullptr;
    UObject* Object = nullptr;
    uint32   SerialNumber = 0;
};
