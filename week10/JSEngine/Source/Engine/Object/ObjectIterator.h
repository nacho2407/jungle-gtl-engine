#pragma once

#include "Object/Object.h" // GUObjectArray 및 UObject 참조를 위해 필요

template <typename T>
class TObjectIterator
{
public:
    TObjectIterator() : CurrentIndex(0) { AdvanceToNextValid(); }

    // Iterator가 유효한 객체를 가리키고 있는지 확인 (루프 조건문에 사용)
    explicit operator bool() const { return CurrentIndex < GUObjectArray.size(); }

	// 다음 유효한 객체 탐색
    TObjectIterator& operator++()
    {
        CurrentIndex++;
        AdvanceToNextValid();
        return *this;
    }
	
	// 역참조 연산자, 멤버 접근 연산자
    T* operator*() const { return static_cast<T*>(GUObjectArray[CurrentIndex]); }
    T* operator->() const { return static_cast<T*>(GUObjectArray[CurrentIndex]); }

private:
    // 배열을 순회하며 T로 캐스팅 가능한 유효한 객체를 찾을 때까지 인덱스를 증가시킨다.
    void AdvanceToNextValid()
    {
        while (CurrentIndex < GUObjectArray.size())
        {
            UObject* Obj = GUObjectArray[CurrentIndex];
            if (Obj && Obj->IsA<T>()) 
            {
                break;
            }
            CurrentIndex++;
        }
    }

    size_t CurrentIndex;
};