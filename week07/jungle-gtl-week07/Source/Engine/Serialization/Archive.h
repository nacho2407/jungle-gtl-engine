#pragma once

#include "Core/EngineTypes.h"
#include "Core/Containers/String.h"
#include "Core/Containers/Array.h"
#include "Object/FName.h"

struct FArchive
{
	virtual ~FArchive() = default;

	virtual const FString& GetCurrentKey() { static FString EmptyKey; return EmptyKey; }
	virtual void SetCurrentKey(const FString& Key) {}

	virtual void BeginArray(const FString& Key, int32& OutCount) {}
	virtual void EndArray() {}

	virtual void BeginObject(const FString& Key) {}
	virtual void BeginObject(int32 Index) {}
	virtual void EndObject() {}


	virtual void Serialize(void* Data, uint32 Size) = 0;

	virtual bool IsLoading() const { return false; }
	virtual bool IsSaving() const { return false; }

	virtual FArchive& operator<<(bool& Value) = 0;
	virtual FArchive& operator<<(int32& Value) = 0;
	virtual FArchive& operator<<(uint32& Value) = 0;
	virtual FArchive& operator<<(float& Value) = 0;
	virtual FArchive& operator<<(const char* Value) = 0;
	virtual FArchive& operator<<(FString& Value) = 0;
	virtual FArchive& operator<<(FName& Value) = 0;
	virtual FArchive& operator<<(FVector2& Value) = 0;
	virtual FArchive& operator<<(FVector& Value) = 0;
	virtual FArchive& operator<<(FVector4& Value) = 0;
	virtual FArchive& operator<<(FColor& Value) = 0;
	virtual FArchive& operator<<(FMatrix& Value) = 0;
};

template <typename T>
FArchive& operator<<(FArchive& Ar, TArray<T>& Array)
{
	int32 Num = (int32)Array.size();

	Ar.BeginArray(Ar.GetCurrentKey(), Num);

	if (Ar.IsSaving())
	{
		for (T& Element : Array)
		{
			Ar << Element;
		}
	}
	else if (Ar.IsLoading())
	{
		Array.resize(Num);
		for (T& Element : Array)
		{
			Ar << Element;
		}
	}
	
	Ar.EndArray();
	return Ar;
}
