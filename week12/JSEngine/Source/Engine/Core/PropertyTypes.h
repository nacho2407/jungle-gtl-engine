#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include "CoreTypes.h"

// 에디터에서 자동 위젯 매핑에 사용되는 프로퍼티 타입.
// 수학 값 타입(FVector/FColor/FQuat/FGuid 등)은 모두 Struct + ScriptStruct + EditorHint로 표현합니다.
enum class EPropertyType : uint8_t
{
	Unknown,
	Bool,
	Int,
	Float,

	String,
	Name,

	Enum,

	ObjectPtr,
	SoftObjectPtr,
	Array,
	Struct,
};

enum class EObjectReferenceKind : uint8_t
{
	None,
	RuntimeObject,
	ActorComponent,
	Asset,
};

enum class EPropertyUsageFlags : uint8_t
{
	None = 0,
	Editable = 1 << 0,
	Animatable = 1 << 1,
};

constexpr EPropertyUsageFlags operator|(EPropertyUsageFlags Lhs, EPropertyUsageFlags Rhs)
{
	return static_cast<EPropertyUsageFlags>(
		static_cast<uint8_t>(Lhs) | static_cast<uint8_t>(Rhs));
}

constexpr bool HasPropertyUsage(EPropertyUsageFlags Value, EPropertyUsageFlags Flag)
{
	return (static_cast<uint8_t>(Value) & static_cast<uint8_t>(Flag)) != 0;
}

enum class EPropertyChangeType : uint8_t
{
	ValueSet,
	Interactive,
	Preview,
	Runtime,
};

struct FPropertyChangedEvent
{
	const char* PropertyName = nullptr;
	EPropertyChangeType ChangeType = EPropertyChangeType::ValueSet;
};

class UEnum;
using FEnumMetaData = UEnum;

inline size_t GetPropertySize(EPropertyType Type)
{
	switch (Type)
	{
	case EPropertyType::Bool:  return sizeof(bool);
	case EPropertyType::Int:   return sizeof(int32);
	case EPropertyType::Float: return sizeof(float);
	default: return 0;
	}
}
