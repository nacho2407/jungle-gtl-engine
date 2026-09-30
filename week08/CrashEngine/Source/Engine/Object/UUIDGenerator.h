// 오브젝트 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once
#include "Core/CoreTypes.h"

// UUIDGenerator는 오브젝트 영역의 핵심 동작을 담당합니다.
class UUIDGenerator
{
public:
    static uint32 GenUUID()
    {
        return NextUUID++;
    }

    static void ResetUUIDGeneration(int Value) { NextUUID = Value; }

private:
    static uint32 NextUUID;
};
