#pragma once

#include "Archive.h"
#include "Core/Containers/String.h"
#include "SimpleJSON/json.hpp"

struct FJsonReader : public FArchive
{
	FString CurrentKey;
	json::JSON& Data;
	TArray<json::JSON*> ScopeStack;
	TArray<int32> ArrayIndexStack;

	FJsonReader(json::JSON& InData)
		: CurrentKey(), Data(InData)
	{
		ScopeStack.push_back(&Data);
		ArrayIndexStack.push_back(0);
	}

	virtual bool IsLoading() const override { return true; }

	virtual const FString& GetCurrentKey() override { return CurrentKey; }
	virtual void SetCurrentKey(const FString& Key) override { CurrentKey = Key; }

	virtual void Serialize(void* Value, uint32 Size) override
	{
		// JSONReader 는 Serialize 를 직접 구현하지 않고 operator<< 오버로드로 처리합니다.
	}

	virtual void BeginArray(const FString& Key, int32& OutCount) override
	{
		json::JSON& Current = *ScopeStack.back();
		if (!Current.hasKey(Key.c_str()))
		{
			OutCount = 0;
			return;
		}

		json::JSON& Array = Current[Key.c_str()];
		OutCount = (int32)Array.length();
		ScopeStack.push_back(&Array);
	}

	virtual void EndArray() override
	{
		if (ScopeStack.size() > 1) ScopeStack.pop_back();
		CurrentKey.clear();
	}

	virtual void BeginObject(const FString& Key) override
	{
		json::JSON& Current = *ScopeStack.back();
		if (Current.hasKey(Key.c_str()))
		{
			ScopeStack.push_back(&Current[Key.c_str()]);
		}
	}

	virtual void BeginObject(int32 Index) override
	{
		json::JSON& Current = *ScopeStack.back();
		if (Current.at(Index).JSONType() == json::JSON::Class::Object)
		{
			ScopeStack.push_back(&Current.at(Index));
		}
	}	

	virtual void EndObject() override
	{
		if (ScopeStack.size() > 1) ScopeStack.pop_back();
	}

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