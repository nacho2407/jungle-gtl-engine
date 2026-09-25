#include "CoreUObject/UObjectIterator.h"

FObjectIteratorBase::FObjectIteratorBase(const void* InClass)
{
    ClassToIterate = InClass;
    Advance();
}

FObjectIteratorBase& FObjectIteratorBase::operator++()
{
    Advance();
    return *this;
}

FObjectIteratorBase FObjectIteratorBase::operator++(int)
{
    FObjectIteratorBase Temp = *this;
    Advance();
    return Temp;
}

void FObjectIteratorBase::Advance(void)
{
    while (++Index < GUObjectArray.Num())
    {
        const FUObjectItem* Item = GUObjectArray.GetObjectItem(Index);

        if (Item == nullptr)
        {
            continue;
        }

        UObject* Object = Item->Object;

        if (Object == nullptr || !Object->IsValidLowLevel() || ClassToIterate == nullptr ||
            !Object->IsA(ClassToIterate))
        {
            continue;
        }

        CurrentObject = Object;

        return;
    }

    CurrentObject = nullptr;
}

void TObjectIterator<UObject>::Advance(void)
{
    while (++Index < GUObjectArray.Num())
    {
        const FUObjectItem* Item = GUObjectArray.GetObjectItem(Index);

        if (Item == nullptr)
        {
            continue;
        }

        UObject* Object = Item->Object;

        // 별도의 클래스 검사는 생략
        if (Object == nullptr || !Object->IsValidLowLevel())
        {
            continue;
        }

        CurrentObject = Object;

        return;
    }

    CurrentObject = nullptr;
}
