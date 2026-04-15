#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include "CoreTypes.h"      // int32, uint8, …
#include "Math/Vector.h"    // FVector  (for sizeof in GetPropertySize)
#include "Math/Vector4.h"   // FVector4 (for sizeof in GetPropertySize)

// 에디터에서 자동 위젯 매핑에 사용되는 프로퍼티 타입
enum class EPropertyType : uint8_t
{
    Bool,
    Int,
    Float,
    Vec3,
    Vec4,
    String,
    Name,              // FName — 문자열 풀 기반 이름 (리소스 키 등)
    SceneComponentRef, // USceneComponent* 변수의 주소 (MovementComponent를 위한 Enum값
    Vec3Array,         // TArray<FVector>* - variable-length array of FVector
                       // 필요 시 Enum, Color 등 추가
	Enum,
	Color,
};

// 컴포넌트가 노출하는 편집 가능한 프로퍼티 디스크립터
struct FPropertyDescriptor
{
    const char*   Name;
    EPropertyType Type;
    void*         ValuePtr;

    // float 범위 힌트 (DragFloat 등에서 사용)
    float Min	= 0.0f;
    float Max	= 0.0f;
    float Speed = 0.1f;

	// Enum Metadata
	const char** EnumNames  = nullptr;
	uint32		 EnumCount  = 0;
};

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
    // String, Name 은 ValuePtr 기반 특수 처리
    case EPropertyType::String: return 0;
    case EPropertyType::Name:   return 0;
    // 포인터 — Duplicate 호출 측에서 직접 처리
    case EPropertyType::SceneComponentRef: return 0;
    default: return 0;
    }
}