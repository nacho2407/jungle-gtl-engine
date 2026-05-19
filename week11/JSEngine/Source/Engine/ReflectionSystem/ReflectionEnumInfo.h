#pragma once

// ============================================================
//  ReflectionEnumInfo.h
//  열거형 메타데이터 구조체 (FEnumValueInfo, FEnumInfo) 정의.
//
//  의존성 : CoreTypes, String, Array, FName
//  다른 리플렉션 구조체에 의존하지 않음 → 최하위 계층.
// ============================================================

#include "Core/CoreTypes.h"
#include "Core/Containers/String.h"
#include "Core/Containers/Array.h"
#include "Object/FName.h"

// ------------------------------------------------------------------
// 열거형 값 하나의 메타데이터
// ------------------------------------------------------------------
struct FEnumValueInfo
{
    FName  Name;
    int64  Value = 0;
};

// ------------------------------------------------------------------
// 열거형 전체 메타데이터
// ------------------------------------------------------------------
struct FEnumInfo
{
    FName                  EnumName;
    TArray<FEnumValueInfo> Values;
    TArray<const char*>    CachedNames;
};
