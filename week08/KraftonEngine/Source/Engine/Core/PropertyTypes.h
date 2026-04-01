#pragma once

#include <cstdint>
#include <vector>
#include <string>

// 에디터에서 자동 위젯 매핑에 사용되는 프로퍼티 타입
enum class EPropertyType : uint8_t
{
	Bool,
	ByteBool, // uint8을 bool처럼 사용 (std::vector<bool> 회피용)
	Int,
	Float,
	Vec3,
	Vec4,
	String,
	Name,		  // FName — 문자열 풀 기반 이름 (리소스 키 등)
	StaticMeshRef, // UStaticMesh* 에셋 레퍼런스 (드롭다운 선택)
	MaterialSlot,  // FMaterialSlot — 머티리얼 경로 + UVScroll 플래그 묶음
	// 필요 시 Enum, Color 등 추가
};

// 머티리얼 슬롯: 경로와 UVScroll 여부를 하나의 단위로 관리
struct FMaterialSlot
{
	std::string Path;
	uint8_t     bUVScroll = 0;
};

// 컴포넌트가 노출하는 편집 가능한 프로퍼티 디스크립터
struct FPropertyDescriptor
{
	std::string   Name;
	EPropertyType Type;
	void*         ValuePtr;

	// float 범위 힌트 (DragFloat 등에서 사용)
	float Min   = 0.0f;
	float Max   = 0.0f;
	float Speed = 0.1f;
};
