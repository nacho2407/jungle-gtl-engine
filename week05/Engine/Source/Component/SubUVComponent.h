#pragma once
#include "PrimitiveComponent.h"

struct FDynamicMesh;
class FPrimitiveSceneProxy;

class ENGINE_API USubUVComponent : public UPrimitiveComponent
{
public:
	DECLARE_RTTI(USubUVComponent, UPrimitiveComponent)

	void PostConstruct() override;

	virtual bool UseSpherePicking() const override { return true; }
	virtual FBoxSphereBounds GetWorldBounds() const override;

	void SetSize(const FVector2& InSize) { Size = InSize; UpdateBounds(); }
	const FVector2& GetSize() const { return Size; }

	void SetColumns(int32 InColumns) { Columns = InColumns; MarkRenderStateDirty(); }
	int32 GetColumns() const { return Columns; }

	void SetRows(int32 InRows) { Rows = InRows; MarkRenderStateDirty(); }
	int32 GetRows() const { return Rows; }

	void SetTotalFrames(int32 InTotalFrames) { TotalFrames = InTotalFrames; MarkRenderStateDirty(); }
	int32 GetTotalFrames() const { return TotalFrames; }

	void SetFirstFrame(int32 InFirstFrame) { FirstFrame = InFirstFrame; MarkRenderStateDirty(); }
	int32 GetFirstFrame() const { return FirstFrame; }

	void SetLastFrame(int32 InLastFrame) { LastFrame = InLastFrame; MarkRenderStateDirty(); }
	int32 GetLastFrame() const { return LastFrame; }

	void SetFPS(float InFPS) { FPS = InFPS; MarkRenderStateDirty(); }
	float GetFPS() const { return FPS; }

	void SetLoop(bool bInLoop) { bLoop = bInLoop; MarkRenderStateDirty(); }
	bool IsLoop() const { return bLoop; }

	void SetBillboard(bool bInBillboard) { bBillboard = bInBillboard; MarkRenderStateDirty(); }
	bool IsBillboard() const { return bBillboard; }

	/** SubUV 렌더링용 메시 데이터 반환 */
	virtual FRenderMesh* GetRenderMesh() const override;
	EPrimitiveRenderCategory GetRenderCategory() const override { return EPrimitiveRenderCategory::SubUV; }
	std::shared_ptr<FPrimitiveSceneProxy> CreateSceneProxy() const override;
	FDynamicMesh* GetSubUVMesh() const { return SubUVMesh.get(); }

private:
	FVector2 Size = FVector2(1.f, 1.f);
	FVector4 Color = FVector4(1.0f, 1.0f, 1.0f, 1.0f);

	int32 Columns = 3;
	int32 Rows = 4;
	int32 TotalFrames = 12;

	int32 FirstFrame = 0;
	int32 LastFrame = 11;

	float FPS = 8.0f;

	bool bLoop = true;
	bool bBillboard = false;

	/** SubUV 렌더링을 위해 생성된 동적 메시 데이터 */
	std::shared_ptr<struct FDynamicMesh> SubUVMesh;
};
