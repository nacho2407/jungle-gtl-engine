#pragma once

#include "Core/CoreTypes.h"
#include "Core/Containers/Array.h"
#include "Core/Containers/String.h"

#include <algorithm>
#include <cstddef>
#include <utility>

class UObject;
class UClass;

class FReferenceCollector
{
public:
	void AddReferencedObject(UObject* Object)
	{
		if (Object)
		{
			ReferencedObjects.push_back(Object);
		}
	}

	const TArray<UObject*>& GetReferencedObjects() const { return ReferencedObjects; }

private:
	TArray<UObject*> ReferencedObjects;
};

class FSoftReferenceCollector
{
public:
	struct FSoftObjectPath
	{
		FString Path;
		UClass* ExpectedClass = nullptr;
	};

	void AddSoftObjectPath(const FString& Path, UClass* ExpectedClass)
	{
		if (!Path.empty())
		{
			SoftObjectPaths.push_back({ Path, ExpectedClass });
		}
	}

	const TArray<FSoftObjectPath>& GetSoftObjectPaths() const { return SoftObjectPaths; }

private:
	TArray<FSoftObjectPath> SoftObjectPaths;
};

struct FDuplicateContext
{
	TArray<std::pair<const UObject*, UObject*>> ObjectMap;

	void Add(const UObject* Original, UObject* Duplicate)
	{
		if (Original && Duplicate)
		{
			ObjectMap.push_back({ Original, Duplicate });
		}
	}

	UObject* ResolveDuplicatedObject(const UObject* Original) const
	{
		for (const auto& Pair : ObjectMap)
		{
			if (Pair.first == Original)
			{
				return Pair.second;
			}
		}
		return nullptr;
	}
};

template <typename T>
class TObjectPtr
{
public:
	TObjectPtr() = default;
	TObjectPtr(std::nullptr_t) {}
	TObjectPtr(T* InPtr)
		: Ptr(InPtr)
	{
	}

	T* Get() const { return Ptr; }
	void Set(T* InPtr) { Ptr = InPtr; }

	T* operator->() const { return Ptr; }
	operator T*() const { return Ptr; }
	explicit operator bool() const { return Ptr != nullptr; }

	TObjectPtr& operator=(T* InPtr)
	{
		Set(InPtr);
		return *this;
	}

	bool IsValid() const { return Ptr != nullptr; }

private:
	T* Ptr = nullptr;
};

template <typename T>
class TSoftObjectPtr
{
public:
	TSoftObjectPtr() = default;
	explicit TSoftObjectPtr(const FString& InPath)
		: Path(InPath)
	{
	}

	const FString& GetPath() const { return Path; }

	void SetPath(const FString& InPath)
	{
		Path = InPath;
		Cached = nullptr;
	}

	T* Get() const { return Cached; }
	bool IsNull() const { return Path.empty(); }

private:
	FString Path;
	mutable T* Cached = nullptr;
};

struct IObjectPtrOps
{
	virtual ~IObjectPtrOps() = default;

	virtual UObject* GetObject(const void* ValuePtr) const = 0;
	virtual void SetObject(void* ValuePtr, UObject* Object) const = 0;
	virtual void VisitReference(FReferenceCollector& Collector, void* ValuePtr) const = 0;
};

struct ISoftObjectPtrOps
{
	virtual ~ISoftObjectPtrOps() = default;

	virtual FString GetPath(const void* ValuePtr) const = 0;
	virtual void SetPath(void* ValuePtr, const FString& Path) const = 0;
	virtual void VisitSoftReference(FSoftReferenceCollector& Collector, void* ValuePtr, UClass* ExpectedClass) const = 0;
};

struct IArrayPropertyOps
{
	virtual ~IArrayPropertyOps() = default;

	virtual int32 Num(const void* ArrayPtr) const = 0;
	virtual void Resize(void* ArrayPtr, int32 NewNum) const = 0;
	virtual void* GetElementPtr(void* ArrayPtr, int32 Index) const = 0;
	virtual const void* GetElementPtr(const void* ArrayPtr, int32 Index) const = 0;
	virtual void AddDefaulted(void* ArrayPtr) const = 0;
	virtual void RemoveAt(void* ArrayPtr, int32 Index) const = 0;
};

template <typename T>
struct TRawObjectPtrOps : IObjectPtrOps
{
	UObject* GetObject(const void* ValuePtr) const override
	{
		if (!ValuePtr)
		{
			return nullptr;
		}
		return static_cast<UObject*>(*static_cast<T* const*>(ValuePtr));
	}

	void SetObject(void* ValuePtr, UObject* Object) const override
	{
		if (ValuePtr)
		{
			*static_cast<T**>(ValuePtr) = static_cast<T*>(Object);
		}
	}

	void VisitReference(FReferenceCollector& Collector, void* ValuePtr) const override
	{
		Collector.AddReferencedObject(GetObject(ValuePtr));
	}
};

template <typename T>
struct TObjectPtrOps : IObjectPtrOps
{
	UObject* GetObject(const void* ValuePtr) const override
	{
		const TObjectPtr<T>* Ptr = static_cast<const TObjectPtr<T>*>(ValuePtr);
		return Ptr ? static_cast<UObject*>(Ptr->Get()) : nullptr;
	}

	void SetObject(void* ValuePtr, UObject* Object) const override
	{
		if (TObjectPtr<T>* Ptr = static_cast<TObjectPtr<T>*>(ValuePtr))
		{
			Ptr->Set(static_cast<T*>(Object));
		}
	}

	void VisitReference(FReferenceCollector& Collector, void* ValuePtr) const override
	{
		Collector.AddReferencedObject(GetObject(ValuePtr));
	}
};

template <typename T>
struct TSoftObjectPtrOps : ISoftObjectPtrOps
{
	FString GetPath(const void* ValuePtr) const override
	{
		const TSoftObjectPtr<T>* Ptr = static_cast<const TSoftObjectPtr<T>*>(ValuePtr);
		return Ptr ? Ptr->GetPath() : FString();
	}

	void SetPath(void* ValuePtr, const FString& Path) const override
	{
		if (TSoftObjectPtr<T>* Ptr = static_cast<TSoftObjectPtr<T>*>(ValuePtr))
		{
			Ptr->SetPath(Path);
		}
	}

	void VisitSoftReference(FSoftReferenceCollector& Collector, void* ValuePtr, UClass* ExpectedClass) const override
	{
		Collector.AddSoftObjectPath(GetPath(ValuePtr), ExpectedClass);
	}
};

template <typename ElementType>
struct TArrayPropertyOps : IArrayPropertyOps
{
	using ArrayType = TArray<ElementType>;

	int32 Num(const void* ArrayPtr) const override
	{
		const ArrayType* Array = static_cast<const ArrayType*>(ArrayPtr);
		return Array ? static_cast<int32>(Array->size()) : 0;
	}

	void Resize(void* ArrayPtr, int32 NewNum) const override
	{
		if (ArrayType* Array = static_cast<ArrayType*>(ArrayPtr))
		{
			Array->resize(std::max(NewNum, 0));
		}
	}

	void* GetElementPtr(void* ArrayPtr, int32 Index) const override
	{
		ArrayType* Array = static_cast<ArrayType*>(ArrayPtr);
		if (!Array || Index < 0 || Index >= static_cast<int32>(Array->size()))
		{
			return nullptr;
		}
		return &(*Array)[Index];
	}

	const void* GetElementPtr(const void* ArrayPtr, int32 Index) const override
	{
		const ArrayType* Array = static_cast<const ArrayType*>(ArrayPtr);
		if (!Array || Index < 0 || Index >= static_cast<int32>(Array->size()))
		{
			return nullptr;
		}
		return &(*Array)[Index];
	}

	void AddDefaulted(void* ArrayPtr) const override
	{
		if (ArrayType* Array = static_cast<ArrayType*>(ArrayPtr))
		{
			Array->emplace_back();
		}
	}

	void RemoveAt(void* ArrayPtr, int32 Index) const override
	{
		ArrayType* Array = static_cast<ArrayType*>(ArrayPtr);
		if (!Array || Index < 0 || Index >= static_cast<int32>(Array->size()))
		{
			return;
		}
		Array->erase(Array->begin() + Index);
	}
};

template <typename T>
const IObjectPtrOps* GetRawObjectPtrOps()
{
	static const TRawObjectPtrOps<T> Ops;
	return &Ops;
}

template <typename T>
const IObjectPtrOps* GetTObjectPtrOps()
{
	static const TObjectPtrOps<T> Ops;
	return &Ops;
}

template <typename T>
const ISoftObjectPtrOps* GetSoftObjectPtrOps()
{
	static const TSoftObjectPtrOps<T> Ops;
	return &Ops;
}

template <typename T>
const IArrayPropertyOps* GetArrayPropertyOps()
{
	static const TArrayPropertyOps<T> Ops;
	return &Ops;
}
