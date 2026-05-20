#pragma once

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
	void Reset() { Object = nullptr; }

	T* operator->() const { return Object; }
	T& operator*() const { return *Object; }
	operator T*() const { return Object; }

	TObjectPtr& operator=(T* InObject)
	{
		Object = InObject;
		return *this;
	}

	bool operator==(const T* Other) const { return Object == Other; }
	bool operator!=(const T* Other) const { return Object != Other; }

private:
	T* Object = nullptr;
};
