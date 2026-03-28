#pragma once

#include "Core/CoreMinimal.h"

#include "CoreUObject/Object.h"
#include "CoreUObject/UObjectArray.h"

/**
 * @brief Traversal + type filtering까지 담당(UE 스타일)
 */
class ENGINE_API FUObjectIteratorBase
{
  public:
    FUObjectIteratorBase(const void* InClass);
    virtual ~FUObjectIteratorBase() = default;

    explicit operator bool() const { return CurrentObject != nullptr; }

    FUObjectIteratorBase& operator ++();
    FUObjectIteratorBase  operator ++(int);

    UObject* operator *() const { return CurrentObject; }
    UObject* operator ->() const { return CurrentObject; }

    UObject* GetObject() const { return CurrentObject; }

  protected:
    virtual void Advance(void);

  protected:
    int32 Index = -1;
    UObject* CurrentObject = nullptr;
    const void* ClassToIterate = nullptr;
};

/**
 * @brief Base iterator의 class to iterate만 결정
 */
template <typename T>
class TUObjectIterator: public FUObjectIteratorBase
{
  public:
    TUObjectIterator(): FUObjectIteratorBase(T::GetClass()) {}
    ~TUObjectIterator() = default;

    TUObjectIterator<T>& operator ++()
    {
        Advance();
        return *this;
    }

    TUObjectIterator<T> operator ++(int)
    {
        TUObjectIterator<T> Temp = *this;
        Advance();
        return Temp;
    }

    T* operator *() const { return Cast<T>(CurrentObject); }
    T* operator ->() const { return Cast<T>(CurrentObject); }

    T* GetObject() const { return Cast<T>(CurrentObject); }
};

/**
 * @brief 별도의 타입 검사가 필요 없는 UObject는 특수화
 */
template<>
class ENGINE_API TUObjectIterator<UObject>: public FUObjectIteratorBase
{
  public:
    TUObjectIterator(): FUObjectIteratorBase(UObject::GetClass()) {}
    ~TUObjectIterator() = default;

    TUObjectIterator<UObject>& operator++()
    {
        Advance();
        return *this;
    }

    TUObjectIterator<UObject> operator++(int)
    {
        TUObjectIterator<UObject> Temp = *this;
        Advance();
        return Temp;
    }

  protected:
    void Advance(void) override;
};
