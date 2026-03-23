#pragma once

#include <cstdint>
#include <vector>
#include <string>

// 에디터에서 자동 위젯 매핑에 사용되는 프로퍼티 타입
enum class EPropertyType : uint8_t
{
	Bool,
	Int,
	Float,
	Vec3,
	Vec4,
	String,
	// 필요 시 Enum, Color 등 추가
};

// 컴포넌트가 노출하는 편집 가능한 프로퍼티 디스크립터
struct FPropertyDescriptor
{
	const char* Name;
	EPropertyType Type;
	void* ValuePtr;

	// float 범위 힌트 (DragFloat 등에서 사용)
	float Min = 0.0f;
	float Max = 0.0f;
	float Speed = 0.1f;
};
