#pragma once

#include "Object/Object.h"

extern TArray<UObject*> GUObjectArray;

// 특정 UObject 타입 순회
template<typename TObject>
class TObjectIterator
{
public:
	TObjectIterator()
		: CurrentIndex(0)
	{
		AdvanceToNextValidObject();
	}

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

// 모든 UObject 순회 (타입 필터 없음)
class FObjectIterator
{
public:
	FObjectIterator()
		: CurrentIndex(0)
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
			if (GUObjectArray[CurrentIndex] != nullptr)
			{
				return;
			}
			++CurrentIndex;
		}
	}

	int32 CurrentIndex;
};
