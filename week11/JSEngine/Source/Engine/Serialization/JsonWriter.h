#pragma once

#include "Archive.h"
#include "Core/Containers/String.h"
#include "Core/Containers/Array.h"
#include "SimpleJSON/json.hpp"

struct FJsonWriter : public FArchive
{
	FString CurrentKey;
	json::JSON& Root;
	TArray<json::JSON*> ScopeStack;

	FJsonWriter(json::JSON& InData)
		: CurrentKey(), Root(InData)
	{
		ScopeStack.push_back(&Root);
	}

	virtual const FString& GetCurrentKey() { return CurrentKey; }
	virtual void SetCurrentKey(const FString& Key) { CurrentKey = Key; }

	virtual void Serialize(void* Value, uint32 Size) override
	{
		// JSONWriter 는 Serialize 를 직접 구현하지 않고 operator<< 오버로드로 처리합니다.
	}

	virtual void BeginArray(const FString& Key, int32& OutCount) override
	{
		json::JSON& Current = *ScopeStack.back();
		Current[Key.c_str()] = json::Array();
		ScopeStack.push_back(&Current[Key.c_str()]);
	}

	virtual void EndArray() override
	{
		if (ScopeStack.size() > 1) ScopeStack.pop_back();

		CurrentKey.clear();
	}

	void BeginObject(const FString& Key)
	{
		json::JSON& Current = *ScopeStack.back();
		Current[Key.c_str()] = json::Object();
		ScopeStack.push_back(&Current[Key.c_str()]);
	}

	void EndObject()
	{
		if (ScopeStack.size() > 1) ScopeStack.pop_back();
	}

	bool IsSaving() const override { return true; }

	FArchive& operator<<(bool& Value) override;
	FArchive& operator<<(uint32& Value) override;
	FArchive& operator<<(int32& Value) override;
	FArchive& operator<<(float& Value) override;
	FArchive& operator<<(const char* Value) override;
	FArchive& operator<<(FString& Value) override;
	FArchive& operator<<(FName& Value) override;
	FArchive& operator<<(FVector2& Value) override;
	FArchive& operator<<(FVector& Value) override;
	FArchive& operator<<(FVector4& Value) override;
	FArchive& operator<<(FColor& Value) override;
	FArchive& operator<<(FMatrix& Value) override;
};
