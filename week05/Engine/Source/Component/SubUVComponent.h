#pragma once
#include "PrimitiveComponent.h"

struct FDynamicMesh;

class ENGINE_API USubUVComponent : public UPrimitiveComponent
{
public:
	DECLARE_RTTI(USubUVComponent, UPrimitiveComponent)

	void PostConstruct() override;
	void BeginPlay() override;
	void Tick(float DeltaTime) override;

	virtual bool UseSpherePicking() const override { return true; }

	virtual FBoxSphereBounds GetWorldBounds() const override;

	void SetSize(const FVector2& InSize)
	{
		if (Size.X != InSize.X || Size.Y != InSize.Y)
		{
			Size = InSize;
			MarkSubUVMeshDirty();
			UpdateBounds();
		}
	}
	const FVector2& GetSize() const { return Size; }

	void SetColor(const FVector4& InColor) { Color = InColor; }
	const FVector4& GetColor() const { return Color; }

	void SetColumns(int32 InColumns)
	{
		const int32 Sanitized = (std::max)(1, InColumns);
		if (Columns != Sanitized)
		{
			Columns = Sanitized;
			ResetAnimation();
		}
	}
	int32 GetColumns() const { return Columns; }

	void SetRows(int32 InRows)
	{
		const int32 Sanitized = (std::max)(1, InRows);
		if (Rows != Sanitized)
		{
			Rows = Sanitized;
			ResetAnimation();
		}
	}
	int32 GetRows() const { return Rows; }

	void SetTotalFrames(int32 InTotalFrames)
	{
		const int32 Sanitized = (std::max)(1, InTotalFrames);
		if (TotalFrames != Sanitized)
		{
			TotalFrames = Sanitized;
			ResetAnimation();
		}
	}
	int32 GetTotalFrames() const { return TotalFrames; }

	void SetFirstFrame(int32 InFirstFrame)
	{
		if (FirstFrame != InFirstFrame)
		{
			FirstFrame = InFirstFrame;
			ResetAnimation();
		}
	}
	int32 GetFirstFrame() const { return FirstFrame; }

	void SetLastFrame(int32 InLastFrame)
	{
		if (LastFrame != InLastFrame)
		{
			LastFrame = InLastFrame;
			ResetAnimation();
		}
	}
	int32 GetLastFrame() const { return LastFrame; }

	void SetFPS(float InFPS)
	{
		const float Sanitized = (std::max)(0.0f, InFPS);
		if (FPS != Sanitized)
		{
			FPS = Sanitized;
			ResetAnimation();
		}
	}
	float GetFPS() const { return FPS; }

	void SetLoop(bool bInLoop)
	{
		if (bLoop != bInLoop)
		{
			bLoop = bInLoop;
			ResetAnimation();
		}
	}
	bool IsLoop() const { return bLoop; }

	void SetBillboard(bool bInBillboard) { bBillboard = bInBillboard; }
	bool IsBillboard() const { return bBillboard; }

	int32 GetCurrentFrame() const { return CurrentFrame; }

	void ResetAnimation();
	void MarkSubUVMeshDirty();
	bool IsSubUVMeshDirty() const { return bSubUVMeshDirty; }
	void ClearSubUVMeshDirty() { bSubUVMeshDirty = false; }

	/** SubUV 렌더링용 메시 데이터 반환 */
	virtual FRenderMesh* GetRenderMesh() const override;
	FDynamicMesh* GetSubUVMesh() const { return SubUVMesh.get(); }
	void Serialize(FArchive& Ar) override;
	void DuplicateShallow(UObject* DuplicatedObject, FDuplicateContext& Context) const override;

private:
	static int32 ComputeFrameIndex(
		int32 Columns,
		int32 Rows,
		int32 TotalFrames,
		int32 FirstFrame,
		int32 LastFrame,
		float FPS,
		float ElapsedTime,
		bool bLoop);

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

	float ElapsedTime = 0.0f;
	int32 CurrentFrame = 0;
	bool bSubUVMeshDirty = true;

	/** SubUV 렌더링을 위해 생성된 동적 메시 데이터 */
	std::shared_ptr<struct FDynamicMesh> SubUVMesh;
};
