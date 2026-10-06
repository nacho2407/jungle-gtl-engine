#pragma once

#include <cstddef>

class UObject;
inline bool IsValid(const UObject* Object);

template<typename T>
class TObjectPtr
{
public:
	TObjectPtr() = default;
	TObjectPtr(T* InObject)
		: Object(InObject)
	{
	}

	T* Get() const { return Object; }
	T* GetRaw() const { return Object; }
	T* GetUnsafe() const { return Object; }

	T* GetValid() const
	{
		return ::IsValid(reinterpret_cast<const UObject*>(Object)) ? Object : nullptr;
	}

	bool IsValid() const { return GetValid() != nullptr; }

	void Reset() { Object = nullptr; }

	T* operator->() const { return Object; }
	T& operator*() const { return *Object; }
	operator T*() const { return Object; }

	TObjectPtr& operator=(T* InObject)
	{
		Object = InObject;
		return *this;
	}

private:
	T* Object = nullptr;
};
