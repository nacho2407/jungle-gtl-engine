#include "ParticleSystemEditorWidget.h"
#include "ParticleSystemEditorPrivate.h"

static uint32 GNextParticleSystemEditorInstanceId = 0;

FParticleSystemEditorWidget::FParticleSystemEditorWidget()
    : InstanceId(GNextParticleSystemEditorInstanceId++)
{
    const FString Id = std::to_string(InstanceId);

    WindowIdSuffix     = "###ParticleSystemEditor_" + std::to_string(InstanceId);
    PreviewWorldHandle = FName("ParticleSystemEditorPreview_" + Id);
}

bool FParticleSystemEditorWidget::CanEdit(UObject* Object) const
{
    return Object && Object->IsA<UParticleSystem>();
}

bool FParticleSystemEditorWidget::IsEditingObject(UObject* Object) const
{
    return Object == EditedObject;
}

void FParticleSystemEditorWidget::Open(UObject* Object)
{
    FAssetEditorWidget::Open(Object);

    bPendingClose = false;

    SelectedEmitterIndex = -1;
    SelectedModuleIndex  = -1;
    bSimulating          = true;
    PreviewTime          = 0.0f;
    PreviewPSC           = nullptr;
    EmitterNameBufFor    = -1;
    EmitterNameBuf[0]    = '\0';

    UParticleSystem* ParticleSystem = GetParticleSystem();
    if (ParticleSystem)
    {
        WindowTitle = "Particle System Editor - ";
        WindowTitle += ParticleSystem->GetSourcePath();
    }
    SyncEmitterUIState();

    FWorldContext& WorldContext = GEngine->CreateWorldContext(EWorldType::EditorPreview, PreviewWorldHandle);

    WorldContext.World->SetWorldType(EWorldType::EditorPreview);
    WorldContext.World->InitWorld();

    AActor* Actor        = WorldContext.World->SpawnActor<AActor>();
    Actor->bTickInEditor = true;

    if (ParticleSystem)
    {
        UParticleSystemComponent* Comp = Actor->AddComponent<UParticleSystemComponent>();
        Comp->SetTemplate(ParticleSystem);
        Actor->SetRootComponent(Comp);
        PreviewPSC = Comp;
    }

    Actor->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));

    ADirectionalLightActor* LightActor = WorldContext.World->SpawnActor<ADirectionalLightActor>();

    LightActor->InitDefaultComponents();
    LightActor->SetActorRotation(FVector(0.0f, 45.0f, -45.0f));

    if (UDirectionalLightComponent* LightComp = LightActor->GetComponentByClass<UDirectionalLightComponent>())
    {
        LightComp->SetShadowBias(0.0f);
        LightComp->PushToScene();
    }

    ViewportClient.Initialize(GEngine->GetRenderer().GetFD3DDevice().GetDevice(), 64, 64);

    ViewportClient.SetPreviewWorld(WorldContext.World);
    ViewportClient.SetPreviewActor(Actor);
    ViewportClient.SetPreviewParticleSystemComponent(PreviewPSC);
    ViewportClient.ResetCameraToPreviewBounds();

    WorldContext.World->SetEditorPOVProvider(&ViewportClient);
    FSlateApplication::Get().RegisterViewport(&ViewportClient);
}

void FParticleSystemEditorWidget::Close()
{
    if (!IsOpen() && !ViewportClient.IsRenderable())
    {
        return;
    }

    FSlateApplication::Get().UnregisterViewport(&ViewportClient);

    PreviewPSC = nullptr;

    if (UWorld* PreviewWorld = ViewportClient.GetPreviewWorld())
    {
        FScene& PreviewScene = PreviewWorld->GetScene();
        GEngine->GetRenderer().GetResources().ReleaseShadowResourcesForScene(&PreviewScene);

        if (PreviewWorldHandle.IsValid())
        {
            GEngine->DestroyWorldContext(PreviewWorldHandle);
        }
    }

    ViewportClient.SetPreviewParticleSystemComponent(nullptr);
    ViewportClient.SetPreviewActor(nullptr);
    ViewportClient.SetPreviewWorld(nullptr);
    ViewportClient.Release();

    FAssetEditorWidget::Close();
}

void FParticleSystemEditorWidget::Tick(float DeltaTime)
{
    if (bPendingClose)
    {
        bPendingClose = false;
        Close();
        return;
    }

    if (bSimulating)
    {
        PreviewTime += DeltaTime;
    }

    if (ViewportClient.IsRenderable())
    {
        ViewportClient.Tick(DeltaTime);
    }
}

void FParticleSystemEditorWidget::Render(float DeltaTime)
{
    (void)DeltaTime;

    if (!IsOpen() || !EditedObject)
    {
        return;
    }

    bool bWindowOpen = true;

    FString VisibleTitle = WindowTitle;
    if (IsDirty())
    {
        VisibleTitle += " *";
    }
    const FString FullTitle = VisibleTitle + WindowIdSuffix;

    ImGui::SetNextWindowSize(ImVec2(1080.0f, 780.0f), ImGuiCond_Once);
    if (ConsumeFocusRequest())
    {
        ImGui::SetNextWindowFocus();
    }

    ImGui::PushStyleColor(ImGuiCol_WindowBg, PSE::WindowBg);
    // NoScrollbar/NoScrollWithMouse — 에디터는 내부 패널 단위로 스크롤되므로
    // 바깥 윈도우 자체는 스크롤바가 절대 나타나지 않아야 한다.
    ImGuiWindowFlags WindowFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoScrollbar |
    ImGuiWindowFlags_NoScrollWithMouse;
    if (ViewportClient.IsMouseOverViewport())
    {
        WindowFlags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;
    }
    const bool bVisible = ImGui::Begin(FullTitle.c_str(), &bWindowOpen, WindowFlags);
    ImGui::PopStyleColor();

    if (!bVisible)
    {
        ImGui::End();
        if (!bWindowOpen)
        {
            bPendingClose = true;
        }
        return;
    }

    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
    {
        FSlateApplication::Get().BringViewportToFront(&ViewportClient);
        HandleKeyboardShortcuts();
    }

    // ── 범용 Undo 캡처 ──────────────────────────────────────────────────────
    // 활성 위젯이 막 활성화되는 순간(드래그 시작 / InputText 포커스 / ColorPicker 클릭 등)
    // PreEditSnapshot 에 PS 를 직렬화해 캐싱. 활성이 풀리면 push 를 "다음 프레임"으로
    // 미룬다 — 같은 프레임의 위젯 핸들러(InvisibleButton+IsItemClicked 처럼 release 시점에
    // MarkDirty 가 늦게 호출되는 케이스)가 IsDirty 를 켤 시간을 준다.
    {
        // 1) 이전 프레임에서 활성이 풀렸으면 이번 프레임 시점에 push 검사.
        if (bPushPending)
        {
            // explicit PushUndoSnapshot 과 동일 상태면 중복 push 스킵.
            // 또한 "현재 상태 = pre-edit" 인 경우 (편집 결과가 net 없음) 도 스킵.
            const bool bAlreadyPushed = !UndoStack.empty() && UndoStack.back() == PreEditSnapshot;
            bool       bNoNetChange   = false;
            if (!bAlreadyPushed && IsDirty())
            {
                FMemoryArchive NowAr(true);
                if (UParticleSystem* PSNow = GetParticleSystem())
                {
                    PSNow->Serialize(NowAr);
                    bNoNetChange = (NowAr.GetBuffer() == PreEditSnapshot);
                }
            }
            if (IsDirty() && !bAlreadyPushed && !bNoNetChange)
            {
                UndoStack.push_back(PreEditSnapshot);
                while (static_cast<int32>(UndoStack.size()) > MaxUndoStackSize)
                {
                    UndoStack.erase(UndoStack.begin());
                }
                RedoStack.clear();
            }
            bPushPending = false;
        }

        // 2) 현재 프레임의 활성 상태 변화 처리.
        const bool bAnyActive = ImGui::IsAnyItemActive();

        // Drag-drop hover 상태 회전 — 이번 프레임에 활성 zone 으로 사용할 값.
        ActiveDropEmitter  = PendingDropEmitter;
        ActiveDropSlot     = PendingDropSlot;
        PendingDropEmitter = -1;
        PendingDropSlot    = -1;
        if (bAnyActive && !bWasAnyItemActive && !bPreEditCached)
        {
            if (UParticleSystem* PSCap = GetParticleSystem())
            {
                FMemoryArchive Ar(true);
                PSCap->Serialize(Ar);
                PreEditSnapshot = Ar.GetBuffer();
                bPreEditCached  = true;
            }
        }
        if (!bAnyActive && bWasAnyItemActive && bPreEditCached)
        {
            // 활성 해제 — 이번 프레임의 위젯 핸들러가 MarkDirty 를 한 뒤에야 IsDirty 가
            // 갱신되므로, push 는 다음 프레임으로 미룬다.
            bPushPending   = true;
            bPreEditCached = false;
        }
        bWasAnyItemActive = bAnyActive;
    }

    SyncEmitterUIState();

    RenderMenuBar();
    RenderToolbar();
    ImGui::Separator();

    // ── 2 x 2 패널 레이아웃 ────────────────────────────────────────────────
    constexpr float SplitT = 7.0f;

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));

    const ImVec2 Avail   = ImGui::GetContentRegionAvail();
    const float  LayoutW = Avail.x;
    const float  LayoutH = (std::max)(Avail.y - 26.0f, 120.0f); // 상태 바 공간 확보

    const float LeftW     = (std::max)(LayoutW * ColumnRatio, 48.0f);
    const float RightW    = (std::max)(LayoutW - LeftW - SplitT, 48.0f);
    const float LeftTopH  = (std::max)(LayoutH * LeftRowRatio, 48.0f);
    const float LeftBotH  = (std::max)(LayoutH - LeftTopH - SplitT, 48.0f);
    const float RightTopH = (std::max)(LayoutH * RightRowRatio, 48.0f);
    const float RightBotH = (std::max)(LayoutH - RightTopH - SplitT, 48.0f);

    // 좌측: 프리뷰(위) + Details(아래, 크게). Window 메뉴로 가시성 토글.
    ImGui::BeginGroup();
    if (bShowPreviewPanel) RenderViewportPanel(LeftW, bShowDetailsPanel ? LeftTopH : LayoutH);
    if (bShowPreviewPanel && bShowDetailsPanel)
    {
        Splitter("##SplitLeftRow", false, LayoutH, LeftW, LeftRowRatio);
    }
    if (bShowDetailsPanel) RenderPropertiesPanel(LeftW, bShowPreviewPanel ? LeftBotH : LayoutH);
    ImGui::EndGroup();

    ImGui::SameLine();
    Splitter("##SplitColumn", true, LayoutW, LayoutH, ColumnRatio);
    ImGui::SameLine();

    // 우측: 이미터 cascade(위) + 커브 에디터(아래).
    ImGui::BeginGroup();
    if (bShowEmittersPanel) RenderEmittersPanel(RightW, bShowCurvePanel ? RightTopH : LayoutH);
    if (bShowEmittersPanel && bShowCurvePanel)
    {
        Splitter("##SplitRightRow", false, LayoutH, RightW, RightRowRatio);
    }
    if (bShowCurvePanel) RenderCurveEditorPanel(RightW, bShowEmittersPanel ? RightBotH : LayoutH);
    ImGui::EndGroup();

    ImGui::PopStyleVar();

    ImGui::Spacing();
    RenderStatusBar();

    // ── 팝업: Save As / Background Color / Find in CB ────────────────────────
    if (bSaveAsPopupRequested)
    {
        ImGui::OpenPopup("##SaveAsPopup");
        bSaveAsPopupRequested = false;
        SaveAsNameBuf[0]      = '\0';
    }
    if (bBgColorPopupRequested)
    {
        ImGui::OpenPopup("##BgColorPopup");
        bBgColorPopupRequested = false;
    }
    if (bFindCBPopupRequested)
    {
        ImGui::OpenPopup("##FindInCBPopup");
        bFindCBPopupRequested = false;
    }

    if (ImGui::BeginPopup("##SaveAsPopup"))
    {
        ImGui::Text("Save particle system as (new name, same folder):");
        ImGui::SetNextItemWidth(280.0f);
        const bool bEnter = ImGui::InputText(
            "##saveasname",
            SaveAsNameBuf,
            sizeof(SaveAsNameBuf),
            ImGuiInputTextFlags_EnterReturnsTrue
        );
        if (ImGui::Button("Save") || bEnter)
        {
            if (SaveAsNameBuf[0] != '\0')
            {
                SaveAssetAs(FString(SaveAsNameBuf));
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopup("##BgColorPopup"))
    {
        ImGui::Text("Preview Background Color");
        FViewportRenderOptions& Opt = ViewportClient.GetRenderOptions();
        ImGui::ColorPicker4(
            "##bgcol",
            Opt.BackgroundColor,
            ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_AlphaBar
        );
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopup("##FindInCBPopup"))
    {
        if (UParticleSystem* PS = GetParticleSystem())
        {
            ImGui::TextColored(PSE::DimTextV, "Asset path:");
            ImGui::TextWrapped("%s", PS->GetSourcePath().c_str());
        }
        ImGui::Separator();
        ImGui::TextColored(PSE::DimTextV, "(Content Browser focus API not wired — copy the path above for now.)");
        if (ImGui::Button("OK")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    ImGui::End();

    if (!bWindowOpen)
    {
        bPendingClose = true;
    }
}

void FParticleSystemEditorWidget::CollectPreviewViewports(TArray<IEditorPreviewViewportClient*>& OutClients) const
{
    if (IsOpen())
    {
        OutClients.push_back(const_cast<FParticleSystemEditorViewportClient*>(&ViewportClient));
    }
}

UParticleSystem* FParticleSystemEditorWidget::GetParticleSystem() const
{
    return Cast<UParticleSystem>(EditedObject);
}

void FParticleSystemEditorWidget::SaveAsset()
{
    if (UParticleSystem* ParticleSystem = GetParticleSystem())
    {
        SyncEmitterUIState();
        if (FParticleSystemManager::Get().Save(ParticleSystem))
        {
            ClearDirty();
            // 동일 템플릿을 참조하는 레벨 내 컴포넌트는 Emitter Instance 캐시에
            // 옛 머티리얼/모듈 상태를 들고 있다. 저장 직후 ResetSystem으로 다시 빌드시킨다.
            RefreshExternalComponents(ParticleSystem);
        }
    }
}

void FParticleSystemEditorWidget::SelectEmitter(int32 EmitterIndex, int32 ModuleIndex)
{
    if (EmitterIndex != SelectedEmitterIndex)
    {
        // 이미터 이름 입력 버퍼를 새 선택에서 다시 채우도록 무효화.
        EmitterNameBufFor = -1;
    }

    if (EmitterIndex != SelectedEmitterIndex || ModuleIndex != SelectedModuleIndex)
    {
        SelectedCurveTrackIndex = -1;
        SelectedCurveTrackModule = nullptr;
        InlineCurveEditor.ResetSelection();
    }

    SelectedEmitterIndex = EmitterIndex;
    SelectedModuleIndex  = ModuleIndex;
}

