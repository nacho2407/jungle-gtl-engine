#pragma once

// ============================================================
//  ReflectionFunctionInfo.h
//  함수·파라미터 메타데이터 (FParameterInfo, FFunctionInfo) 정의.
//
//  FParameterInfo 는 FClassInfo*, FStructInfo*, FEnumInfo* 를
//  포인터로만 보관하므로 전방선언만으로 충분합니다.
//  → 순환 include 없이 각 Info 헤더와 독립적으로 존재합니다.
// ============================================================

#include "Core/CoreTypes.h"
#include "Core/Containers/String.h"
#include "Core/Containers/Array.h"
#include "Object/FName.h"
#include "ReflectionFwd.h"   // FClassInfo*, FStructInfo*, FEnumInfo* 전방선언


enum EFunctionFlags : uint32
{
    FF_None = 0,
    FF_Callable = 1 << 0,
    FF_ScriptCallable = 1 << 1,	//Lua 자동 바인딩 대상.
    FF_CallInEditor = 1 << 2,	//Details 패널 버튼으로 호출 가능한 함수. 나중에 에디터 디버그용으로 좋음.
    FF_Event = 1 << 3,	//delegate/script event로 연결할 수 있는 함수 표시
    FF_Const = 1 << 4,	//const 함수 표시용.
    FF_Static = 1 << 5,//    static 함수면 owner object 없이 호출 가능 현재 파싱만
    FF_Exec = 1 << 6,	//콘솔 커맨드로 호출 가능한 함수. 나중에 디버그 콘솔이랑 붙이기 좋음
};

using FNativeFunctionInvoker = void(*)(UObject*);

// ------------------------------------------------------------------
// FParameterInfo  — 함수 인자 하나의 메타데이터
// ------------------------------------------------------------------
struct FParameterInfo
{
    FName        Name;
    FString      CPPType;

    // 포인터로만 참조 → 전방선언으로 충분, 헤더 include 불필요
    FClassInfo*  ClassInfo  = nullptr;
    FStructInfo* StructInfo = nullptr;
    FEnumInfo*   EnumInfo   = nullptr;

    bool bIsConst     = false;
    bool bIsReference = false;
    bool bIsPointer   = false;
};

// ------------------------------------------------------------------
// FFunctionInfo  — 함수 하나의 메타데이터
// ------------------------------------------------------------------
struct FFunctionInfo
{
    FName                    Name;
    FString                  ReturnType;
    TArray<FParameterInfo>   Parameters;   // 완전한 타입 필요 → 위에서 정의

    uint32 Flags = 0;
    FString Category;
    FString DisplayName;
    FNativeFunctionInvoker NativeInvoker = nullptr;
};
