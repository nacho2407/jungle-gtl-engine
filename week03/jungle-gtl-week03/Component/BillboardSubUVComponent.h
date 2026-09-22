#pragma once

#include "PrimitiveComponent.h"

class UBillboardSubUVComp : public UPrimitiveComponent
{
	DECLARE_RTTI(UBillboardSubUVComp, UPrimitiveComponent)
public:
	UBillboardSubUVComp();
	virtual ~UBillboardSubUVComp() override;

	virtual void OnComponentAdded() override;
	virtual void Update(float DeltaTime) override;
	virtual json::JSON Serialize() override;
	virtual void Deserialize(json::JSON) override;

	/**
	 * 레거시 호환용 크기 설정 함수입니다. 내부적으로는 transform scale을 갱신합니다.
	 * \param InWidth 
	 * \param InHeight
	 */
	void SetSize(float InWidth, float InHeight);
	/**
	 * DDS에서 UV를 쪼개기 위한 함수입니다.
	 * \param InRows : DDS를 나눌 row 값입니다.
	 * \param InColumns : DDS를 나눌 column 값입니다.
	 * \param InFrameCount : 실제로 사용할 갯수입니다.
	 */
	void SetGrid(int32 InRows, int32 InColumns, int32 InFrameCount);
	/**
	 * 프레임 속도 및 루프 여부를 조정합니다.
	 * \param InFramesPerSecond : 1초에 몇 프레임이 돌아가는지인데 위의 InFrameCount와 관련됩니다.
	 * \param bInLoop : 루프 여부
	 */
	void SetPlayback(float InFramesPerSecond, bool bInLoop);
	float GetWidth() const { return GetRelativeScale3D().y; }
	float GetHeight() const { return GetRelativeScale3D().z; }
	float GetFramesPerSecond() const { return FramesPerSecond; }
	bool IsLooping() const { return bLoop; }

private:
	/**
	 * SubUV 빌보드가 사용할 렌더 오브젝트를 생성하고 기본 렌더 상태를 설정합니다.
	 * 
	 * \return 씬 레벨에 등록할 렌더 오브젝트 포인터입니다.
	 */
	RenderObject* CreateRenderObject();
	/**
	 * 현재 시간과 카메라 방향을 기준으로 월드 행렬, 선택 상태, UV 변환 값을 렌더 오브젝트에 반영합니다.
	 */
	void UpdateRenderObject();
	/**
	 * 누적 시간과 재생 속도를 바탕으로 현재 표시할 SubUV 프레임 인덱스를 계산합니다.
	 * \return 0 이상 FrameCount 미만의 현재 프레임 인덱스입니다.
	 */
	int32 GetCurrentFrameIndex() const;

private:
	int32 Rows = 6;
	int32 Columns = 6;
	int32 FrameCount = 36;
	float FramesPerSecond = 8.0f;
	float CurrentTime = 0.0f;
	bool bLoop = true;
};
