#pragma once

#include "Core/CoreTypes.h"

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
		CachedObject = nullptr;
	}

	void Reset()
	{
		ObjectPath.Reset();
		CachedObject = nullptr;
	}

	UObject* Get() const { return CachedObject; }
	void SetCachedObject(UObject* InObject) const { CachedObject = InObject; }

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
	FSoftObjectPath ObjectPath;
	mutable UObject* CachedObject = nullptr;
};
