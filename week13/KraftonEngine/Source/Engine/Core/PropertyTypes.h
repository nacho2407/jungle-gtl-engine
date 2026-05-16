#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include "Core/CoreTypes.h"

namespace json { class JSON; }
class UClass;

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
	ObjectRef,	 // 일반 UObject 자식 에셋 레퍼런스. AssetTypeName 으로 FAssetRegistry 조회.
	ClassRef,	  // TSubclassOf<T> 의 UClass* 슬롯. ClassBase 의 자식 콤보로 노출/직렬화.
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

	// ObjectRef Metadata — 드롭다운에 표시할 자산 종류 (FAssetRegistry::ListByTypeName 의 키).
	// 예: "USkeletalMesh", "UStaticMesh", "UAnimSequence".
	const char* AssetTypeName = nullptr;

	// ClassRef Metadata — 자식 enumerate 의 베이스. 보통 TSubclassOf<T>::StaticClass() 로 채움.
	// 베이스 자신은 콤보에서 제외 (factory 미등록 가능 + 추상 의미).
	UClass* ClassBase = nullptr;

	// JSON 직렬화 — FSceneSaveManager 등 외부 직렬자가 호출.
	// 헤더에 SimpleJSON 의존을 들이지 않기 위해 본문은 PropertyTypes.cpp 에 둔다.
	json::JSON Serialize() const;
	void	   Deserialize(json::JSON& Value);
};
