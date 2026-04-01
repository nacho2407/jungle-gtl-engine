#pragma once

#include "Object.h"
#include <type_traits>

template <typename T>
class TObjectIterator
{
	static_assert(std::is_base_of_v<UObject, T>, "T must derive from UObject");

private:
	SIZE_T Index = 0;
	T* CurrentObject = nullptr;

	void Next()
	{
		CurrentObject = nullptr;
		while (Index < GUObjectArray.size())
		{
			UObject* Candidate = GUObjectArray[Index++];
			if (!Candidate) continue;

			if (Candidate->IsA<T>())
			{
				CurrentObject = static_cast<T*>(Candidate);
				return;
			}
		}
	}

public:
	TObjectIterator() { Next(); }

	explicit operator bool() const
	{
		return CurrentObject != nullptr;
	}

	TObjectIterator& operator++()
	{
		Next();
		return *this;
	}

	T& operator*() const
	{
		return *CurrentObject;
	}

	T* operator->() const
	{
		return CurrentObject;
	}

	T* Get() const
	{
		return CurrentObject;
	}
};