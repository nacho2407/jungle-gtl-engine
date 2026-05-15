#pragma once

// C++ 컴파일 시에는 아무 기능도 하지 않는 Python 파서 전용 마커
#define UCLASS(...)
#define UPROPERTY(...)

// 기존 DECLARE_CLASS 래핑, 대체하여 사용
#define GENERATED_BODY(...) \
	static const FTypeInfo s_TypeInfo; \
	virtual const FTypeInfo* GetTypeInfo() const override { return &S_TypeInfo; }