// 직렬화 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Core/CoreTypes.h" // TArray가 정의된 곳
#include "Object/FName.h"
#include <type_traits>
#include <string>

// FArchive는 데이터 저장과 복원을 위한 직렬화 흐름을 다룹니다.
class FArchive
{
protected:
    bool bIsLoading = false;
    bool bIsSaving = false;

public:
    virtual ~FArchive() = default;

    inline bool IsLoading() const { return bIsLoading; }
    inline bool IsSaving() const { return bIsSaving; }

    // 핵심 순수 가상 함수: 파생 클래스(Writer/Reader)가 구현해야 할 실제 입출력 로직
    // Data 포인터부터 Num 바이트만큼을 읽거나 씁니다.
    virtual void Serialize(void* Data, size_t Num) = 0;

    // ----------------------------------------------------
    // 마법의 연산자 오버로딩 (기본 자료형: int, float, 구조체 등)
    // ----------------------------------------------------
    template <typename T>
    FArchive& operator<<(T& Value)
    {
        // 포인터가 아닌 기본 데이터 타입이나 메모리 복사가 가능한 구조체만 허용
        static_assert(std::is_trivially_copyable<T>::value, "Error: T is not trivially copyable! You need a custom operator<< for this type.");
        this->Serialize(&Value, sizeof(T));
        return *this;
    }
};

inline FArchive& operator<<(FArchive& Ar, std::string& Str)
{
    uint32 Length = static_cast<uint32>(Str.size());
    Ar << Length;

    if (Ar.IsLoading())
        Str.resize(Length);
    if (Length > 0)
        Ar.Serialize(Str.data(), Length * sizeof(char));

    return Ar;
}

// FName은 풀 인덱스가 프로세스/환경마다 다르므로 문자열로 왕복.
inline FArchive& operator<<(FArchive& Ar, FName& Name)
{
    FString Str = Ar.IsSaving() ? Name.ToString() : FString();
    Ar << Str;
    if (Ar.IsLoading())
    {
        Name = FName(Str);
    }
    return Ar;
}

// ----------------------------------------------------
// 마법의 연산자 특수화 (TArray 지원)
// ----------------------------------------------------
template <typename T>
FArchive& operator<<(FArchive& Ar, TArray<T>& Array)
{
    // 1. 배열의 총 개수(Length)를 먼저 직렬화합니다.
    uint32 ArrayNum = static_cast<uint32>(Array.size());
    Ar << ArrayNum;

    if (Ar.IsLoading())
        Array.resize(ArrayNum);
    if (ArrayNum > 0)
    {
        // FVertexPNCT_T처럼 완벽한 숫자 덩어리일 때만 O(1) 고속 복사를 수행합니다.
        if constexpr (std::is_trivially_copyable<T>::value)
        {
            Ar.Serialize(Array.data(), ArrayNum * sizeof(T));
        }
        else
        {
            // FStaticMeshSection처럼 안에 FString이 들어있으면,
            // 느리더라도 반드시 한 요소씩 돌면서 안전하게(Deep Copy) 직렬화합니다.
            for (auto& Item : Array)
            {
                Ar << Item;
            }
        }
    }

    return Ar;
}
