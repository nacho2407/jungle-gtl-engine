#pragma once

#include "SceneComponent.h"
#include "Renderer/RenderObject.h"

class UBillboardSubUVComponent : public USceneComponent
{
	DECLARE_RTTI(UBillboardSubUVComponent, USceneComponent)
public:
	UBillboardSubUVComponent();
	virtual ~UBillboardSubUVComponent() override;

	virtual void OnComponentAdded() override;
	virtual void Update(float DeltaTime) override;

	void SetSize(float InWidth, float InHeight);
	void SetGrid(int32 InRows, int32 InColumns, int32 InFrameCount);
	void SetPlayback(float InFramesPerSecond, bool bInLoop);

private:
	RenderObject* CreateRenderObject();
	void UpdateRenderObject();
	int32 GetCurrentFrameIndex() const;

private:
	RenderObject* RenderObj = nullptr;
	float Width = 4.0f;
	float Height = 4.0f;
	int32 Rows = 4;
	int32 Columns = 4;
	int32 FrameCount = 16;
	float FramesPerSecond = 8.0f;
	float CurrentTime = 0.0f;
	bool bLoop = true;
};
