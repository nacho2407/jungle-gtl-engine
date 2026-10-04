#pragma once

// ============================================================
//  ReflectionClassInfo.h
//  클래스 메타데이터 (FClassInfo) 정의.
//
//  의존성 (직접)  : ReflectionPropertyFlags.h, ReflectionFunctionInfo.h
//  전방선언 사용  : FProperty* (ReflectionFwd.h 경유)
//
//  FClassInfo::ParentClass 는 자기 자신을 가리키는 포인터이므로
//  순환 include 없이 포인터 멤버로 선언합니다.
// ============================================================

#include "Core/CoreTypes.h"
#include "Core/Containers/Array.h"
#include "Object/FName.h"
#include "ReflectionFwd.h"            // FProperty* 전방선언
#include "ReflectionPropertyFlags.h"  // FPropertyInfo
#include "ReflectionFunctionInfo.h"   // FFunctionInfo (→ FParameterInfo 포함)

// ------------------------------------------------------------------
// FClassInfo
// ------------------------------------------------------------------
struct FClassInfo
{
    FName       ClassName;
    FName       ParentClassName;
    FClassInfo* ParentClass = nullptr;   // 자기 참조 → 포인터로 OK

    TArray<FPropertyInfo>  Properties;
    TArray<FProperty*>     ReflectedProperties; // 전방선언으로 충분
    TArray<size_t>         GcPointerOffsets;
    TArray<FFunctionInfo>  Functions;
};
