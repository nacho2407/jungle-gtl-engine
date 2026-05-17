#pragma once

#include <cstddef>
#include <cstdint>
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
	Struct,    // 자기기술 구조체 — StructType의 property metadata로 Children 생성
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

// 객체 인스턴스에 바인딩된 프로퍼티 값 뷰
struct FPropertyValue
{
	UObject* Object = nullptr;
	const FProperty* Property = nullptr;
	void* ContainerPtr = nullptr;

	void*	   GetValuePtr() const;
	void	   GetStructChildren(TArray<FPropertyValue>& OutProps) const;

	const char* GetName() const;
	const char* GetDisplayName() const;
	const char* GetCategory() const;
	EPropertyType GetType() const;
	float GetMin() const;
	float GetMax() const;
	float GetSpeed() const;
	const char** GetEnumNames() const;
	uint32 GetEnumCount() const;
	uint32 GetEnumSize() const;
	UStruct* GetStructType() const;
	const TMap<FString, FString>& GetMetadata() const;
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

	size_t Offset = 0;
	size_t Size = 0;

	float Min = 0.0f;	
	float Max = 0.0f;
	float Speed = 0.1f;	//에디터 드래그 입력 시 값 변화량

	const char** EnumNames = nullptr;	//콤보박스/드롭다운용 이름 배열
	uint32 EnumCount = 0;
	uint32 EnumSize = sizeof(int32);

	UStruct* StructType = nullptr;
	const char* DisplayName = nullptr;
	TMap<FString, FString> Metadata;
	const char* OwnerClassName = nullptr;

	inline void* GetValuePtrFor(void* Container) const
	{
		return Container ? reinterpret_cast<uint8*>(Container) + Offset : nullptr;
	}

	inline FPropertyValue ToValue(void* Container, UObject* Object = nullptr) const
	{
		FPropertyValue Desc;
		Desc.Object = Object;
		Desc.Property = this;
		Desc.ContainerPtr = Container;
		return Desc;
	}

	json::JSON Serialize(void* Container) const;
	void	   Deserialize(void* Container, json::JSON& Value) const;
	void	   Serialize(void* Container, FArchive& Ar) const;

	json::JSON Serialize(UObject* Object) const;
	void	   Deserialize(UObject* Object, json::JSON& Value) const;
	void	   Serialize(UObject* Object, FArchive& Ar) const;
};
