#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include "CoreTypes.h"      // int32, uint8, …
#include "Math/Vector.h"    // FVector  (for sizeof in GetPropertySize)
#include "Math/Vector4.h"   // FVector4 (for sizeof in GetPropertySize)
#include "Math/Quat.h"
#include "Core/Guid.h"

struct ID3D11ShaderResourceView;

// 에디터에서 자동 위젯 매핑에 사용되는 프로퍼티 타입
enum class EPropertyType : uint8_t
{
	Unknown,
	Bool,
	Int,
	Float,
	Color,
	Vec3,
	Vec4,
	Guid,
	Quat,

	// String, Name
	String,
	Name,

	// Pointer
	SceneComponentRef,

	// Array
	Vec3Array,         // TArray<FVector>* - variable-length array of FVector
	StringArray,       // TArray<FString>* - variable-length array of FString

	// Enum MetaData
	Enum,

	// Asset
	Material, // TArray<UMaterialInterface*>
	SRV,      // FSRVPropertyData
	CubeSRV,  // FCubeSRVPropertyData
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

// SRV 정보
struct FSRVDisplayInfo
{
	float ImageWidth  = 256.f;
	float ImageHeight = 256.f;
	float UV0X = 0.f;
	float UV0Y = 0.f;
	float UV1X = 1.f;
	float UV1Y = 1.f;
};

// 단일 Shader Resource View를 Details 패널에서 read-only preview로 표시하기 위한 wrapper입니다.
// ValuePtr는 항상 FSRVPropertyData 멤버/임시 데이터의 주소를 가리킵니다.
struct FSRVPropertyData
{
	ID3D11ShaderResourceView* SRV = nullptr;
	FSRVDisplayInfo DisplayInfo;
};

// Cube shadow map처럼 6개 face SRV를 Details 패널에서 read-only preview로 표시하기 위한 wrapper입니다.
// FaceSRVs 배열 자체가 wrapper 안에 있으므로 generated reflection과 수동 descriptor 모두 같은 포인터 규칙을 사용합니다.
struct FCubeSRVPropertyData
{
	ID3D11ShaderResourceView* FaceSRVs[6] = {};
	FSRVDisplayInfo DisplayInfo = { 64.f, 64.f, 0.f, 0.f, 1.f, 1.f };
};

struct UEnum;
using FEnumMetaData = UEnum;

/** 각 프로퍼티의 Size 값을 반환합니다. 0을 반환하는 경우 특수 케이스입니다.
 * 이런 경우에는 CopyPropertiesFrom 함수 내에서 알아서 잘 처리해줄 수 있어야 합니다. 
 **/
inline size_t GetPropertySize(EPropertyType Type)
{
	switch (Type)
	{
	case EPropertyType::Bool:   return sizeof(bool);
	case EPropertyType::Int:    return sizeof(int32);
	case EPropertyType::Float:  return sizeof(float);
	case EPropertyType::Vec3:   return sizeof(FVector);
	case EPropertyType::Color:	return sizeof(FColor);
	case EPropertyType::Vec4:   return sizeof(FVector4);
	case EPropertyType::Guid:   return sizeof(FGuid);
	case EPropertyType::Quat:   return sizeof(FQuat);
	// String, Name 은 ValuePtr 기반 특수 처리
	case EPropertyType::String: return 0;
	case EPropertyType::Name:   return 0;
	// 포인터 — Duplicate 호출 측에서 직접 처리
	case EPropertyType::SceneComponentRef: return 0;
	default: return 0;
	}
}
