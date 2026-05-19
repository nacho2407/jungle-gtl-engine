#pragma once

// ============================================================
//  ReflectionFwd.h
//  리플렉션 시스템 전방선언 전용 헤더.
//  순환 include 차단을 위해 실제 정의는 각 전용 헤더에서 합니다.
// ============================================================

// --- TypeInfo 측 ---
//struct FPropertyInfo;
//struct FClassInfo;
//struct FStructInfo;
//struct FEnumValueInfo;
//struct FEnumInfo;

// --- ReflectedProperty 측 ---
class  FProperty;
class  FNumericProperty;
class  FObjectPropertyBase;
class  FObjectProperty;
class  FClassProperty;
class  FStructProperty;
class  FEnumProperty;
class  FArrayProperty;
class  FMapProperty;
class  FSetProperty;

// --- 기타 ---
struct FEditorPropertyDrawContext;
struct FReferenceCollector;
struct FScriptArrayOps;
