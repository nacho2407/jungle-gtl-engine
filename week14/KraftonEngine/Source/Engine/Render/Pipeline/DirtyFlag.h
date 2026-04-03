#pragma once

#include "Core/CoreTypes.h"

// ============================================================
// EDirtyFlag — 프록시 필드별 변경 추적 비트마스크
// ============================================================
enum class EDirtyFlag : uint32
{
	None       = 0,
	Transform  = 1 << 0,
	Material   = 1 << 1,
	Visibility = 1 << 2,
	Mesh       = 1 << 3,
	All        = 0xFFFFFFFF,
};

inline EDirtyFlag  operator|(EDirtyFlag A, EDirtyFlag B)  { return static_cast<EDirtyFlag>(static_cast<uint32>(A) | static_cast<uint32>(B)); }
inline EDirtyFlag  operator&(EDirtyFlag A, EDirtyFlag B)  { return static_cast<EDirtyFlag>(static_cast<uint32>(A) & static_cast<uint32>(B)); }
inline EDirtyFlag& operator|=(EDirtyFlag& A, EDirtyFlag B) { A = A | B; return A; }
inline EDirtyFlag& operator&=(EDirtyFlag& A, EDirtyFlag B) { A = A & B; return A; }
inline EDirtyFlag  operator~(EDirtyFlag A) { return static_cast<EDirtyFlag>(~static_cast<uint32>(A)); }
inline bool HasFlag(EDirtyFlag Flags, EDirtyFlag Test) { return (Flags & Test) != EDirtyFlag::None; }
