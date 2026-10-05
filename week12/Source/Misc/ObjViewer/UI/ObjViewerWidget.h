#pragma once

class UObjViewerEngine;

class FObjViewerWidget
{
public:
	virtual ~FObjViewerWidget() = default;

	virtual void Initialize(UObjViewerEngine* InEngine)
	{
		Engine = InEngine;
	}

	virtual void Render(float DeltaTime) = 0;

protected:
	UObjViewerEngine* Engine = nullptr;
};