#pragma once

#include "Editor/UI/Asset/AssetEditorWidget.h"
#include "Object/FName.h"
#include "Editor/Viewport/Asset/ParticleSystemEditorViewportClient.h"
#include "Editor/UI/Asset/Curve/InlineFloatCurveEditor.h"

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
    void Render(const FEditorPanelContext& Context) override;

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

    // Regenerate — 모든 이미터에서 src LOD 의 모듈을 dst LOD 로 deep-clone 한 뒤
    // spawn rate / spawn rate scale 을 SpawnRateScale 만큼 곱한다. (Cascade와 동일.)
    void RegenerateLOD(int32 SrcLODIndex, int32 DstLODIndex, float SpawnRateScale);

    // 모듈 sharing 관리 — sub-LOD에서만 의미 있음.
    void DuplicateModuleFromHigherLOD(class UParticleEmitter* Emitter, int32 LODIndex, int32 ModuleIndex);
    void ShareModuleFromHigherLOD(class UParticleEmitter* Emitter, int32 LODIndex, int32 ModuleIndex);
    void DuplicateModuleFromHighestLOD(class UParticleEmitter* Emitter, int32 LODIndex, int32 ModuleIndex);

    // 드래그앤드롭 이동. LOD 0 의 Modules 배열 인덱스 기준 — 구조 변경이므로 모든 LOD에
    // 동일 인덱스로 동기 적용. SrcEmitter == DstEmitter 면 단순 재정렬.
    void MoveModule(int32 SrcEmitterIndex, int32 SrcArrayIndex, int32 DstEmitterIndex, int32 DstArrayIndex);
    void MoveEmitter(int32 SrcEmitterIndex, int32 DstEmitterIndex);

    // 이미터의 TypeData 슬롯을 지정 클래스로 교체. ClassName == nullptr 면 Sprite (TypeData 없음).
    // 모든 LOD에 동일하게 적용 (sharing).
    void SetEmitterTypeData(int32 EmitterIndex, const char* TypeDataClassName);

    // Undo / Redo — 구조적 변경(Add/Delete/Duplicate Emitter/Module/LOD) 단위로 스냅샷.
    void PushUndoSnapshot();
    void Undo();
    void Redo();
    void RestoreFromSnapshot(const TArray<uint8>& Buffer);

    // 부가 기능.
    void SaveThumbnail();
    void SaveAssetAs(const FString& NewAssetName);
    void ReimportAsset();
    void FindInContentBrowser();

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

    // Window 메뉴로 토글되는 패널 가시성.
    bool bShowPreviewPanel   = true;
    bool bShowEmittersPanel  = true;
    bool bShowDetailsPanel   = true;
    bool bShowCurvePanel     = true;

    // Save As 팝업 입력.
    char SaveAsNameBuf[128] = {};
    bool bSaveAsPopupRequested = false;

    // 배경색 팝업 진입 신호.
    bool bBgColorPopupRequested = false;

    // Find in Content Browser 팝업 진입 신호.
    bool bFindCBPopupRequested = false;

    // 범용 Undo — 활성 위젯이 바뀔 때 pre-edit 상태를 캐시. 편집 종료 시 스택에 push.
    // bPushPending 은 "활성이 풀린 다음 프레임에 IsDirty 검사 후 push" — 같은 프레임의
    // 위젯 핸들러(예: InvisibleButton+IsItemClicked) 가 MarkDirty 를 늦게 호출해도 놓치지
    // 않게 한 프레임 지연시켜 처리.
    TArray<uint8> PreEditSnapshot;
    bool          bPreEditCached       = false;
    bool          bWasAnyItemActive    = false;
    bool          bPushPending         = false;

    // Drag-drop "현재 활성 drop zone" 추적 (1 frame lag) — 가장 가까운 zone 만 펼치고
    // 강조선을 그리기 위함. (Em, Slot) 의미:
    //   Em<0                : 활성 없음
    //   Em>=0, Slot>=0      : 해당 emitter 의 Modules[Slot] 자리 갭 zone
    //   Em>=0, Slot==SlotAppendSentinel  : 모듈 리스트 끝 append zone
    //   Em>=0, Slot==SlotEmitterColSentinel : emitter 컬럼 헤더 zone
    static constexpr int32 SlotAppendSentinel    = 0x7FFFFFFE;
    static constexpr int32 SlotEmitterColSentinel = 0x7FFFFFFD;
    int32 ActiveDropEmitter  = -1;
    int32 ActiveDropSlot     = -1;
    int32 PendingDropEmitter = -1;
    int32 PendingDropSlot    = -1;

    // 구조적 변경 단위 Undo/Redo 스택 (전체 ParticleSystem 직렬화 스냅샷).
    TArray<TArray<uint8>> UndoStack;
    TArray<TArray<uint8>> RedoStack;
    static constexpr int32 MaxUndoStackSize = 50;

    // 커브 에디터의 상호작용 모드 상태.
    enum class ECurveInteractionMode : uint8 { Pan, Zoom };
    ECurveInteractionMode CurveMode = ECurveInteractionMode::Pan;
    int32 SelectedCurveTrackIndex = -1;
    UParticleModule* SelectedCurveTrackModule = nullptr;
    FInlineFloatCurveEditor InlineCurveEditor;

    FParticleSystemEditorViewportClient ViewportClient;
    FName                               PreviewWorldHandle = FName::None;
    UParticleSystemComponent*           PreviewPSC         = nullptr;
    bool                                bPendingClose      = false;
};
