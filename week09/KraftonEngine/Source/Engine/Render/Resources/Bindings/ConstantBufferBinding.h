// 렌더 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Core/CoreTypes.h"

class FConstantBuffer;

/*
    드로우 커맨드가 임시 상수 버퍼 데이터를 안전하게 보관하기 위한 바인딩 헬퍼입니다.
    작은 크기의 상수 버퍼 데이터를 인라인 저장소에 담아 제출 단계에서 바로 업로드합니다.
*/
struct FCBBindingEntry
{
    FConstantBuffer* Buffer = nullptr;
    uint32           Size   = 0;
    uint32           Slot   = 0;

    static constexpr size_t kMaxDataSize = 128;
    alignas(16) uint8 Data[kMaxDataSize] = {};

    template <typename T>
    T& Bind(FConstantBuffer* InBuffer, uint32 InSlot)
    {
        static_assert(sizeof(T) <= kMaxDataSize, "CB data exceeds inline buffer size");
        Buffer = InBuffer;
        Size   = sizeof(T);
        Slot   = InSlot;
        return *reinterpret_cast<T*>(Data);
    }

    template <typename T>
    T& As()
    {
        static_assert(sizeof(T) <= kMaxDataSize, "CB data exceeds inline buffer size");
        return *reinterpret_cast<T*>(Data);
    }

    template <typename T>
    const T& As() const
    {
        static_assert(sizeof(T) <= kMaxDataSize, "CB data exceeds inline buffer size");
        return *reinterpret_cast<const T*>(Data);
    }
};
