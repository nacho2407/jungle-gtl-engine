#pragma once

#include "Editor/UI/Asset/AssetEditorWidget.h"
#include "Object/FName.h"
#include "Editor/Viewport/Asset/ParticleSystemEditorViewportClient.h"

class UParticleSystem;
class UParticleSystemComponent;

class FParticleSystemEditorWidget : public FAssetEditorWidget
{
public:
    FParticleSystemEditorWidget();
    ~FParticleSystemEditorWidget() override = default;

    bool CanEdit(UObject* Object) const override;
    bool IsEditingObject(UObject* Object) const override;

    void Open(UObject* Object) override;
    void Close() override;
    void Tick(float DeltaTime) override;
    void Render(float DeltaTime) override;

    bool AllowsMultipleInstances() const override { return true; }

    void CollectPreviewViewports(TArray<IEditorPreviewViewportClient*>& OutClients) const override;

private:
    UParticleSystem* GetParticleSystem() const;

    void SaveAsset();
    void SelectEmitter(int32 EmitterIndex, int32 ModuleIndex);

    void AddEmitter();
    void DeleteSelectedEmitter();
    void SyncEmitterUIState();
    void RestartPreviewSimulation();

    void RenderMenuBar();
    void RenderToolbar();
    void RenderStatusBar();
    void RenderViewportPanel(float Width, float Height);
    void RenderEmittersPanel(float Width, float Height);
    void RenderPropertiesPanel(float Width, float Height);
    void RenderCurveEditorPanel(float Width, float Height);

    void ResetPreviewComponent();

private:
    FString WindowTitle    = "Particle System Editor";
    FString WindowIdSuffix = "###ParticleSystemEditor";
    uint32  InstanceId     = 0;

    // 패널이 공유하는 선택 상태. Emitters 패널이 갱신하고
    // Properties / Curve Editor 패널이 이를 읽어 표시 대상을 결정한다.
    // EmitterIndex < 0 : 파티클 시스템 자체를 검사 중.
    // ModuleIndex  < 0 : 이미터 자체를 검사 중.
    int32 SelectedEmitterIndex = -1;
    int32 SelectedModuleIndex  = -1;

    // 프리뷰 시뮬레이션 상태 (뷰포트 트랜스포트 바와 연결).
    bool  bSimulating = false;
    float PreviewTime = 0.0f;

    // 프리뷰 렌더 타깃. 파티클 뷰포트 클라이언트를 붙여 이 핸들을 채우면
    // 별도 수정 없이 곧바로 프리뷰가 표시된다.
    void* PreviewTextureHandle = nullptr;

    // 드래그 가능한 레이아웃 분할 비율.
    float ColumnRatio   = 0.46f;   // 좌(프리뷰+프로퍼티) 폭 비율
    float LeftRowRatio  = 0.56f;   // 좌측에서 프리뷰 높이 비율
    float RightRowRatio = 0.58f;   // 우측에서 이미터 높이 비율

    // 이미터별 활성 플래그 (UI 상태; 데이터 모델에 bEnabled 가 생기면 대체).
    TArray<bool> EmitterEnabled;

    // 커브 트랙 가시성 (선택된 모듈의 커브 수만큼 사용).
    bool CurveTrackVisible[4] = { true, true, true, true };

    char PropertySearch[128] = {};

    FParticleSystemEditorViewportClient ViewportClient;
    FName                               PreviewWorldHandle = FName::None;
    UParticleSystemComponent*           PreviewPSC         = nullptr;
};
