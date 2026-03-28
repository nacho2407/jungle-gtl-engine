#include "CoreUObject/UObjectIterator.h"

FUObjectIteratorBase::FUObjectIteratorBase(const void* InClass)
{
    ClassToIterate = InClass;
    Advance();
}

FUObjectIteratorBase& FUObjectIteratorBase::operator++()
{
    Advance();
    return *this;
}

FUObjectIteratorBase FUObjectIteratorBase::operator++(int)
{
    FUObjectIteratorBase Temp = *this;
    Advance();
    return Temp;
}

void FUObjectIteratorBase::Advance(void)
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

void TUObjectIterator<UObject>::Advance(void)
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
