#pragma once

class UClass;
class UScriptStruct;

// C++ 컴파일 시에는 대부분 아무 기능도 하지 않는 Python 파서 전용 마커입니다.
// GENERATED_BODY는 런타임 UClass 진입점을 선언합니다.
#define UCLASS(...)
#define UPROPERTY(...)
#define UENUM(...)
#define UMETA(...)
#define USTRUCT(...)
#define UFUNCTION(...)
#define UDELEGATE(...)
#define UINTERFACE(...)

#define GENERATED_BODY(ClassName, ParentClass) \
public: \
	using ThisClass = ClassName; \
	using Super = ParentClass; \
	static UClass* StaticClass(); \
	virtual UClass* GetClass() const override { return StaticClass(); } \
	friend struct Z_Construct_UClass_##ClassName;

#define GENERATED_STRUCT_BODY(StructName) \
public: \
	static const UScriptStruct* StaticStruct(); \
	friend struct Z_Construct_UScriptStruct_##StructName;
