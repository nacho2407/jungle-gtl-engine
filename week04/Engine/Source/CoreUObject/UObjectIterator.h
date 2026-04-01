#pragma once

#include "Core/CoreMinimal.h"

#include "CoreUObject/Object.h"
#include "CoreUObject/UObjectArray.h"

/**
 * @brief Traversal + type filtering까지 담당(UE 스타일)
 */
class ENGINE_API FObjectIteratorBase
{
  public:
    FObjectIteratorBase(const void* InClass);
    virtual ~FObjectIteratorBase() = default;

    explicit operator bool() const { return CurrentObject != nullptr; }

    FObjectIteratorBase& operator ++();
    FObjectIteratorBase  operator ++(int);

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
class TObjectIterator: public FObjectIteratorBase
{
  public:
    TObjectIterator(): FObjectIteratorBase(T::GetClass()) {}
    ~TObjectIterator() = default;

    TObjectIterator<T>& operator ++()
    {
        Advance();
        return *this;
    }

    TObjectIterator<T> operator ++(int)
    {
        TObjectIterator<T> Temp = *this;
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
class ENGINE_API TObjectIterator<UObject>: public FObjectIteratorBase
{
  public:
    TObjectIterator(): FObjectIteratorBase(UObject::GetClass()) {}
    ~TObjectIterator() = default;

    TObjectIterator<UObject>& operator++()
    {
        Advance();
        return *this;
    }

    TObjectIterator<UObject> operator++(int)
    {
        TObjectIterator<UObject> Temp = *this;
        Advance();
        return Temp;
    }

  protected:
    void Advance(void) override;
};
