#pragma once

#include "Core/CoreMinimal.h"


#include <cstdint>
#include <type_traits>

class FArchive
{
  public:
    virtual ~FArchive() = default;

    bool IsLoading() const { return bIsLoading; }
    bool IsSaving() const { return !bIsLoading; }

    virtual void Serialize(void* Data, size_t Size) = 0;

  protected:
    explicit FArchive(bool bInIsLoading)
        : bIsLoading(bInIsLoading)
    {
    }

  private:
    bool bIsLoading = false;
};

template <typename T>
    requires(std::is_trivially_copyable_v<T>)
inline FArchive& operator<<(FArchive& Ar, T& Value)
{
    Ar.Serialize(&Value, sizeof(T));
    return Ar;
}


template <typename T>
    requires(std::is_trivially_copyable_v<T>)
inline FArchive& operator<<(FArchive& Ar, const T& Value)
{
    // Saving-only overload for const data
    T& Mutable = const_cast<T&>(Value);
    Ar.Serialize(&Mutable, sizeof(T));
    return Ar;
}
inline FArchive& operator<<(FArchive& Ar, FString& Value)
{
    uint32 Size = 0;
    if (Ar.IsSaving())
    {
        Size = static_cast<uint32>(Value.size());
        Ar << Size;
        if (Size > 0)
        {
            Ar.Serialize(Value.data(), Size);
        }
    }
    else
    {
        Ar << Size;
        Value.resize(Size);
        if (Size > 0)
        {
            Ar.Serialize(Value.data(), Size);
        }
    }
    return Ar;
}

template <typename T>
    requires(std::is_trivially_copyable_v<T>)
inline void SerializeArray(FArchive& Ar, TArray<T>& Array)
{
    uint32 Count = 0;
    if (Ar.IsSaving())
    {
        Count = static_cast<uint32>(Array.size());
        Ar << Count;
        if (Count > 0)
        {
            Ar.Serialize(Array.data(), sizeof(T) * Count);
        }
    }
    else
    {
        Ar << Count;
        Array.resize(Count);
        if (Count > 0)
        {
            Ar.Serialize(Array.data(), sizeof(T) * Count);
        }
    }
}

// Raw array serialize helper for POD-like structs (manual responsibility).
template <typename T>
inline void SerializeArrayRaw(FArchive& Ar, TArray<T>& Array)
{
    uint32 Count = 0;
    if (Ar.IsSaving())
    {
        Count = static_cast<uint32>(Array.size());
        Ar << Count;
        if (Count > 0)
        {
            Ar.Serialize(Array.data(), sizeof(T) * Count);
        }
    }
    else
    {
        Ar << Count;
        Array.resize(Count);
        if (Count > 0)
        {
            Ar.Serialize(Array.data(), sizeof(T) * Count);
        }
    }
}
