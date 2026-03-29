#pragma once
#include "ObjViewerWidget.h"
#include "Engine/Core/Common.h"

class FObjViewerMenuBarWidget : public FObjViewerWidget
{
public:
	virtual void Render(float DeltaTime) override;

private:
	FString OpenFileDialog();
	FString SaveFileDialog();
};