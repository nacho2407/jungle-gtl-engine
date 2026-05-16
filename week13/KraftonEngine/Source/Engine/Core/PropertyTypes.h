#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include "Core/CoreTypes.h"

namespace json { class JSON; }
class FArchive;
class UStruct;

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
	MaterialSlotArray, // TArray<FMaterialSlot> — 메시 섹션별 머티리얼 경로
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

struct FPropertyValue;
struct FProperty;
class UObject;

// 구조체 자기기술 함수: 구조체 포인터로부터 하위 프로퍼티를 생성
using FStructPropertyFunc = void(*)(void* StructPtr, std::vector<FPropertyValue>& OutProps);

// 객체 인스턴스에 바인딩된 프로퍼티 값 뷰
struct FPropertyValue
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
	UStruct* StructType = nullptr;

	std::string   DisplayName;   // 에디터 표시명. 비어 있으면 Name 사용.
	TMap<FString, FString> Metadata;

	// JSON 직렬화 — FSceneSaveManager 등 외부 직렬자가 호출.
	// 헤더에 SimpleJSON 의존을 들이지 않기 위해 본문은 PropertyTypes.cpp 에 둔다.
	json::JSON Serialize() const;
	void	   Deserialize(json::JSON& Value);
	void	   Serialize(FArchive& Ar) const;
};

enum EPropertyFlags : uint32
{
	PF_None = 0,
	PF_Edit = 1 << 0,
	PF_Save = 1 << 1,
	PF_ReadOnly = 1 << 2,
	PF_Transient = 1 << 3, //저장, 로드에서 제외
};

enum class EPropertyChangeType : uint8
{
	ValueSet,
	Interactive,
	ArrayAdd,
	ArrayRemove,
	Duplicate,
	Load,
};

struct FPropertyChangedEvent
{
	UObject* Object = nullptr;
	const FProperty* Property = nullptr;
	const char* PropertyName = nullptr;
	const char* DisplayName = nullptr;
	EPropertyType Type = EPropertyType::Bool;
	EPropertyChangeType ChangeType = EPropertyChangeType::ValueSet;
	int32 ArrayIndex = -1;
};

struct FProperty
{
	const char* Name = nullptr;
	EPropertyType Type = EPropertyType::Bool;
	const char* Category = nullptr;
	uint32 Flags = PF_None;

	void* (*GetValuePtr)(void* Container) = nullptr;	//컨테이너 안에 있는 실제 프로퍼티 값의 주소를 반환

	float Min = 0.0f;	
	float Max = 0.0f;
	float Speed = 0.1f;	//에디터 드래그 입력 시 값 변화량

	const char** EnumNames = nullptr;	//콤보박스/드롭다운용 이름 배열
	uint32 EnumCount = 0;
	uint32 EnumSize = sizeof(int32);

	FStructPropertyFunc StructFunc = nullptr;
	UStruct* StructType = nullptr;
	const char* DisplayName = nullptr;
	TMap<FString, FString> Metadata;
	const char* OwnerClassName = nullptr;

	inline void* GetValuePtrFor(UObject* Object) const
	{
		return GetValuePtr ? GetValuePtr(Object) : nullptr;
	}

	inline FPropertyValue ToValue(UObject* Object) const
	{
		FPropertyValue Desc;
		Desc.Name = this->Name ? this->Name : "";
		Desc.Type = this->Type;
		Desc.Category = this->Category ? this->Category : "";
		Desc.DisplayName = this->DisplayName ? this->DisplayName : Desc.Name;
		Desc.Metadata = this->Metadata;
		Desc.ValuePtr = GetValuePtr ? GetValuePtr(Object) : nullptr;
		Desc.Min = this->Min;
		Desc.Max = this->Max;
		Desc.Speed = this->Speed;
		Desc.EnumNames = this->EnumNames;
		Desc.EnumCount = this->EnumCount;
		Desc.EnumSize = this->EnumSize;
		Desc.StructFunc = this->StructFunc;
		Desc.StructType = this->StructType;
		return Desc;
	}

	json::JSON Serialize(UObject* Object) const;
	void	   Deserialize(UObject* Object, json::JSON& Value) const;
	void	   Serialize(UObject* Object, FArchive& Ar) const;
};

struct FEditableProperty
{
	UObject* Object = nullptr;
	const FProperty* Property = nullptr;

	void* GetValuePtr() const
	{
		return Property ? Property->GetValuePtrFor(Object) : nullptr;
	}

	const char* GetName() const
	{
		return Property && Property->Name ? Property->Name : "";
	}

	const char* GetDisplayName() const
	{
		return Property && Property->DisplayName ? Property->DisplayName : GetName();
	}

	const char* GetCategory() const
	{
		return Property && Property->Category ? Property->Category : "";
	}
};
