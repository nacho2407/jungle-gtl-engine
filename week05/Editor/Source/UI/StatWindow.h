#pragma once
#include "CoreMinimal.h"
#include "imgui.h"

struct FObjectEntry
{
	FString Name;
	FString ClassName;
	uint32 Size = 0;
};

class CStatWindow
{
public:
	void Render();
	void SetFPS(float InFPS) { FPS = InFPS; }
	void SetFrameTimeMs(float InMs) { FrameTimeMs = InMs; }
	void SetObjectCount(uint32 InCount) { ObjectCount = InCount; }
	void SetHeapUsage(uint32 InBytes) { HeapUsageBytes = InBytes; }

private:
	void RefreshObjectList();

	float FPS = 0.0f;
	float FrameTimeMs = 0.0f;
	uint32 ObjectCount = 0;
	uint32 HeapUsageBytes = 0;

	TArray<FObjectEntry> ObjectEntries;
	bool bShowObjectList = false;
};
