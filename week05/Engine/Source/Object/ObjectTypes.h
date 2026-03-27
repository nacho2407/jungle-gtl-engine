#pragma once
#include "Types/CoreTypes.h"

enum class EObjectFlags : uint32
{
    None = 0,
    Public = 1 << 0,
    Transient = 1 << 1,
    Standalone = 1 << 2,
    RootSet = 1 << 3,
    PendingKill = 1 << 4,
};

inline EObjectFlags operator|(EObjectFlags A, EObjectFlags B)
{
    return static_cast<EObjectFlags>(
        static_cast<uint32>(A) | static_cast<uint32>(B));
}

inline EObjectFlags operator&(EObjectFlags A, EObjectFlags B)
{
    return static_cast<EObjectFlags>(
        static_cast<uint32>(A) & static_cast<uint32>(B));
}

inline EObjectFlags& operator|=(EObjectFlags& A, EObjectFlags B)
{
    A = A | B;
    return A;
}

inline EObjectFlags& operator&=(EObjectFlags& A, EObjectFlags B)
{
    A = A & B;
    return A;
}

inline EObjectFlags operator~(EObjectFlags A)
{
    return static_cast<EObjectFlags>(~static_cast<uint32>(A));
}