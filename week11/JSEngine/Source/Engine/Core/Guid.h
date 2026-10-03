#pragma once

#include "Core/CoreTypes.h"
#include "Core/Containers/String.h"

struct FGuid
{
    uint32 A = 0;
    uint32 B = 0;
    uint32 C = 0;
    uint32 D = 0;

    FGuid() = default;
    FGuid(uint32 InA, uint32 InB, uint32 InC, uint32 InD)
        : A(InA), B(InB), C(InC), D(InD)
    {
    }

    bool IsValid() const;
    void Reset();

    FString ToString() const;

    static FGuid NewGuid();
    static bool Parse(const FString& Text, FGuid& OutGuid);
    static FGuid FromString(const FString& Text);

    bool operator==(const FGuid& Other) const;
    bool operator!=(const FGuid& Other) const;
};
