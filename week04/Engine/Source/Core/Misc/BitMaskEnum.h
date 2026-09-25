#pragma once

#include "Core/HAL/PlatformTypes.h"
#include <type_traits>

template <typename T> struct TEnableBitMaskOperators
{
    static constexpr bool bEnabled = false;
};

template <typename T>
concept BitMaskEnum = std::is_enum_v<T> && TEnableBitMaskOperators<T>::bEnabled;

template <BitMaskEnum T> inline T operator|(T Lhs, T Rhs)
{
    using U = std::underlying_type_t<T>;
    return static_cast<T>(static_cast<U>(Lhs) | static_cast<U>(Rhs));
}

template <BitMaskEnum T> inline T operator&(T Lhs, T Rhs)
{
    using U = std::underlying_type_t<T>;
    return static_cast<T>(static_cast<U>(Lhs) & static_cast<U>(Rhs));
}

template <BitMaskEnum T> inline T operator~(T Value)
{
    using U = std::underlying_type_t<T>;
    return static_cast<T>(~static_cast<U>(Value));
}

template <BitMaskEnum T> inline T& operator|=(T& Lhs, T Rhs)
{
    Lhs = Lhs | Rhs;
    return Lhs;
}

template <BitMaskEnum T> inline T& operator&=(T& Lhs, T Rhs)
{
    Lhs = Lhs & Rhs;
    return Lhs;
}

template <BitMaskEnum T> inline bool IsFlagSet(T Value, T Flags)
{
    using U = std::underlying_type_t<T>;
    return (static_cast<U>(Value) & static_cast<U>(Flags)) == static_cast<U>(Flags);
}

template <BitMaskEnum T> inline void SetFlag(T& Value, T Flag, bool bEnabled)
{
    using U = std::underlying_type_t<T>;

    if (bEnabled)
    {
        Value |= Flag;
    }
    else
    {
        Value = static_cast<T>(static_cast<U>(Value) & ~static_cast<U>(Flag));
    }
}