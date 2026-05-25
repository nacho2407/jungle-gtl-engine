#pragma once

#include "Editor/UI/Asset/AssetEditorWidget.h"
#include "Object/FName.h"
#include "Editor/Viewport/Asset/ParticleSystemEditorViewportClient.h"

class UParticleSystem;
class UParticleSystemComponent;
class UParticleModule;
struct FParticleBurst;

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
    void DuplicateEmitter(int32 SourceIndex);
    void DeleteSelectedModule();
    void SyncEmitterUIState();
    void RestartPreviewSimulation();
    void HandleKeyboardShortcuts();
    void RefreshExternalComponents(class UParticleSystem* Template);
    void OpenMaterialForRequired(class UParticleModuleRequired* Required);
    void DuplicateMaterialForRequired(class UParticleModuleRequired* Required);

    // LOD 관리. SelectedLODIndex가 현재 편집 중인 레벨.
    void AddLODAfterSelected();
    void RemoveLODAt(int32 LODIndex);
    void SelectLOD(int32 LODIndex);

    // 모듈 sharing 관리 — sub-LOD에서만 의미 있음.
    void DuplicateModuleFromHigherLOD(class UParticleEmitter* Emitter, int32 LODIndex, int32 ModuleIndex);
    void ShareModuleFromHigherLOD(class UParticleEmitter* Emitter, int32 LODIndex, int32 ModuleIndex);
    void DuplicateModuleFromHighestLOD(class UParticleEmitter* Emitter, int32 LODIndex, int32 ModuleIndex);

    void RenderMenuBar();
    void RenderToolbar();
    void RenderStatusBar();
    void RenderViewportPanel(float Width, float Height);
    void RenderEmittersPanel(float Width, float Height);
    void RenderPropertiesPanel(float Width, float Height);
    void RenderCurveEditorPanel(float Width, float Height);
    void RenderModuleProperties(UParticleModule* Module);
    void RenderBurstList(TArray<FParticleBurst>& Bursts);

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

    // 현재 편집 중인 LOD 레벨. 0이 highest detail, N>0은 sub-LOD.
    // 모든 이미터가 같은 LOD 수를 가진다고 가정 (Cascade 규약).
    int32 SelectedLODIndex     = 0;

    // 툴바 Play/Pause와 연결되는 시뮬레이션 상태.
    bool  bSimulating = false;
    float PreviewTime = 0.0f;

    // 드래그 가능한 레이아웃 분할 비율.
    float ColumnRatio   = 0.50f;
    float LeftRowRatio  = 0.45f;   // 좌측: 프리뷰는 작게, Details를 크게(스크롤 회피).
    float RightRowRatio = 0.55f;   // 우측: 이미터 cascade를 커브보다 넓게.

    // 이미터 이름 InputText 버퍼. 선택이 바뀔 때만 모델에서 다시 채워서
    // 사용자가 입력 중인 글자가 매 프레임 덮어써지지 않게 한다.
    char  EmitterNameBuf[128] = {};
    int32 EmitterNameBufFor   = -1;

    // Details 패널 상단의 속성 검색 입력. 현재는 시각용 placeholder.
    char  PropertySearch[128] = {};

    FParticleSystemEditorViewportClient ViewportClient;
    FName                               PreviewWorldHandle = FName::None;
    UParticleSystemComponent*           PreviewPSC         = nullptr;
    bool                                bPendingClose      = false;
};
