// 컴포넌트 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once
#include "LightComponentBase.h"

class FLightSceneProxy;
class FScene;

// ULightComponent 컴포넌트이다.
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
