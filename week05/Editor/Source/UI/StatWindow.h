#pragma once
#include "CoreMinimal.h"
#include "imgui.h"

struct FRect;

struct FObjectEntry
{
	FString Name;
	FString ClassName;
	uint32 Size = 0;
};

class FStatWindow
{
public:
	void Render(const FRect& AreaRect);
	void SetObjectCount(uint32 InCount) { ObjectCount = InCount; }
	void SetHeapUsage(uint32 InBytes) { HeapUsageBytes = InBytes; }

private:
	void RefreshObjectList();

	uint32 ObjectCount = 0;
	uint32 HeapUsageBytes = 0;

	TArray<FObjectEntry> ObjectEntries;
	bool bShowObjectList = false;
};
