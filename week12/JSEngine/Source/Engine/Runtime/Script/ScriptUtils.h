#pragma once
#include "ThirdParty/sol/sol.hpp"

// ============================================================
// Lua Binding Helper Macros
// ============================================================

// 생성자 없음. AActor, UObject, Component 계열에 추천.
#define LUA_BEGIN_TYPE_NO_CTOR(StatePtr, ClassType, LuaName) \
    {                                                        \
        using LuaClass = ClassType;                          \
        auto usertype = (StatePtr)->new_usertype<LuaClass>(LuaName, sol::no_constructor);

// 생성자 없음 + 베이스 클래스
#define LUA_BEGIN_TYPE_NO_CTOR_BASE(StatePtr, ClassType, LuaName, ...)          \
    {                                                                           \
        using LuaClass = ClassType;                                             \
        auto usertype = (StatePtr)->new_usertype<LuaClass>(LuaName,             \
                                                           sol::no_constructor, \
                                                           sol::base_classes, sol::bases<__VA_ARGS__>());

// 팩토리 생성자 있음. FVector 같은 값 타입에 추천.
#define LUA_BEGIN_TYPE_FACTORY(StatePtr, ClassType, LuaName, ...)                 \
    {                                                                             \
        using LuaClass = ClassType;                                               \
        auto usertype = (StatePtr)->new_usertype<LuaClass>(LuaName,               \
                                                           sol::call_constructor, \
                                                           sol::factories(__VA_ARGS__));

// 팩토리 생성자 있음 + 베이스 클래스
#define LUA_BEGIN_TYPE_FACTORY_BASE(StatePtr, ClassType, LuaName, ...)            \
    {                                                                             \
        using LuaClass = ClassType;                                               \
        auto usertype = (StatePtr)->new_usertype<LuaClass>(LuaName,               \
                                                           sol::call_constructor, \
                                                           sol::factories(__VA_ARGS__));

// 종료
#define LUA_END_TYPE() \
    }

// 멤버 변수
#define LUA_FIELD(LuaName, MemberName) \
    usertype[#LuaName] = &LuaClass::MemberName

// 멤버 함수
#define LUA_METHOD(LuaName, MethodName) \
    usertype[#LuaName] = &LuaClass::MethodName

// 자유 함수 / 람다 / 이미 만든 함수 포인터
#define LUA_SET(LuaName, Callable) \
    usertype[#LuaName] = Callable

// 읽기 전용 프로퍼티
#define LUA_RO_PROPERTY(LuaName, Getter) \
    usertype[#LuaName] = sol::property(&LuaClass::Getter)

// 읽기/쓰기 프로퍼티
#define LUA_RW_PROPERTY(LuaName, Getter, Setter) \
    usertype[#LuaName] = sol::property(&LuaClass::Getter, &LuaClass::Setter)

// 직접 property 지정. 람다 getter 같은 경우용.
#define LUA_PROPERTY(LuaName, ...) \
    usertype[#LuaName] = sol::property(__VA_ARGS__)

// 오버로드 멤버 함수
#define LUA_METHOD_OVERLOAD(LuaName, ReturnType, MethodName, ...) \
    usertype[#LuaName] = sol::resolve<ReturnType(__VA_ARGS__)>(&LuaClass::MethodName)

// const 오버로드 멤버 함수
#define LUA_METHOD_CONST_OVERLOAD(LuaName, ReturnType, MethodName, ...) \
    usertype[#LuaName] = sol::resolve<ReturnType(__VA_ARGS__) const>(&LuaClass::MethodName)

// sol::overload 직접 등록
#define LUA_OVERLOAD(LuaName, ...) \
    usertype[#LuaName] = sol::overload(__VA_ARGS__)

// 메타 함수 직접 등록
#define LUA_META(MetaFunc, Callable) \
    usertype[sol::meta_function::MetaFunc] = Callable

// const 연산자 오버로드 등록
#define LUA_META_CONST_OVERLOAD(MetaFunc, ReturnType, MethodName, ...) \
    usertype[sol::meta_function::MetaFunc] = sol::resolve<ReturnType(__VA_ARGS__) const>(&LuaClass::MethodName)

// 정적 값 반환
#define LUA_STATIC_VALUE(LuaName, Value) \
    usertype[#LuaName] = []() { return Value; }