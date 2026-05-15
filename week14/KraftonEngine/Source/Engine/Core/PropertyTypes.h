#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include "Core/CoreTypes.h"

namespace json { class JSON; }

// 에디터에서 자동 위젯 매핑에 사용되는 프로퍼티 타입
enum class EPropertyType : uint8_t
{
	Bool,
	ByteBool, // uint8을 bool처럼 사용 (std::vector<bool> 회피용)
	Int,
	Float,
	Vec3,
	Vec4,
	Rotator,	// FRotator (Pitch, Yaw, Roll)
	String,
	Name,		  // FName — 문자열 풀 기반 이름 (리소스 키 등)
	SceneComponentRef, // Owner actor 내부 USceneComponent 참조
	Color4,	   // FVector4 RGBA — ImGui::ColorEdit4 위젯
	StaticMeshRef, // UStaticMesh* 에셋 레퍼런스 (드롭다운 선택)
	SkeletalMeshRef, // USkeletalMesh* 에셋 레퍼런스 (드롭다운 선택)
	MaterialSlot,  // FMaterialSlot — 머티리얼 경로
	Enum,
	Vec3Array,
	Struct,    // 자기기술 구조체 — StructFunc로 Children 생성
	Script,
};

// 머티리얼 슬롯: 경로를 하나의 단위로 관리
struct FMaterialSlot
{
	std::string Path;
};

struct FPropertyDescriptor;
class UObject;

// 구조체 자기기술 함수: 구조체 포인터로부터 하위 프로퍼티를 생성
using FStructPropertyFunc = void(*)(void* StructPtr, std::vector<FPropertyDescriptor>& OutProps);

// 컴포넌트가 노출하는 편집 가능한 프로퍼티 디스크립터
struct FPropertyDescriptor
{
	std::string   Name;
	EPropertyType Type = EPropertyType::Bool;
	std::string   Category;      // 에디터 카테고리 (같은 문자열끼리 그룹화)
	void*         ValuePtr = nullptr;

	// float 범위 힌트 (DragFloat 등에서 사용)
	float Min   = 0.0f;
	float Max   = 0.0f;
	float Speed = 0.1f;

	// Enum Metadata
	const char** EnumNames = nullptr;
	uint32		 EnumCount = 0;
	uint32		 EnumSize  = sizeof(int32); // underlying type 크기 (uint8 enum은 1)

	// Struct Metadata
	FStructPropertyFunc StructFunc = nullptr;

	// JSON 직렬화 — FSceneSaveManager 등 외부 직렬자가 호출.
	// 헤더에 SimpleJSON 의존을 들이지 않기 위해 본문은 PropertyTypes.cpp 에 둔다.
	json::JSON Serialize() const;
	void	   Deserialize(json::JSON& Value);
};

enum EPropertyFlags : uint32
{
	PF_None = 0,
	PF_Edit = 1 << 0,
	PF_Save = 1 << 1,
	PF_ReadOnly = 1 << 2,
	PF_Transient = 1 << 3, //저장, 로드에서 제외
};

struct FProperty
{
	const char* Name = nullptr;
	EPropertyType Type = EPropertyType::Bool;
	const char* Category = nullptr;
	uint32 Flags = PF_None;

	void* (*GetValuePtr)(UObject* Object) = nullptr;	//객체 안에 있는 실제 프로퍼티 값의 주소를 반환

	float Min = 0.0f;	
	float Max = 0.0f;
	float Speed = 0.1f;	//에디터 드래그 입력 시 값 변화량

	const char** EnumNames = nullptr;	//콤보박스/드롭다운용 이름 배열
	uint32 EnumCount = 0;
	uint32 EnumSize = sizeof(int32);

	FStructPropertyFunc StructFunc = nullptr;
};