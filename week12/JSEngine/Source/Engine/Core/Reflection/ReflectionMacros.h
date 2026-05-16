#pragma once

class UClass;
struct FTypeInfo;

// C++ 컴파일 시에는 대부분 아무 기능도 하지 않는 Python 파서 전용 마커입니다.
// GENERATED_BODY는 3단계 전환용으로 StaticClass/GetClass 선언을 제공합니다.
#define UCLASS(...)
#define UPROPERTY(...)
#define UENUM(...)
#define UMETA(...)

#define GENERATED_BODY(ClassName, ParentClass) \
public: \
	using ThisClass = ClassName; \
	using Super = ParentClass; \
	static UClass* StaticClass(); \
	virtual UClass* GetClass() const override { return StaticClass(); } \
	static const FTypeInfo s_TypeInfo; \
	virtual const FTypeInfo* GetTypeInfo() const override { return &s_TypeInfo; } \
	friend struct Z_Construct_UClass_##ClassName;
