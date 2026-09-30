// 오브젝트 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Object/Object.h"

extern TArray<UObject*> GUObjectArray;

template <typename TObject>
// TObjectIterator는 오브젝트 영역의 핵심 동작을 담당합니다.
class TObjectIterator
{
public:
    TObjectIterator()
        : CurrentIndex(0)
    {
        AdvanceToNextValidObject();
    }

    // 다음 조건에 맞는 객체가 나올 때까지 계속 스킵
    TObjectIterator& operator++()
    {
        ++CurrentIndex;
        AdvanceToNextValidObject();
        return *this;
    }

    explicit operator bool() const
    {
        return CurrentIndex < static_cast<int32>(GUObjectArray.size());
    }

    TObject* operator*() const
    {
        return static_cast<TObject*>(GUObjectArray[CurrentIndex]);
    }

private:
    void AdvanceToNextValidObject()
    {
        while (CurrentIndex < static_cast<int32>(GUObjectArray.size()))
        {
            UObject* Obj = GUObjectArray[CurrentIndex];
            if (Obj && Obj->IsA<TObject>())
            {
                return;
            }
            ++CurrentIndex;
        }
    }

    int32 CurrentIndex;
};

// FObjectIterator는 오브젝트 영역의 핵심 동작을 담당합니다.
class FObjectIterator
{
public:
    FObjectIterator(UClass* InType = nullptr)
        : CurrentIndex(0), FilterType(InType)
    {
        AdvanceToNextValidObject();
    }

    FObjectIterator& operator++()
    {
        ++CurrentIndex;
        AdvanceToNextValidObject();
        return *this;
    }

    explicit operator bool() const
    {
        return CurrentIndex < static_cast<int32>(GUObjectArray.size());
    }

    UObject* operator*() const
    {
        return GUObjectArray[CurrentIndex];
    }

private:
    void AdvanceToNextValidObject()
    {
        while (CurrentIndex < static_cast<int32>(GUObjectArray.size()))
        {
            UObject* Obj = GUObjectArray[CurrentIndex];
            if (Obj != nullptr)
            {
                if (FilterType == nullptr || Obj->GetClass()->IsA(FilterType))
                {
                    return;
                }
            }
            ++CurrentIndex;
        }
    }

    int32 CurrentIndex;
    UClass* FilterType;
};
