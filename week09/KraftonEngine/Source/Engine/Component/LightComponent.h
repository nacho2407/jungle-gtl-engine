#pragma once
#include "LightComponentBase.h"

class FLightSceneProxy;
class FScene;

/**
 * ULightComponentBase: 빛이라는 개념이 가져야 할 핵심적인 데이터만 정의합니다.
 * ULightComponent: 렌더 프록시를 생성하고 화면에 그리기 위한 로직을 담당합니다.
 */
class ULightComponent : public ULightComponentBase
{
public:
    DECLARE_CLASS(ULightComponent, ULightComponentBase)
    ULightComponent() = default;

    void Serialize(FArchive& Ar) override;
    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void PostEditProperty(const char* PropertyName) override;

	// ─── 렌더 상태 관리 ───
    void CreateRenderState() override;
    void DestroyRenderState() override;
    void OnTransformDirty() override;

	// ─── 프록시 재생성 (전체 재생성 / 위치·방향만 재생성) ───
    void MarkRenderStateDirty();
	void MarkRenderTransformDirty();
	
	// ─── 서브클래스가 오버라이드하여 구체적인 프록시 생성 ───
	virtual FLightSceneProxy* CreateLightSceneProxy();
	
	FLightSceneProxy* GetLightSceneProxy() const { return LightSceneProxy; }

protected:
	FLightSceneProxy* LightSceneProxy = nullptr;
};