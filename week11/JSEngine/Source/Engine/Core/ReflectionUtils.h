#pragma once
#include "Object/Object.h"

// 아무 객체나 오프셋만 알면 값을 읽고(Get), 쓸(Set) 수 있는 템플릿 함수

class ReflectionUtils
{
public:
    // 1. 값 쓰기 (Set)
    template <typename T>
    static void SetValue(UObject* TargetObject, size_t Offset, T NewValue)
    {
        if (!TargetObject)
            return;

        // 객체의 시작 주소 가져오기
        uint8* BaseAddress = reinterpret_cast<uint8*>(TargetObject);

        // 오프셋만큼 더해서 실제 변수 위치 찾기
        T* TargetPtr = reinterpret_cast<T*>(BaseAddress + Offset);

        // 값 덮어씌우기
        *TargetPtr = NewValue;
    }

    // 2. 값 읽기 (Get)
    template <typename T>
    static T GetValue(UObject* TargetObject, size_t Offset)
    {
        if (!TargetObject)
            return T(); // 기본값 반환

        uint8* BaseAddress = reinterpret_cast<uint8*>(TargetObject);
        T* TargetPtr = reinterpret_cast<T*>(BaseAddress + Offset);

        return *TargetPtr;
    }
};