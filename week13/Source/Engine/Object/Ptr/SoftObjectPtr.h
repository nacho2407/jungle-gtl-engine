#pragma once

#include "WeakObjectPtr.h"
#include "Core/Types/CoreTypes.h"

class UObject;

struct FSoftObjectPath
{
	FString Path;

	FSoftObjectPath() = default;
	FSoftObjectPath(const FString& InPath)
		: Path(InPath)
	{
	}

	const FString& ToString() const { return Path; }
	bool IsNull() const { return Path.empty() || Path == "None"; }
	void Reset() { Path = "None"; }
};

class FSoftObjectPtr
{
public:
	FSoftObjectPtr() = default;
	FSoftObjectPtr(const FString& InPath)
		: ObjectPath(InPath)
	{
	}
	FSoftObjectPtr(const char* InPath)
		: ObjectPath(InPath ? FString(InPath) : FString("None"))
	{
	}
	FSoftObjectPtr(const FSoftObjectPath& InPath)
		: ObjectPath(InPath)
	{
	}

	const FSoftObjectPath& GetUniqueID() const { return ObjectPath; }
	const FString& ToString() const { return ObjectPath.ToString(); }
	bool IsNull() const { return ObjectPath.IsNull(); }
	bool empty() const { return ObjectPath.ToString().empty(); }

	void SetPath(const FString& InPath)
	{
		ObjectPath = FSoftObjectPath(InPath);
        CachedObject.Reset();
	}

	void Reset()
	{
		ObjectPath.Reset();
        CachedObject.Reset();
	}

    UObject* Get() const { return CachedObject.Get(); }
    void     SetCachedObject(UObject* InObject) const { CachedObject.Reset(InObject); }

	FSoftObjectPtr& operator=(const FString& InPath)
	{
		SetPath(InPath);
		return *this;
	}

	FSoftObjectPtr& operator=(const char* InPath)
	{
		SetPath(InPath ? FString(InPath) : FString("None"));
		return *this;
	}

	operator const FString&() const { return ToString(); }
	bool operator==(const FString& Other) const { return ToString() == Other; }
	bool operator!=(const FString& Other) const { return ToString() != Other; }
	bool operator==(const char* Other) const { return ToString() == (Other ? Other : ""); }
	bool operator!=(const char* Other) const { return ToString() != (Other ? Other : ""); }

private:
    FSoftObjectPath                 ObjectPath;
    mutable TWeakObjectPtr<UObject> CachedObject = nullptr;
};
