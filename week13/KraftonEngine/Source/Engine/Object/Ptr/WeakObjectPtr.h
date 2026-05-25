#pragma once

#include "Object/Object.h"

template <typename T>
class TWeakObjectPtr
{
public:
    TWeakObjectPtr() = default;

    TWeakObjectPtr(T* InObject)
    {
        Reset(InObject);
    }

    void Reset(T* InObject = nullptr)
    {
        if (!IsAliveObject(InObject))
        {
            Object = nullptr;
            SerialNumber = 0;
            return;
        }

        Object = InObject;
        SerialNumber = InObject->GetSerialNumber();
    }

    T* Get() const
    {
        if (!IsAliveObject(Object))
        {
            return nullptr;
        }

        if (Object->GetSerialNumber() != SerialNumber)
        {
            return nullptr;
        }

        if (Object->HasAnyFlags(RF_PendingKill | RF_Garbage))
        {
            return nullptr;
        }

        return Object;
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

    T* operator->() const
    {
        return Get();
    }

    bool operator==(const T* Other) const
    {
        return Get() == Other;
    }

    bool operator!=(const T* Other) const
    {
        return Get() != Other;
    }

private:
    T*     Object = nullptr;
    uint32 SerialNumber = 0;
};
