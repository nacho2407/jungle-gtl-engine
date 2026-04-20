#pragma once

#include "Object/Object.h"
#include "imgui.h"

struct DragSoruceInfo final
{
	UObject* Object;
};

class EditorDragSource
{
public:
	void Render(ImVec2 InSize);
	void SetDragSourceInfo(DragSoruceInfo* info) { DragSourceInfo = info; }
	void SetID(FString ID) { DragID = ID; }

protected:
	virtual void RenderSource(ImVec2 InSize) = 0;

	DragSoruceInfo* DragSourceInfo;
private:
	FString DragID;
};

