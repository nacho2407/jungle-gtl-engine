#include "ParticleSystemEditorWidget.h"
#include "ParticleSystemEditorPrivate.h"

// =============================================================================
// Particle system editor UI panels
// =============================================================================

// -----------------------------------------------------------------------------
// Menu bar
// -----------------------------------------------------------------------------

void FParticleSystemEditorWidget::RenderMenuBar()
{
    if (!ImGui::BeginMenuBar())
    {
        return;
    }

    if (ImGui::BeginMenu("File"))
    {
        if (ImGui::MenuItem("Save", "Ctrl+S", false, IsDirty())) SaveAsset();
        if (ImGui::MenuItem("Save As...", nullptr, false, GetParticleSystem() != nullptr))
        {
            bSaveAsPopupRequested = true;
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Close")) bPendingClose = true;
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Edit"))
    {
        if (ImGui::MenuItem("Undo", "Ctrl+Z", false, !UndoStack.empty())) Undo();
        if (ImGui::MenuItem("Redo", "Ctrl+Y", false, !RedoStack.empty())) Redo();
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Asset"))
    {
        if (ImGui::MenuItem("Find in Content Browser", nullptr, false, GetParticleSystem() != nullptr))
        {
            FindInContentBrowser();
        }
        if (ImGui::MenuItem("Reimport", nullptr, false, GetParticleSystem() != nullptr))
        {
            ReimportAsset();
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Window"))
    {
        ImGui::MenuItem("Preview", nullptr, &bShowPreviewPanel);
        ImGui::MenuItem("Emitters", nullptr, &bShowEmittersPanel);
        ImGui::MenuItem("Details", nullptr, &bShowDetailsPanel);
        ImGui::MenuItem("Curve Editor", nullptr, &bShowCurvePanel);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Help"))
    {
        if (ImGui::MenuItem("Documentation"))
        {
            // 사용자 환경에서 기본 브라우저로 URL 열기.
        #if defined(_WIN32)
            ShellExecuteA(
                nullptr,
                "open",
                "https://docs.unrealengine.com/Engine/Rendering/ParticleSystems/",
                nullptr,
                nullptr,
                SW_SHOWNORMAL
            );
        #endif
        }
        ImGui::EndMenu();
    }

    ImGui::EndMenuBar();
}

// -----------------------------------------------------------------------------
// Main toolbar and status bar
// -----------------------------------------------------------------------------

void FParticleSystemEditorWidget::RenderToolbar()
{
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 4.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(3.0f, 3.0f));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 1.0f, 1.0f, 0.12f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0f, 1.0f, 1.0f, 0.22f));

    constexpr float IconSize = 28.0f;
    // 높이 = WindowPadding y*2 + (icon + FramePadding y*2) + 가로 스크롤바.
    //      = 2*2 + (28 + 2*4) + 14 = 54px.
    constexpr float ToolbarH = 54.0f;

    // child 내부의 기본 WindowPadding(8,8)이 그대로면 세로 스크롤바가 항상 생긴다.
    // 가로 스크롤바만 필요할 때만 보이도록 padding을 최소화.
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(2.0f, 2.0f));
    if (ImGui::BeginChild(
        "##PSEToolbar",
        ImVec2(0.0f, ToolbarH),
        false,
        ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoBackground
    ))
    {
        auto Group = []()
        {
            const ImVec2 Pos = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddLine(
                ImVec2(Pos.x + 4.0f, Pos.y + 4.0f),
                ImVec2(Pos.x + 4.0f, Pos.y + 32.0f),
                PSE::Border32
            );
            ImGui::Dummy(ImVec2(8.0f, 0.0f));
            ImGui::SameLine();
        };

        // 그룹 1: 에셋.
        if (IconToolButton(
            "##Save",
            LoadToolIcon(L"SaveCurrent.png"),
            "Save",
            "Save the particle system (Ctrl+S)",
            IsDirty(),
            IconSize
        ))
        {
            SaveAsset();
        }
        ImGui::SameLine();
        if (IconToolButton(
            "##FindCB",
            LoadToolIcon(L"ContentBrowser.png"),
            "Find",
            "Show this asset's location",
            GetParticleSystem() != nullptr,
            IconSize
        ))
        {
            FindInContentBrowser();
        }
        ImGui::SameLine();
        Group();

        // 그룹 2: 시뮬레이션.
        if (IconToolButton(
            "##RestartSim",
            LoadToolIcon(L"icon_Cascade_RestartSim_40x.png"),
            "RSim",
            "Restart the preview simulation",
            true,
            IconSize
        ))
        {
            RestartPreviewSimulation();
        }
        ImGui::SameLine();
        if (IconToolButton(
            "##RestartLvl",
            LoadToolIcon(L"icon_Cascade_RestartInLevel_40x.png"),
            "RLvl",
            "Restart all level instances of this particle system\n"
            "(re-runs ResetSystem on every UParticleSystemComponent referencing this asset)",
            GetParticleSystem() != nullptr,
            IconSize
        ))
        {
            RefreshExternalComponents(GetParticleSystem());
            RestartPreviewSimulation();
        }
        ImGui::SameLine();
        Group();

        // 그룹 3: 편집 이력 — 구조적 변경(Add/Delete/Duplicate Emitter/Module/LOD) 단위로 동작.
        if (IconToolButton(
            "##Undo",
            LoadToolIcon(L"icon_Generic_Undo_40x.png"),
            "Undo",
            "Undo (Ctrl+Z)",
            !UndoStack.empty(),
            IconSize
        ))
        {
            Undo();
        }
        ImGui::SameLine();
        if (IconToolButton(
            "##Redo",
            LoadToolIcon(L"icon_Generic_Redo_40x.png"),
            "Redo",
            "Redo (Ctrl+Y)",
            !RedoStack.empty(),
            IconSize
        ))
        {
            Redo();
        }
        ImGui::SameLine();
        Group();

        // 그룹 4: 뷰포트 옵션.
        FViewportRenderOptions& VPOpt = ViewportClient.GetRenderOptions();

        if (IconToolButton(
            "##Thumb",
            LoadToolIcon(L"icon_Cascade_Thumbnail_40x.png"),
            "Thmb",
            "Capture preview thumbnail to <asset>.thumb.bmp",
            ViewportClient.IsRenderable() && GetParticleSystem() != nullptr,
            IconSize
        ))
        {
            SaveThumbnail();
        }
        ImGui::SameLine();
        {
            char Tip[96];
            std::snprintf(
                Tip,
                sizeof(Tip),
                "Toggle bounds display (currently %s)",
                VPOpt.ShowFlags.bBoundingVolume ? "ON" : "OFF"
            );
            if (IconToolButton("##Bounds", LoadToolIcon(L"icon_Cascade_Bounds_40x.png"), "Bnds", Tip, true, IconSize))
            {
                VPOpt.ShowFlags.bBoundingVolume = !VPOpt.ShowFlags.bBoundingVolume;
            }
        }
        ImGui::SameLine();
        {
            char Tip[96];
            std::snprintf(
                Tip,
                sizeof(Tip),
                "Toggle world axis display (currently %s)",
                VPOpt.ShowFlags.bWorldAxis ? "ON" : "OFF"
            );
            if (IconToolButton("##Axis", LoadToolIcon(L"icon_Cascade_Axis_40x.png"), "Axis", Tip, true, IconSize))
            {
                VPOpt.ShowFlags.bWorldAxis = !VPOpt.ShowFlags.bWorldAxis;
            }
        }
        ImGui::SameLine();
        if (IconToolButton(
            "##BG",
            LoadToolIcon(L"icon_Cascade_Color_40x.png"),
            "BG",
            "Set preview background color",
            true,
            IconSize
        ))
        {
            bBgColorPopupRequested = true;
        }
        ImGui::SameLine();
        Group();

        // 그룹 5: LOD. RegenLOD 만 미구현, 나머지는 모두 작동.
        const int32 PSLODCount = GetParticleSystemLODCount(GetParticleSystem());

        if (IconToolButton(
            "##RegenLOD1",
            LoadToolIcon(L"icon_Cascade_RegenLOD1_40x.png"),
            "RL1",
            "Regenerate lowest LOD from highest (LOD 0 → last LOD, spawn x0.5)",
            PSLODCount > 1,
            IconSize
        ))
        {
            RegenerateLOD(/*Src=*/0, /*Dst=*/PSLODCount - 1, 0.5f);
        }
        ImGui::SameLine();
        if (IconToolButton(
            "##RegenLOD2",
            LoadToolIcon(L"icon_Cascade_RegenLOD2_40x.png"),
            "RL2",
            "Regenerate highest LOD from lowest (last LOD → LOD 0, spawn x2.0)",
            PSLODCount > 1,
            IconSize
        ))
        {
            RegenerateLOD(/*Src=*/PSLODCount - 1, /*Dst=*/0, 2.0f);
        }
        ImGui::SameLine();
        if (IconToolButton(
            "##LowestLOD",
            LoadToolIcon(L"icon_Cascade_LowestLOD_40x.png"),
            "LowL",
            "Jump to lowest LOD",
            SelectedLODIndex < PSLODCount - 1,
            IconSize
        ))
        {
            SelectLOD(PSLODCount - 1);
        }
        ImGui::SameLine();
        if (IconToolButton(
            "##LowerLOD",
            LoadToolIcon(L"icon_Cascade_LowerLOD_40x.png"),
            "Lwr",
            "Jump to next lower LOD (higher index)",
            SelectedLODIndex < PSLODCount - 1,
            IconSize
        ))
        {
            SelectLOD(SelectedLODIndex + 1);
        }
        ImGui::SameLine();
        if (IconToolButton(
            "##HigherLOD",
            LoadToolIcon(L"icon_Cascade_HigherLOD_40x.png"),
            "Hgr",
            "Jump to next higher LOD (lower index)",
            SelectedLODIndex > 0,
            IconSize
        ))
        {
            SelectLOD(SelectedLODIndex - 1);
        }
        ImGui::SameLine();
        if (IconToolButton(
            "##HighestLOD",
            LoadToolIcon(L"icon_Cascade_HighestLOD_40x.png"),
            "HghL",
            "Jump to highest LOD (LOD 0)",
            SelectedLODIndex > 0,
            IconSize
        ))
        {
            SelectLOD(0);
        }
        ImGui::SameLine();
        if (IconToolButton(
            "##AddLOD1",
            LoadToolIcon(L"icon_Cascade_AddLOD1_40x.png"),
            "+L1",
            "Add LOD before the current (= insert at current index)",
            GetParticleSystem() != nullptr,
            IconSize
        ))
        {
            // "Before" = 현재 인덱스에 끼워넣기 — SelectedLODIndex 를 1 줄여서 AddAfter 호출.
            const int32 OldSel = SelectedLODIndex;
            if (OldSel > 0)
            {
                SelectedLODIndex = OldSel - 1;
                AddLODAfterSelected();
            }
            else
            {
                // LOD 0 위에 끼울 수는 없으므로 후행으로 추가하고 0번 유지.
                AddLODAfterSelected();
            }
        }
        ImGui::SameLine();
        if (IconToolButton(
            "##AddLOD2",
            LoadToolIcon(L"icon_Cascade_AddLOD2_40x.png"),
            "+L2",
            "Add LOD after the current",
            GetParticleSystem() != nullptr,
            IconSize
        ))
        {
            AddLODAfterSelected();
        }
        ImGui::SameLine();
        if (IconToolButton(
            "##DeleteLOD",
            LoadToolIcon(L"icon_Cascade_DeleteLOD_40x.png"),
            "-LOD",
            "Delete current LOD (LOD 0 cannot be removed)",
            SelectedLODIndex > 0,
            IconSize
        ))
        {
            RemoveLODAt(SelectedLODIndex);
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleVar(); // WindowPadding

    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(3);
}

void FParticleSystemEditorWidget::RenderStatusBar()
{
    UParticleSystem* ParticleSystem = GetParticleSystem();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, PSE::FrameBg);
    if (ImGui::BeginChild("##PSEStatusBar", ImVec2(0.0f, 24.0f), false))
    {
        const FString Path = ParticleSystem ? ParticleSystem->GetSourcePath() : FString();
        ImGui::TextColored(PSE::DimTextV, "%s", Path.empty() ? "Unsaved asset" : Path.c_str());

        ImGui::SameLine();
        ImGui::TextColored(PSE::DimTextV, "  |  %s", IsDirty() ? "Modified" : "Saved");

        ImGui::SameLine();
        if (SelectedEmitterIndex < 0)
        {
            ImGui::TextColored(PSE::DimTextV, "  |  Selection: Particle System");
        }
        else if (SelectedModuleIndex < 0)
        {
            ImGui::TextColored(PSE::DimTextV, "  |  Selection: Emitter %d", SelectedEmitterIndex);
        }
        else
        {
            UParticleEmitter* StatusEmitter = nullptr;
            if (ParticleSystem && SelectedEmitterIndex < static_cast<int32>(ParticleSystem->GetEmitters().size()))
            {
                StatusEmitter = ParticleSystem->GetEmitters()[SelectedEmitterIndex];
            }

            TArray<FEmitterModuleEntry> ModuleList;
            BuildEmitterModuleListAt(StatusEmitter, SelectedLODIndex, ModuleList);

            const char* ModuleName = "?";
            if (SelectedModuleIndex < static_cast<int32>(ModuleList.size()))
            {
                ModuleName = ModuleList[SelectedModuleIndex].Name;
            }

            ImGui::TextColored(PSE::DimTextV, "  |  Selection: Emitter %d > %s", SelectedEmitterIndex, ModuleName);
        }

        ImGui::SameLine();
        ImGui::TextColored(PSE::DimTextV, "  |  Sim %.2fs %s", PreviewTime, bSimulating ? "(playing)" : "(paused)");
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// -----------------------------------------------------------------------------
// Preview viewport panel
// -----------------------------------------------------------------------------

void FParticleSystemEditorWidget::RenderViewportPanel(float Width, float Height)
{
    char Context[32];
    std::snprintf(Context, sizeof(Context), "%.2fs", PreviewTime);

    if (BeginPanel("##PSEViewport", "Preview", Width, Height, Context))
    {
        const ImVec2 CanvasMin  = ImGui::GetCursorScreenPos();
        ImVec2       CanvasSize = ImGui::GetContentRegionAvail();
        CanvasSize.y            = (std::max)(CanvasSize.y, 32.0f);
        const ImVec2 CanvasMax(CanvasMin.x + CanvasSize.x, CanvasMin.y + CanvasSize.y);

        ImDrawList* DrawList = ImGui::GetWindowDrawList();

        FViewport* VP = ViewportClient.GetViewport();

        if (VP && CanvasSize.x > 0.0f && CanvasSize.y > 0.0f)
        {
            ViewportClient.SetViewportRect(CanvasMin.x, CanvasMin.y, CanvasSize.x, CanvasSize.y);
            VP->RequestResize(static_cast<uint32>(CanvasSize.x), static_cast<uint32>(CanvasSize.y));

            if (VP->GetSRV())
            {
                ImGui::Image(reinterpret_cast<ImTextureID>(VP->GetSRV()), CanvasSize);
                FSlateApplication::Get().SetViewportImGuiHovered(&ViewportClient, ImGui::IsItemHovered());
            }
            else
            {
                ImGui::Dummy(CanvasSize);
                DrawList->AddRectFilled(CanvasMin, CanvasMax, PSE::ViewportBg, 4.0f);
                CanvasHint(DrawList, CanvasMin, CanvasMax, "Preview viewport is initializing.");
            }
        }
        else
        {
            ImGui::Dummy(CanvasSize);
            DrawList->AddRectFilled(CanvasMin, CanvasMax, PSE::ViewportBg, 4.0f);
            CanvasHint(DrawList, CanvasMin, CanvasMax, "Attach a particle viewport to render the preview");
        }
    }
    EndPanel();
}

// -----------------------------------------------------------------------------
// Emitter stack panel
// -----------------------------------------------------------------------------

void FParticleSystemEditorWidget::RenderEmittersPanel(float Width, float Height)
{
    UParticleSystem* ParticleSystem = GetParticleSystem();
    const int32      EmitterCount   = ParticleSystem ? static_cast<int32>(ParticleSystem->GetEmitters().size()) : 0;

    // 시스템 단위 LOD 카운트 — LODDistances와 LODLevels가 같은 인덱스를 공유한다.
    const int32 LODCount = GetParticleSystemLODCount(ParticleSystem);

    // SelectedLODIndex 범위 검증.
    if (SelectedLODIndex < 0) SelectedLODIndex = 0;
    if (SelectedLODIndex >= LODCount) SelectedLODIndex = LODCount - 1;

    char Context[64];
    std::snprintf(
        Context,
        sizeof(Context),
        "LOD %d / %d  ·  %d emitter%s",
        SelectedLODIndex,
        LODCount - 1,
        EmitterCount,
        EmitterCount == 1 ? "" : "s"
    );

    if (BeginPanel("##PSEEmitters", "Emitters", Width, Height, Context))
    {
        // ── LOD 바 ─────────────────────────────────────────────────────────
        // [LOD 0] [LOD 1] ... [+] [-]  Distance: [ ###### ]
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
        for (int32 L = 0; L < LODCount; ++L)
        {
            char Label[16];
            std::snprintf(Label, sizeof(Label), "LOD %d", L);
            const bool bActive = (L == SelectedLODIndex);
            if (bActive)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.29f, 0.56f, 1.0f, 0.40f));
            }
            if (ImGui::SmallButton(Label))
            {
                SelectLOD(L);
            }
            if (bActive) ImGui::PopStyleColor();
            ImGui::SameLine();
        }
        if (ImGui::SmallButton("+##addlod"))
        {
            AddLODAfterSelected();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Add a new LOD level after the current one");
        ImGui::SameLine();
        const bool bCanRemove = SelectedLODIndex > 0;
        if (!bCanRemove) ImGui::BeginDisabled();
        if (ImGui::SmallButton("-##rmlod"))
        {
            RemoveLODAt(SelectedLODIndex);
        }
        if (!bCanRemove) ImGui::EndDisabled();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Remove the current LOD (LOD 0 cannot be removed)");

        // sub-LOD 의 활성 거리 편집 — LOD N (N>0) 거리는 LODDistances[N].
        if (SelectedLODIndex > 0 && ParticleSystem)
        {
            ImGui::SameLine();
            ImGui::TextColored(PSE::DimTextV, "Distance");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(100.0f);
            const int32 DistIdx = SelectedLODIndex;
            if (DistIdx < static_cast<int32>(ParticleSystem->LODDistances.size()))
            {
                float Dist = ParticleSystem->LODDistances[DistIdx];
                if (ImGui::DragFloat("##LODDist", &Dist, 10.0f, 0.0f, 1000000.0f, "%.1f"))
                {
                    ParticleSystem->LODDistances[DistIdx] = (std::max)(0.0f, Dist);
                    MarkDirty();
                }
            }
        }
        ImGui::PopStyleVar();
        ImGui::Separator();

        constexpr float ColumnWidth = 178.0f;
        if (ImGui::BeginChild("##PSEEmitterColumns", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_HorizontalScrollbar))
        {
            int32 EmitterToDelete    = -1;
            int32 EmitterToDuplicate = -1;

            // Drag and drop deferred state — 한 프레임 안에 여러 target 이 fire 해도 마지막
            // 하나만 적용. 루프 종료 후 한 번에 처리해서 이터레이션 중 배열 변형을 피한다.
            int32 ModuleDropSrcE = -1, ModuleDropSrcAi = -1;
            int32 ModuleDropDstE = -1, ModuleDropDstAi = -1;
            int32 EmitterDropSrc = -1, EmitterDropDst  = -1;

            for (int32 EmitterIndex = 0; EmitterIndex < EmitterCount; ++EmitterIndex)
            {
                ImGui::PushID(EmitterIndex);
                if (EmitterIndex > 0)
                {
                    ImGui::SameLine();
                }

                const bool bEmitterSelected = (SelectedEmitterIndex == EmitterIndex);

                ImGui::PushStyleColor(ImGuiCol_Border, bEmitterSelected ? PSE::Accent : PSE::Border32);
                ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
                ImGui::BeginChild("##EmitterCol", ImVec2(ColumnWidth, 0.0f), true);
                {
                    UParticleEmitter* Emitter = ParticleSystem->GetEmitters()[EmitterIndex];

                    // 헤더 줄: Selectable + x 버튼.
                    const FString EmitterLabel = (Emitter && !Emitter->EmitterName.ToString().empty())
                    ? Emitter->EmitterName.ToString() : (FString("Emitter ") + std::to_string(EmitterIndex));

                    const bool bHeaderSel = bEmitterSelected && SelectedModuleIndex < 0;

                    // 헤더 영역 전체(빈 여백 포함)를 emitter drag 영역으로 사용하려면
                    // 백그라운드 InvisibleButton 을 먼저 깔고 그 위에 실제 위젯을 얹는다.
                    // SetItemAllowOverlap 으로 위젯들이 정상적으로 클릭/호버를 가져가고,
                    // 어느 위젯도 잡지 않은 빈 픽셀은 백그라운드 InvisibleButton 이 받는다.
                    const ImVec2 ColStart    = ImGui::GetCursorScreenPos();
                    const float  ColW        = ImGui::GetContentRegionAvail().x;
                    float        HeaderZoneH = ImGui::GetFrameHeight() * 2.0f + 12.0f;

                    ImGui::SetNextItemAllowOverlap();
                    ImGui::InvisibleButton("##EmitterDragZone", ImVec2(ColW, HeaderZoneH));
                    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceNoDisableHover))
                    {
                        const int32 SrcIdx = EmitterIndex;
                        ImGui::SetDragDropPayload("PSE_EMITTER", &SrcIdx, sizeof(int32));
                        ImGui::Text("Move %s", EmitterLabel.c_str());
                        ImGui::EndDragDropSource();
                    }
                    if (ImGui::BeginDragDropTarget())
                    {
                        if (const ImGuiPayload* P = ImGui::AcceptDragDropPayload("PSE_EMITTER"))
                        {
                            EmitterDropSrc = *static_cast<const int32*>(P->Data);
                            EmitterDropDst = EmitterIndex;
                        }
                        ImGui::EndDragDropTarget();
                    }
                    // bg invisible button 이 hover 되면 이 컬럼을 활성 zone 으로 마킹.
                    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem))
                    {
                        if (const ImGuiPayload* EmCarryProbe = ImGui::GetDragDropPayload())
                        {
                            if (EmCarryProbe->IsDataType("PSE_EMITTER"))
                            {
                                PendingDropEmitter = EmitterIndex;
                                PendingDropSlot    = SlotEmitterColSentinel;
                            }
                        }
                    }
                    // 활성 zone (가장 가까운 컬럼) 만 외곽선 강조. 다른 컬럼은 표시 안 함.
                    if (const ImGuiPayload* EmCarry = ImGui::GetDragDropPayload())
                    {
                        if (EmCarry->IsDataType("PSE_EMITTER") && ActiveDropEmitter == EmitterIndex && ActiveDropSlot ==
                            SlotEmitterColSentinel)
                        {
                            ImGui::GetWindowDrawList()->AddRect(
                                ImVec2(ColStart.x, ColStart.y),
                                ImVec2(ColStart.x + ColW, ColStart.y + HeaderZoneH),
                                PSE::Accent,
                                3.0f,
                                0,
                                3.0f
                            );
                        }
                    }

                    // 실제 위젯들을 백그라운드 위로 겹쳐 렌더링. 각 위젯에 SetItemAllowOverlap.
                    // 위젯 자체도 drag source/target 으로 등록 — 위젯이 이벤트를 먼저 가져가므로
                    // 위젯 위에서 드래그를 시작/수신하려면 위젯에도 source/target 필요.
                    ImGui::SetCursorScreenPos(ColStart);

                    auto AttachEmitterDragOnLastItem = [&]()
                    {
                        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceNoDisableHover))
                        {
                            const int32 SrcIdx = EmitterIndex;
                            ImGui::SetDragDropPayload("PSE_EMITTER", &SrcIdx, sizeof(int32));
                            ImGui::Text("Move %s", EmitterLabel.c_str());
                            ImGui::EndDragDropSource();
                        }
                        if (ImGui::BeginDragDropTarget())
                        {
                            if (const ImGuiPayload* P = ImGui::AcceptDragDropPayload("PSE_EMITTER"))
                            {
                                EmitterDropSrc = *static_cast<const int32*>(P->Data);
                                EmitterDropDst = EmitterIndex;
                            }
                            ImGui::EndDragDropTarget();
                        }
                        // 위젯 자체 hover 도 emitter 컬럼 활성 zone 으로 마킹 — 아래 background
                        // bg button 이 못 받는 경우 (위젯이 입력 가로챔) 까지 커버.
                        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem))
                        {
                            if (const ImGuiPayload* C = ImGui::GetDragDropPayload())
                            {
                                if (C->IsDataType("PSE_EMITTER"))
                                {
                                    PendingDropEmitter = EmitterIndex;
                                    PendingDropSlot    = SlotEmitterColSentinel;
                                }
                            }
                        }
                    };

                    // x 버튼 폭만큼 셀렉터블 너비를 줄인다.
                    constexpr float CloseBtnW = 20.0f;
                    const float     RowW      = ImGui::GetContentRegionAvail().x;
                    if (ImGui::Selectable(
                        EmitterLabel.c_str(),
                        bHeaderSel,
                        ImGuiSelectableFlags_AllowOverlap,
                        ImVec2(RowW - CloseBtnW - 4.0f, 0.0f)
                    ))
                    {
                        SelectEmitter(EmitterIndex, -1);
                    }
                    AttachEmitterDragOnLastItem();
                    if (ImGui::BeginPopupContextItem("##EmitterCtx"))
                    {
                        if (ImGui::MenuItem("Delete Emitter", "Del"))
                        {
                            SelectEmitter(EmitterIndex, -1);
                            EmitterToDelete = EmitterIndex;
                        }
                        if (ImGui::MenuItem("Duplicate Emitter"))
                        {
                            EmitterToDuplicate = EmitterIndex;
                        }
                        ImGui::Separator();
                        bool bEnabled = Emitter ? Emitter->IsEnabled() : true;
                        if (ImGui::MenuItem("Enabled", nullptr, &bEnabled))
                        {
                            PushUndoSnapshot();
                            if (Emitter) Emitter->SetEnabled(bEnabled);
                            MarkDirty();
                            RestartPreviewSimulation();
                        }
                        ImGui::EndPopup();
                    }
                    ImGui::SameLine();
                    if (ImGui::SmallButton("x##del"))
                    {
                        SelectEmitter(EmitterIndex, -1);
                        EmitterToDelete = EmitterIndex;
                    }
                    if (ImGui::IsItemHovered())
                    {
                        ImGui::SetTooltip("Delete this emitter");
                    }

                    bool bEnabled = Emitter ? Emitter->IsEnabled() : true;
                    if (ImGui::Checkbox("Enabled##col", &bEnabled))
                    {
                        PushUndoSnapshot();
                        if (Emitter)
                        {
                            Emitter->SetEnabled(bEnabled);
                        }
                        MarkDirty();
                        RestartPreviewSimulation();
                    }
                    AttachEmitterDragOnLastItem();

                    ImGui::Separator();

                    TArray<FEmitterModuleEntry> ModuleList;
                    BuildEmitterModuleListAt(Emitter, SelectedLODIndex, ModuleList);

                    int32 ModuleToDelete           = -1;
                    int32 ModuleToDuplicateHigher  = -1;
                    int32 ModuleToShareHigher      = -1;
                    int32 ModuleToDuplicateHighest = -1;
                    int32 ModuleToRefresh          = -1;
                    // 드래그 중인지 미리 판정 (각 row 위의 갭 드롭존을 동적으로 키운다).
                    const ImGuiPayload* ActiveCarry       = ImGui::GetDragDropPayload();
                    const bool          bModuleDragActive = ActiveCarry && ActiveCarry->IsDataType("PSE_MODULE");

                    // 모듈 row 들은 모서리 라운드 없이 edge-to-edge fill + 패딩 0 으로 빽빽하게.
                    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));

                    // 컬럼의 좌/우 픽셀 좌표 (WindowPadding 무시) — row bg 를 edge 까지 채우기 위함.
                    const float ColLeftPx  = ImGui::GetWindowPos().x;
                    const float ColRightPx = ColLeftPx + ImGui::GetWindowSize().x;

                    for (int32 ModuleIndex = 0; ModuleIndex < static_cast<int32>(ModuleList.size()); ++ModuleIndex)
                    {
                        ImGui::PushID(ModuleIndex);
                        const FEmitterModuleEntry& Entry     = ModuleList[ModuleIndex];
                        const bool                 bSelected = bEmitterSelected && (SelectedModuleIndex == ModuleIndex);

                        UParticleLODLevel* SelLOD = Emitter ? Emitter->GetLODLevel(SelectedLODIndex) : nullptr;
                        const bool bIsCoreSlot = SelLOD && (Entry.Module == SelLOD->RequiredModule || Entry.Module ==
                            SelLOD->SpawnModule || Entry.Module == static_cast<UParticleModule*>(SelLOD->
                                TypeDataModule));

                        // 비-core 모듈은 LOD0 Modules 배열에서의 인덱스를 미리 계산. drop 위치 산정용.
                        int32 ThisRowArrayIdx = -1;
                        if (!bIsCoreSlot)
                        {
                            if (UParticleLODLevel* L0 = Emitter ? Emitter->GetLODLevel(0) : nullptr)
                            {
                                for (int32 i = 0; i < static_cast<int32>(L0->Modules.size()); ++i)
                                {
                                    if (L0->Modules[i] == Entry.Module)
                                    {
                                        ThisRowArrayIdx = i;
                                        break;
                                    }
                                }
                            }
                        }

                        // 활성 drop zone 이 이 row 바로 위 위치일 때만 14px 갭 + 사각형 강조 표시.
                        // 그 외 row 들은 갭 없이 평소처럼 붙어있음 → 한 곳만 벌어지는 효과.
                        // ★ 갭 영역 자체도 hover/drop target 으로 만들어 oscillation 방지 —
                        // 갭이 열리면서 row 가 아래로 밀려 마우스가 갭 위에 있게 되어도 같은
                        // PendingDropSlot 을 유지해, 다음 프레임에 갭이 닫혔다가 다시 열리는
                        // 진동을 막는다.
                        if (SelectedLODIndex == 0 && bModuleDragActive && ThisRowArrayIdx >= 0 &&
                            ActiveDropEmitter == EmitterIndex && ActiveDropSlot == ThisRowArrayIdx)
                        {
                            constexpr float GapH   = 14.0f;
                            const ImVec2    GapMin = ImGui::GetCursorScreenPos();
                            const float     GapW   = ColRightPx - GapMin.x;
                            ImGui::InvisibleButton("##activegap", ImVec2(GapW, GapH));
                            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem))
                            {
                                PendingDropEmitter = EmitterIndex;
                                PendingDropSlot    = ThisRowArrayIdx;
                            }
                            if (ImGui::BeginDragDropTarget())
                            {
                                if (const ImGuiPayload* P = ImGui::AcceptDragDropPayload("PSE_MODULE"))
                                {
                                    const int32* D  = static_cast<const int32*>(P->Data);
                                    ModuleDropSrcE  = D[0];
                                    ModuleDropSrcAi = D[1];
                                    ModuleDropDstE  = EmitterIndex;
                                    ModuleDropDstAi = ThisRowArrayIdx;
                                }
                                ImGui::EndDragDropTarget();
                            }
                            const ImVec2 GapMax = ImGui::GetItemRectMax();
                            // edge-to-edge 강조 사각형.
                            ImGui::GetWindowDrawList()->AddRectFilled(
                                ImVec2(ColLeftPx, GapMin.y),
                                ImVec2(ColRightPx, GapMax.y),
                                IM_COL32(74, 144, 255, 60),
                                0.0f
                            );
                            ImGui::GetWindowDrawList()->AddRect(
                                ImVec2(ColLeftPx, GapMin.y),
                                ImVec2(ColRightPx, GapMax.y),
                                PSE::Accent,
                                0.0f,
                                0,
                                2.0f
                            );
                        }

                        const bool bIsShared = IsModuleSharedWithHigher(Emitter, SelectedLODIndex, ModuleIndex);

                        // 좌측 enable 토글 아이콘 — sub-LOD에서 공유 중이면 회색.
                        ImDrawList*     DL          = ImGui::GetWindowDrawList();
                        const ImVec2    IconPos     = ImGui::GetCursorScreenPos();
                        constexpr float IconSize    = 14.0f;
                        const float     RowH        = ImGui::GetFrameHeight();
                        const float     IconYOff    = (RowH - IconSize) * 0.5f;
                        constexpr float LeftPad     = 4.0f;
                        const ImVec2    IconDrawPos(IconPos.x + LeftPad, IconPos.y + IconYOff);

                        // 모듈 카테고리 색으로 row 배경을 칠한다. edge-to-edge fill (컬럼 좌/우
                        // 끝까지) + 모서리 라운드 0. sub-LOD에서 공유 중이면 alpha 절반으로 dim.
                        {
                            ImU32 RowBgCol = GetModuleCategoryColor(Entry.Module);
                            if (bIsShared)
                            {
                                ImU32 A = (RowBgCol >> 24) & 0xFF;
                                A = A / 2;
                                RowBgCol = (RowBgCol & 0x00FFFFFF) | (A << 24);
                            }
                            DL->AddRectFilled(
                                ImVec2(ColLeftPx, IconPos.y),
                                ImVec2(ColRightPx, IconPos.y + RowH),
                                RowBgCol,
                                0.0f
                            );
                        }

                        const bool      bModEnabled = Entry.Module && (Entry.Module->bEnabled != 0);
                        const ImU32     IconCol     = (bIsCoreSlot || bIsShared) ? IM_COL32(110, 115, 125, 200)
                        : (bModEnabled ? PSE::Accent : IM_COL32(80, 84, 92, 255));
                        DL->AddRectFilled(
                            IconDrawPos,
                            ImVec2(IconDrawPos.x + IconSize, IconDrawPos.y + IconSize),
                            IconCol, 2.0f);
                        if (bModEnabled)
                        {
                            DL->AddLine(
                                ImVec2(IconDrawPos.x + 3.0f,              IconDrawPos.y + IconSize * 0.55f),
                                ImVec2(IconDrawPos.x + IconSize * 0.42f,  IconDrawPos.y + IconSize - 3.0f),
                                IM_COL32(255, 255, 255, 230), 1.6f);
                            DL->AddLine(
                                ImVec2(IconDrawPos.x + IconSize * 0.42f,  IconDrawPos.y + IconSize - 3.0f),
                                ImVec2(IconDrawPos.x + IconSize - 2.0f,   IconDrawPos.y + 3.0f),
                                IM_COL32(255, 255, 255, 230), 1.6f);
                        }
                        // InvisibleButton 의 layout 높이를 RowH 로 맞춰서 SameLine 베이스라인을 통일.
                        ImGui::InvisibleButton("##enableicon", ImVec2(IconSize + LeftPad * 2.0f, RowH));
                        if (!bIsCoreSlot && !bIsShared && ImGui::IsItemClicked() && Entry.Module)
                        {
                            PushUndoSnapshot();
                            Entry.Module->bEnabled = bModEnabled ? 0 : 1;
                            MarkDirty();
                            RestartPreviewSimulation();
                        }
                        if (ImGui::IsItemHovered())
                        {
                            ImGui::SetTooltip(
                                bIsCoreSlot ? "Required/Spawn/TypeData are always enabled" : (bIsShared
                                    ? "Shared from higher LOD — duplicate first to edit"
                                    : (bModEnabled ? "Disable module" : "Enable module"))
                            );
                        }
                        ImGui::SameLine(0.0f, 0.0f);

                        // Selectable 도 RowH 만큼 키우고 텍스트 세로 중앙 정렬 — 아이콘과 baseline 일치.
                        ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.0f, 0.5f));
                        if (bIsShared) ImGui::PushStyleColor(ImGuiCol_Text, PSE::DimTextV);
                        if (ImGui::Selectable(Entry.Name, bSelected, ImGuiSelectableFlags_AllowOverlap, ImVec2(0.0f, RowH)))
                        {
                            SelectEmitter(EmitterIndex, ModuleIndex);
                        }
                        if (bIsShared) ImGui::PopStyleColor();
                        ImGui::PopStyleVar();

                        // Module row 자체가 drop hover 감지 + drop target — hover 시 다음 프레임에
                        // "이 row 위 갭"이 활성으로 펼쳐진다. 비-core 모듈만 대상 (Required/Spawn/TypeData 제외).
                        if (SelectedLODIndex == 0 && bModuleDragActive && ThisRowArrayIdx >= 0)
                        {
                            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem))
                            {
                                PendingDropEmitter = EmitterIndex;
                                PendingDropSlot    = ThisRowArrayIdx;
                            }
                            if (ImGui::BeginDragDropTarget())
                            {
                                if (const ImGuiPayload* P = ImGui::AcceptDragDropPayload("PSE_MODULE"))
                                {
                                    const int32* D = static_cast<const int32*>(P->Data);
                                    ModuleDropSrcE  = D[0];
                                    ModuleDropSrcAi = D[1];
                                    ModuleDropDstE  = EmitterIndex;
                                    ModuleDropDstAi = ThisRowArrayIdx;
                                }
                                ImGui::EndDragDropTarget();
                            }
                        }

                        // 모듈 drag and drop — LOD 0 에서 비-core 모듈만. ArrayIndex 는 LOD0->Modules
                        // 내부 인덱스 (Required/Spawn/TypeData 는 별도 슬롯이라 제외).
                        int32 ModuleArrayIdx = -1;
                        if (UParticleLODLevel* L0 = Emitter ? Emitter->GetLODLevel(0) : nullptr)
                        {
                            for (int32 i = 0; i < static_cast<int32>(L0->Modules.size()); ++i)
                            {
                                if (L0->Modules[i] == Entry.Module)
                                {
                                    ModuleArrayIdx = i;
                                    break;
                                }
                            }
                        }
                        const bool bModuleDragOK = (SelectedLODIndex == 0) && !bIsCoreSlot && (ModuleArrayIdx >= 0);

                        if (bModuleDragOK && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceNoDisableHover))
                        {
                            const int32 Payload[2] = { EmitterIndex, ModuleArrayIdx };
                            ImGui::SetDragDropPayload("PSE_MODULE", Payload, sizeof(Payload));
                            ImGui::Text("Move %s", Entry.Name);
                            ImGui::EndDragDropSource();
                        }
                        // Row 자체는 drop target 아님 — 갭 zone 들이 insert 위치를 정확히
                        // 표시하므로 row-onto-row 드롭은 모호함만 만든다.

                        // 우측 curve 아이콘. Curve-capable 모듈에서는 실제 버튼처럼 동작한다.
                        const ImVec2 RowMin     = ImGui::GetItemRectMin();
                        const ImVec2 RowMax     = ImGui::GetItemRectMax();
                        const float  RowCenterY = (RowMin.y + RowMax.y) * 0.5f;
                        const ImVec2 CurveIconMin(ColRightPx - IconSize - 4.0f, RowCenterY - IconSize * 0.5f);
                        const ImVec2 CurveIconMax(ColRightPx - 4.0f, RowCenterY + IconSize * 0.5f);

                        const bool bCurveCapable = ParticleModuleHasCurveProperties(Entry.Module);
                        const bool bHasCurveTracks = ParticleModuleHasActiveCurveTracks(Entry.Module);
                        const bool bCurveIconHovered = ImGui::IsMouseHoveringRect(CurveIconMin, CurveIconMax);
                        const ImU32 CurveBorder = bHasCurveTracks
                            ? PSE::Accent
                            : (bCurveCapable ? IM_COL32(145, 150, 162, 230) : IM_COL32(70, 74, 84, 150));
                        const ImU32 CurveLine = bHasCurveTracks
                            ? IM_COL32(120, 185, 255, 255)
                            : (bCurveCapable ? IM_COL32(155, 160, 172, 230) : IM_COL32(90, 95, 105, 150));
                        const ImU32 CurveFill = bCurveIconHovered && bCurveCapable
                            ? IM_COL32(74, 144, 255, 45)
                            : IM_COL32(0, 0, 0, 0);

                        if (CurveFill != IM_COL32(0, 0, 0, 0))
                        {
                            DL->AddRectFilled(CurveIconMin, CurveIconMax, CurveFill, 2.0f);
                        }
                        DL->AddRect(CurveIconMin, CurveIconMax, CurveBorder, 2.0f);
                        const float CW = CurveIconMax.x - CurveIconMin.x;
                        const float CH = CurveIconMax.y - CurveIconMin.y;
                        for (int32 i = 0; i < 6; ++i)
                        {
                            const float T0 = static_cast<float>(i) / 6.0f;
                            const float T1 = static_cast<float>(i + 1) / 6.0f;
                            const float Y0 = CurveIconMin.y + CH * (0.5f + 0.35f * (i % 2 == 0 ? -1.0f : 1.0f));
                            const float Y1 = CurveIconMin.y + CH * (0.5f + 0.35f * ((i + 1) % 2 == 0 ? -1.0f : 1.0f));
                            DL->AddLine(
                                ImVec2(CurveIconMin.x + CW * T0, Y0),
                                ImVec2(CurveIconMin.x + CW * T1, Y1),
                                CurveLine,
                                1.0f
                            );
                        }

                        if (bCurveIconHovered)
                        {
                            if (bCurveCapable)
                            {
                                ImGui::SetTooltip(
                                    bHasCurveTracks
                                        ? "Open Curve Editor for this module"
                                        : "Open Curve Editor and create curves for this module");
                                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                                {
                                    SelectEmitter(EmitterIndex, ModuleIndex);
                                    bShowCurvePanel = true;
                                    SelectedCurveTrackIndex = -1;
                                    SelectedCurveTrackModule = nullptr;
                                    InlineCurveEditor.ResetSelection();
                                }
                            }
                            else
                            {
                                ImGui::SetTooltip("This module has no curve-capable distribution");
                            }
                        }

                        // ── 모듈 컨텍스트 메뉴 (LOD 인식) ──
                        if (ImGui::BeginPopupContextItem("##ModuleCtx"))
                        {
                            const bool bIsLOD0     = (SelectedLODIndex == 0);
                            const bool bIsRequired = SelLOD && Entry.Module == SelLOD->RequiredModule;
                            // 모듈 삭제 — LOD 0의 비-core 모듈만 가능 (구조 변경은 LOD 0에서만).
                            if (ImGui::MenuItem("모듈 삭제", "Del", false, bIsLOD0 && !bIsCoreSlot))
                            {
                                SelectEmitter(EmitterIndex, ModuleIndex);
                                ModuleToDelete = ModuleIndex;
                            }
                            if (!bIsLOD0 && ImGui::IsItemHovered()) ImGui::SetTooltip(
                                "Structural changes only in LOD 0"
                            );

                            // 모듈 새로고침.
                            if (ImGui::MenuItem("모듈 새로고침", nullptr, false, Entry.Module != nullptr))
                            {
                                ModuleToRefresh = ModuleIndex;
                            }

                            ImGui::Separator();

                            // Required 전용 — 머티리얼 동기화 / 머티리얼 사용.
                            if (ImGui::MenuItem("머티리얼 동기화", nullptr, false, bIsRequired))
                            {
                                if (auto* R = Cast<UParticleModuleRequired>(Entry.Module))
                                {
                                    R->ResolveMaterialFromSlot();
                                    MarkDirty();
                                    RestartPreviewSimulation();
                                }
                            }
                            if (ImGui::MenuItem("머티리얼 사용", nullptr, false, bIsRequired))
                            {
                                if (auto* R = Cast<UParticleModuleRequired>(Entry.Module))
                                {
                                    OpenMaterialForRequired(R);
                                }
                            }

                            ImGui::Separator();

                            // sub-LOD 전용 sharing 메뉴.
                            const bool bIsSubLOD = (SelectedLODIndex > 0);
                            if (ImGui::MenuItem("상위에서 복제", nullptr, false, bIsSubLOD && bIsShared))
                            {
                                ModuleToDuplicateHigher = ModuleIndex;
                            }
                            if (ImGui::MenuItem("상위에서 공유", nullptr, false, bIsSubLOD))
                            {
                                ModuleToShareHigher = ModuleIndex;
                            }
                            if (ImGui::MenuItem("최상에서 복제", nullptr, false, bIsSubLOD))
                            {
                                ModuleToDuplicateHighest = ModuleIndex;
                            }

                            ImGui::Separator();
                            bool bModEn = Entry.Module ? (Entry.Module->bEnabled != 0) : true;
                            if (ImGui::MenuItem("Enabled", nullptr, &bModEn, !bIsCoreSlot && !bIsShared))
                            {
                                PushUndoSnapshot();
                                if (Entry.Module) Entry.Module->bEnabled = bModEn ? 1 : 0;
                                MarkDirty();
                                RestartPreviewSimulation();
                            }
                            ImGui::EndPopup();
                        }
                        ImGui::PopID();
                    }
                    ImGui::PopStyleVar(); // ItemSpacing(0,0) for module loop

                    if (ModuleToDelete >= 0)
                    {
                        SelectEmitter(EmitterIndex, ModuleToDelete);
                        DeleteSelectedModule();
                    }
                    if (ModuleToDuplicateHigher >= 0)
                    {
                        DuplicateModuleFromHigherLOD(Emitter, SelectedLODIndex, ModuleToDuplicateHigher);
                    }
                    if (ModuleToShareHigher >= 0)
                    {
                        ShareModuleFromHigherLOD(Emitter, SelectedLODIndex, ModuleToShareHigher);
                    }
                    if (ModuleToDuplicateHighest >= 0)
                    {
                        DuplicateModuleFromHighestLOD(Emitter, SelectedLODIndex, ModuleToDuplicateHighest);
                    }
                    if (ModuleToRefresh >= 0 && ModuleToRefresh < static_cast<int32>(ModuleList.size()))
                    {
                        if (UParticleModule* M = ModuleList[ModuleToRefresh].Module)
                        {
                            M->RefreshModule();
                            MarkDirty();
                            RestartPreviewSimulation();
                        }
                    }

                    ImGui::Separator();

                    // 모듈 리스트 맨 끝 drop zone — 동일하게 활성 zone 만 펼침.
                    if (SelectedLODIndex == 0 && bModuleDragActive)
                    {
                        UParticleLODLevel* L0Append  = Emitter ? Emitter->GetLODLevel(0) : nullptr;
                        const bool         bIsActive = (ActiveDropEmitter == EmitterIndex && ActiveDropSlot ==
                            SlotAppendSentinel);
                        const float  ZoneH   = bIsActive ? 14.0f : 6.0f;
                        const ImVec2 ZoneMin = ImGui::GetCursorScreenPos();
                        const float  ZoneW   = ImGui::GetContentRegionAvail().x;
                        ImGui::InvisibleButton("##ModuleAppendDrop", ImVec2(ZoneW, ZoneH));
                        const bool bAppHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
                        if (bAppHovered)
                        {
                            PendingDropEmitter = EmitterIndex;
                            PendingDropSlot    = SlotAppendSentinel;
                        }
                        if (L0Append && ImGui::BeginDragDropTarget())
                        {
                            if (const ImGuiPayload* P = ImGui::AcceptDragDropPayload("PSE_MODULE"))
                            {
                                const int32* D  = static_cast<const int32*>(P->Data);
                                ModuleDropSrcE  = D[0];
                                ModuleDropSrcAi = D[1];
                                ModuleDropDstE  = EmitterIndex;
                                ModuleDropDstAi = static_cast<int32>(L0Append->Modules.size());
                            }
                            ImGui::EndDragDropTarget();
                        }
                        if (bIsActive)
                        {
                            const ImVec2 ZoneMax(ZoneMin.x + ZoneW, ZoneMin.y + ZoneH);
                            ImGui::GetWindowDrawList()->AddRectFilled(
                                ZoneMin, ZoneMax, IM_COL32(74, 144, 255, 60), 3.0f);
                            ImGui::GetWindowDrawList()->AddRect(
                                ZoneMin, ZoneMax, PSE::Accent, 3.0f, 0, 2.0f);
                        }
                    }

                    // + Module 팝업 — LOD0 에서만 노출 (구조 변경은 LOD 0 전용).
                    if (SelectedLODIndex == 0 && ImGui::SmallButton("+ Module"))
                    {
                        ImGui::OpenPopup("##AddModulePopup");
                    }
                    if (ImGui::BeginPopup("##AddModulePopup"))
                    {
                        UParticleLODLevel* LOD0 = Emitter ? Emitter->GetLODLevel(0) : nullptr;
                        UParticleModuleTypeDataBase* TypeData = LOD0 ? LOD0->TypeDataModule : nullptr;
                        const bool bMeshType   = Cast<UParticleModuleTypeDataMesh>(TypeData) != nullptr;
                        const bool bRibbonType = Cast<UParticleModuleTypeDataRibbon>(TypeData) != nullptr;
                        const bool bBeamType   = Cast<UParticleModuleTypeDataBeam2>(TypeData) != nullptr;

                        auto SyncNewModuleToSubLOD = [&](UParticleModule* New)
                        {
                            if (!Emitter || !LOD0 || !New) return;
                            const int32 LCount = static_cast<int32>(Emitter->GetLODLevels().size());
                            for (int32 L = 1; L < LCount; ++L)
                            {
                                if (UParticleLODLevel* Sub = Emitter->GetLODLevel(L))
                                {
                                    Sub->Modules.push_back(New);
                                    Sub->UpdateModuleLists();
                                }
                            }
                        };

                        auto AddItem = [&](const char* Label, bool bTypeAllowed, bool bExists, auto Creator)
                        {
                            if (ImGui::MenuItem(Label, nullptr, false, LOD0 && bTypeAllowed && !bExists))
                            {
                                UParticleModule* New = Creator(LOD0);
                                if (!New) return;
                                LOD0->Modules.push_back(New);
                                LOD0->UpdateModuleLists();
                                SyncNewModuleToSubLOD(New);
                                SelectEmitter(EmitterIndex, -1);
                                MarkDirty();
                                RestartPreviewSimulation();
                            }
                        };

                        AddItem(
                            "Lifetime",
                            true,
                            HasModuleOfType<UParticleModuleLifetime>(LOD0),
                            [](UParticleLODLevel* L)
                            {
                                auto* N = UObjectManager::Get().CreateObject<UParticleModuleLifetime>(L);
                                N->bEnabled           = true;
                                N->bSpawnModule       = true;
                                N->bUpdateModule      = false;
                                N->bFinalUpdateModule = false;
                                N->LifetimeMin        = 1.0f;
                                N->LifetimeMax        = 1.0f;
                                return static_cast<UParticleModule*>(N);
                            }
                        );
                        AddItem(
                            "Location",
                            true,
                            HasModuleOfType<UParticleModuleLocation>(LOD0),
                            [](UParticleLODLevel* L)
                            {
                                auto* N = UObjectManager::Get().CreateObject<UParticleModuleLocation>(L);
                                N->bEnabled           = true;
                                N->bSpawnModule       = true;
                                N->bUpdateModule      = false;
                                N->bFinalUpdateModule = false;
                                auto* D = UObjectManager::Get().CreateObject<UDistributionVectorUniform>(N);
                                D->Min = FVector(-1.0f, -1.0f, -1.0f);
                                D->Max = FVector(1.0f, 1.0f, 1.0f);
                                N->StartLocation.Distribution = D;
                                return static_cast<UParticleModule*>(N);
                            }
                        );
                        AddItem(
                            "Velocity",
                            true,
                            HasModuleOfType<UParticleModuleVelocity>(LOD0),
                            [](UParticleLODLevel* L)
                            {
                                auto* N = UObjectManager::Get().CreateObject<UParticleModuleVelocity>(L);
                                N->bEnabled           = true;
                                N->bSpawnModule       = true;
                                N->bUpdateModule      = false;
                                N->bFinalUpdateModule = false;
                                N->bInWorldSpace      = false;
                                N->bApplyOwnerScale   = false;
                                auto* V = UObjectManager::Get().CreateObject<UDistributionVectorUniform>(N);
                                V->Min = FVector(-1.0f, -1.0f, -1.0f);
                                V->Max = FVector(1.0f, 1.0f, 1.0f);
                                N->StartVelocity.Distribution = V;
                                auto* R = UObjectManager::Get().CreateObject<UDistributionFloatConstant>(N);
                                R->Constant = 0.0f;
                                N->StartVelocityRadial.Distribution = R;
                                return static_cast<UParticleModule*>(N);
                            }
                        );
                        AddItem(
                            "Size",
                            true,
                            HasModuleOfType<UParticleModuleSize>(LOD0),
                            [](UParticleLODLevel* L)
                            {
                                auto* N = UObjectManager::Get().CreateObject<UParticleModuleSize>(L);
                                N->bEnabled           = true;
                                N->bSpawnModule       = true;
                                N->bUpdateModule      = false;
                                N->bFinalUpdateModule = false;
                                auto* D = UObjectManager::Get().CreateObject<UDistributionVectorUniform>(N);
                                D->Min = FVector(0.0f, 0.0f, 0.0f);
                                D->Max = FVector(50.0f, 50.0f, 50.0f);
                                N->StartSize.Distribution = D;
                                return static_cast<UParticleModule*>(N);
                            }
                        );
                        AddItem(
                            "Color",
                            true,
                            HasModuleOfType<UParticleModuleColor>(LOD0),
                            [](UParticleLODLevel* L)
                            {
                                auto* N = UObjectManager::Get().CreateObject<UParticleModuleColor>(L);
                                N->bEnabled           = true;
                                N->bSpawnModule       = true;
                                N->bUpdateModule      = false;
                                N->bFinalUpdateModule = false;
                                N->bClampAlpha        = true;
                                auto* C = UObjectManager::Get().CreateObject<UDistributionVectorConstant>(N);
                                C->Constant = FVector(1.0f, 1.0f, 1.0f);
                                N->StartColor.Distribution = C;
                                auto* A = UObjectManager::Get().CreateObject<UDistributionFloatConstant>(N);
                                A->Constant = 1.0f;
                                N->StartAlpha.Distribution = A;
                                return static_cast<UParticleModule*>(N);
                            }
                        );
                        AddItem(
                            "Color Over Life",
                            true,
                            HasModuleOfType<UParticleModuleColorOverLife>(LOD0),
                            [](UParticleLODLevel* L)
                            {
                                auto* N = UObjectManager::Get().CreateObject<UParticleModuleColorOverLife>(L);
                                N->bEnabled           = true;
                                N->bSpawnModule       = true;
                                N->bUpdateModule      = true;
                                N->bFinalUpdateModule = false;
                                N->bClampAlpha        = true;
                                auto* C = UObjectManager::Get().CreateObject<UDistributionVectorConstant>(N);
                                C->Constant = FVector(1.0f, 1.0f, 1.0f);
                                N->ColorOverLife.Distribution = C;
                                auto* A = UObjectManager::Get().CreateObject<UDistributionFloatConstant>(N);
                                A->Constant = 1.0f;
                                N->AlphaOverLife.Distribution = A;
                                return static_cast<UParticleModule*>(N);
                            }
                        );
                        AddItem(
                            "Collision",
                            true,
                            HasModuleOfType<UParticleModuleCollision>(LOD0),
                            [](UParticleLODLevel* L)
                            {
                                auto* N = UObjectManager::Get().CreateObject<UParticleModuleCollision>(L);
                                N->bEnabled           = true;
                                N->bSpawnModule       = false;
                                N->bUpdateModule      = true;
                                N->bFinalUpdateModule = false;
                                N->Radius             = 1.0f;
                                N->Restitution        = 0.5f;
                                N->bKillOnCollision   = false;
                                return static_cast<UParticleModule*>(N);
                            }
                        );

                        ImGui::Separator();

                        AddItem(
                            "Mesh Material",
                            bMeshType,
                            HasModuleOfType<UParticleModuleMeshMaterial>(LOD0),
                            [](UParticleLODLevel* L)
                            {
                                auto* N = UObjectManager::Get().CreateObject<UParticleModuleMeshMaterial>(L);
                                N->bEnabled           = true;
                                N->bSpawnModule       = false;
                                N->bUpdateModule      = false;
                                N->bFinalUpdateModule = false;
                                return static_cast<UParticleModule*>(N);
                            }
                        );
                        AddItem(
                            "Mesh Rotation",
                            bMeshType,
                            HasModuleOfType<UParticleModuleMeshRotation>(LOD0),
                            [](UParticleLODLevel* L)
                            {
                                auto* N = UObjectManager::Get().CreateObject<UParticleModuleMeshRotation>(L);
                                N->bEnabled           = true;
                                N->bSpawnModule       = true;
                                N->bUpdateModule      = false;
                                N->bFinalUpdateModule = false;
                                return static_cast<UParticleModule*>(N);
                            }
                        );
                        AddItem(
                            "Mesh Rotation Rate",
                            bMeshType,
                            HasModuleOfType<UParticleModuleMeshRotationRate>(LOD0),
                            [](UParticleLODLevel* L)
                            {
                                auto* N = UObjectManager::Get().CreateObject<UParticleModuleMeshRotationRate>(L);
                                N->bEnabled           = true;
                                N->bSpawnModule       = true;
                                N->bUpdateModule      = true;
                                N->bFinalUpdateModule = false;
                                return static_cast<UParticleModule*>(N);
                            }
                        );

                        ImGui::Separator();

                        AddItem(
                            "Spawn Per Unit",
                            bRibbonType,
                            HasModuleOfType<UParticleModuleSpawnPerUnit>(LOD0),
                            [](UParticleLODLevel* L)
                            {
                                auto* N = UObjectManager::Get().CreateObject<UParticleModuleSpawnPerUnit>(L);
                                N->bEnabled                    = true;
                                N->bSpawnModule                = false;
                                N->bUpdateModule               = false;
                                N->bFinalUpdateModule          = false;
                                N->bProcessSpawnRate           = true;
                                N->bProcessBurstList           = true;
                                N->UnitScalar                  = 1.0f;
                                N->MovementTolerance           = 0.1f;
                                N->SpawnPerUnit                = 1.0f;
                                N->MaxFrameDistance            = 0.0f;
                                N->bIgnoreSpawnRateWhenMoving  = false;
                                N->bIgnoreMovementAlongX       = false;
                                N->bIgnoreMovementAlongY       = false;
                                N->bIgnoreMovementAlongZ       = false;
                                return static_cast<UParticleModule*>(N);
                            }
                        );
                        AddItem(
                            "Trail Source",
                            bRibbonType,
                            HasModuleOfType<UParticleModuleTrailSource>(LOD0),
                            [](UParticleLODLevel* L)
                            {
                                auto* N = UObjectManager::Get().CreateObject<UParticleModuleTrailSource>(L);
                                N->bEnabled           = true;
                                N->bSpawnModule       = false;
                                N->bUpdateModule      = false;
                                N->bFinalUpdateModule = false;
                                return static_cast<UParticleModule*>(N);
                            }
                        );

                        ImGui::Separator();

                        AddItem(
                            "Beam Source",
                            bBeamType,
                            HasModuleOfType<UParticleModuleBeamSource>(LOD0),
                            [](UParticleLODLevel* L)
                            {
                                auto* N = UObjectManager::Get().CreateObject<UParticleModuleBeamSource>(L);
                                N->bEnabled           = true;
                                N->bSpawnModule       = true;
                                N->bUpdateModule      = true;
                                N->bFinalUpdateModule = false;
                                return static_cast<UParticleModule*>(N);
                            }
                        );
                        AddItem(
                            "Beam Target",
                            bBeamType,
                            HasModuleOfType<UParticleModuleBeamTarget>(LOD0),
                            [](UParticleLODLevel* L)
                            {
                                auto* N = UObjectManager::Get().CreateObject<UParticleModuleBeamTarget>(L);
                                N->bEnabled           = true;
                                N->bSpawnModule       = true;
                                N->bUpdateModule      = true;
                                N->bFinalUpdateModule = false;
                                return static_cast<UParticleModule*>(N);
                            }
                        );
                        AddItem(
                            "Beam Noise",
                            bBeamType,
                            HasModuleOfType<UParticleModuleBeamNoise>(LOD0),
                            [](UParticleLODLevel* L)
                            {
                                auto* N = UObjectManager::Get().CreateObject<UParticleModuleBeamNoise>(L);
                                N->bEnabled           = true;
                                N->bSpawnModule       = true;
                                N->bUpdateModule      = true;
                                N->bFinalUpdateModule = false;
                                return static_cast<UParticleModule*>(N);
                            }
                        );
                        AddItem(
                            "Beam Source Modifier",
                            bBeamType,
                            HasBeamModifierOfType(LOD0, PEB2MT_Source),
                            [](UParticleLODLevel* L)
                            {
                                auto* N = UObjectManager::Get().CreateObject<UParticleModuleBeamModifier>(L);
                                N->bEnabled           = true;
                                N->bSpawnModule       = true;
                                N->bUpdateModule      = true;
                                N->bFinalUpdateModule = false;
                                N->ModifierType       = PEB2MT_Source;
                                return static_cast<UParticleModule*>(N);
                            }
                        );
                        AddItem(
                            "Beam Target Modifier",
                            bBeamType,
                            HasBeamModifierOfType(LOD0, PEB2MT_Target),
                            [](UParticleLODLevel* L)
                            {
                                auto* N = UObjectManager::Get().CreateObject<UParticleModuleBeamModifier>(L);
                                N->bEnabled           = true;
                                N->bSpawnModule       = true;
                                N->bUpdateModule      = true;
                                N->bFinalUpdateModule = false;
                                N->ModifierType       = PEB2MT_Target;
                                return static_cast<UParticleModule*>(N);
                            }
                        );
                        ImGui::EndPopup();
                    }

                    // ── 빈 영역 우클릭 → TypeData(Sprite/Mesh/Ribbon/Beam) 전환 ──────
                    // ImGuiPopupFlags_NoOpenOverItems: 모듈 row(Selectable)/버튼 위에서는
                    // 이 메뉴가 안 뜨고, 진짜 빈 공간에서만 뜸. 모듈 컨텍스트 메뉴와 충돌 X.
                    if (ImGui::BeginPopupContextWindow("##EmitterTypeDataCtx",
                            ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
                    {
                        ImGui::TextColored(PSE::DimTextV, "Emitter Type");
                        ImGui::Separator();

                        UParticleLODLevel*           LOD0Probe = Emitter ? Emitter->GetLODLevel(0) : nullptr;
                        UParticleModuleTypeDataBase* CurType   = LOD0Probe ? LOD0Probe->TypeDataModule : nullptr;

                        const bool bIsSprite = (CurType == nullptr);
                        const bool bIsMesh   = (Cast<UParticleModuleTypeDataMesh>(CurType) != nullptr);
                        const bool bIsRibbon = (Cast<UParticleModuleTypeDataRibbon>(CurType) != nullptr);
                        const bool bIsBeam2  = (Cast<UParticleModuleTypeDataBeam2>(CurType) != nullptr);

                        if (ImGui::MenuItem("Sprite", nullptr, bIsSprite))
                        {
                            SetEmitterTypeData(EmitterIndex, nullptr);
                        }
                        if (ImGui::MenuItem("Mesh Data", nullptr, bIsMesh))
                        {
                            SetEmitterTypeData(EmitterIndex, "UParticleModuleTypeDataMesh");
                        }
                        if (ImGui::MenuItem("Ribbon Data", nullptr, bIsRibbon))
                        {
                            SetEmitterTypeData(EmitterIndex, "UParticleModuleTypeDataRibbon");
                        }
                        if (ImGui::MenuItem("Beam Data", nullptr, bIsBeam2))
                        {
                            SetEmitterTypeData(EmitterIndex, "UParticleModuleTypeDataBeam2");
                        }
                        ImGui::EndPopup();
                    }
                }
                ImGui::EndChild();
                ImGui::PopStyleVar();
                ImGui::PopStyleColor();
                ImGui::PopID();
            }

            // cascade 끝의 '+' 컬럼 — 새 이미터 추가.
            if (EmitterCount > 0)
            {
                ImGui::SameLine();
            }
            ImGui::PushStyleColor(ImGuiCol_Border, PSE::Border32);
            ImGui::PushStyleColor(ImGuiCol_ChildBg, PSE::FrameBg);
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
            const float AddColH = (std::max)(ImGui::GetContentRegionAvail().y, 80.0f);
            if (ImGui::BeginChild("##AddEmitterCol", ImVec2(46.0f, AddColH), true))
            {
                if (ImGui::Button("+", ImVec2(-1.0f, -1.0f)))
                {
                    AddEmitter();
                }
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("Add a new sprite emitter");
                }
            }
            ImGui::EndChild();
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(2);

            // 컬럼 순회 후 한 번에 처리해야 이터레이션 도중 배열 변동을 피할 수 있다.
            if (EmitterToDuplicate >= 0)
            {
                DuplicateEmitter(EmitterToDuplicate);
            }
            else if (EmitterToDelete >= 0)
            {
                SelectEmitter(EmitterToDelete, -1);
                DeleteSelectedEmitter();
            }
            else if (ModuleDropSrcE >= 0 && ModuleDropDstE >= 0)
            {
                MoveModule(ModuleDropSrcE, ModuleDropSrcAi, ModuleDropDstE, ModuleDropDstAi);
            }
            else if (EmitterDropSrc >= 0 && EmitterDropDst >= 0)
            {
                MoveEmitter(EmitterDropSrc, EmitterDropDst);
            }
        }
        ImGui::EndChild();
    }
    EndPanel();
}

// -----------------------------------------------------------------------------
// Details panel and module property editors
// -----------------------------------------------------------------------------

void FParticleSystemEditorWidget::RenderPropertiesPanel(float Width, float Height)
{
    UParticleSystem* ParticleSystem = GetParticleSystem();

    FString Context = "Particle System";

    UParticleEmitter* SelectedEmitter = nullptr;
    if (ParticleSystem && SelectedEmitterIndex >= 0 && SelectedEmitterIndex < static_cast<int32>(ParticleSystem->
        GetEmitters().size()))
    {
        SelectedEmitter = ParticleSystem->GetEmitters()[SelectedEmitterIndex];
    }

    TArray<FEmitterModuleEntry> ModuleList;
    BuildEmitterModuleListAt(SelectedEmitter, SelectedLODIndex, ModuleList);

    UParticleModule* SelectedModule     = nullptr;
    const char*      SelectedModuleName = nullptr;
    if (SelectedModuleIndex >= 0 && SelectedModuleIndex < static_cast<int32>(ModuleList.size()))
    {
        SelectedModule     = ModuleList[SelectedModuleIndex].Module;
        SelectedModuleName = ModuleList[SelectedModuleIndex].Name;
    }

    if (SelectedEmitterIndex >= 0)
    {
        Context = "Emitter " + std::to_string(SelectedEmitterIndex);
        if (SelectedModuleName)
        {
            Context += "  >  ";
            Context += SelectedModuleName;
        }
    }

    if (BeginPanel("##PSEProperties", "Details", Width, Height, Context.c_str()))
    {
        // 상단 검색창 — 레퍼런스 Cascade의 Details 검색 상자. 현재는 시각용.
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputTextWithHint("##PSEPropSearch", "Search", PropertySearch, sizeof(PropertySearch));
        ImGui::Spacing();

        // 위젯이 패널 폭을 다 먹어 라벨이 잘리는 일이 없도록 우측에 160px를 라벨 영역으로 남긴다.
        ImGui::PushItemWidth(-160.0f);

        if (SelectedEmitterIndex < 0)
        {
            // ── Particle System ──
            ImGui::SetNextItemOpen(true, ImGuiCond_Once);
            if (ImGui::CollapsingHeader("Particle System"))
            {
                ImGui::TextColored(PSE::DimTextV, "Source Path");
                ImGui::TextWrapped(
                    "%s",
                    ParticleSystem && !ParticleSystem->GetSourcePath().empty() ? ParticleSystem->GetSourcePath().c_str()
                    : "(unsaved)"
                );

                ImGui::TextColored(
                    PSE::DimTextV,
                    "Emitters: %d",
                    ParticleSystem ? static_cast<int32>(ParticleSystem->GetEmitters().size()) : 0
                );
                ImGui::TextColored(PSE::DimTextV, "Status: %s", IsDirty() ? "Modified" : "Saved");

                // 시스템 단위 필드 — 실제 UParticleSystem UPROPERTY 들과 연결.
                if (ParticleSystem)
                {
                    bool bSysChanged = false;
                    bSysChanged      |= ImGui::Combo(
                        "System Update Mode",
                        &ParticleSystem->SystemUpdateMode,
                        "EPSUM_RealTime\0EPSUM_FixedTime\0\0"
                    );
                    bSysChanged |= ImGui::DragFloat(
                        "Update Time FPS",
                        &ParticleSystem->UpdateTimeFPS,
                        1.0f,
                        1.0f,
                        240.0f
                    );
                    bSysChanged |= ImGui::DragFloat("Warmup Time", &ParticleSystem->WarmupTime, 0.05f, 0.0f, 1000.0f);
                    bSysChanged |= ImGui::DragFloat(
                        "Warmup Tick Rate",
                        &ParticleSystem->WarmupTickRate,
                        0.05f,
                        0.0f,
                        1000.0f
                    );
                    bSysChanged |= ImGui::Checkbox(
                        "Orient ZAxis Toward Camera",
                        &ParticleSystem->bOrientZAxisTowardCamera
                    );
                    bSysChanged |= ImGui::DragFloat(
                        "Seconds Before Inactive",
                        &ParticleSystem->SecondsBeforeInactive,
                        0.1f,
                        0.0f,
                        1000.0f
                    );
                    if (bSysChanged) MarkDirty();
                }
            }

            // ── Thumbnail ──
            if (ImGui::CollapsingHeader("Thumbnail"))
            {
                if (ParticleSystem)
                {
                    bool bThumbChanged = false;
                    bThumbChanged      |= ImGui::DragFloat(
                        "Thumbnail Warmup",
                        &ParticleSystem->ThumbnailWarmup,
                        0.1f,
                        0.0f,
                        60.0f
                    );
                    bThumbChanged |= ImGui::Checkbox("Use Realtime Thumbnail", &ParticleSystem->bUseRealtimeThumbnail);
                    if (bThumbChanged) MarkDirty();
                }
            }

            // ── LOD ──
            ImGui::SetNextItemOpen(true, ImGuiCond_Once);
            if (ImGui::CollapsingHeader("LOD"))
            {
                if (ParticleSystem)
                {
                    bool bLODChanged = false;
                    bLODChanged      |= ImGui::DragFloat(
                        "LOD Distance Check Time",
                        &ParticleSystem->LODDistanceCheckTime,
                        0.05f,
                        0.0f,
                        10.0f
                    );
                    bLODChanged |= ImGui::Combo(
                        "LOD Method",
                        &ParticleSystem->LODMethod,
                        "Automatic\0DirectSet\0ActivateAutomatic\0\0"
                    );
                    if (bLODChanged) MarkDirty();
                }

                // LODDistances는 실제 UPROPERTY → 편집 가능.
                if (ParticleSystem)
                {
                    SyncParticleSystemLODDistances(ParticleSystem);
                    TArray<float>& Dist = ParticleSystem->LODDistances;
                    ImGui::Text("LOD Distances (%d)", static_cast<int32>(Dist.size()));
                    bool  bDistChanged = false;
                    int32 RemoveAt     = -1;
                    for (int32 i = 0; i < static_cast<int32>(Dist.size()); ++i)
                    {
                        ImGui::PushID(i);
                        char Lbl[24];
                        std::snprintf(Lbl, sizeof(Lbl), "LOD %d", i);
                        if (i == 0) ImGui::BeginDisabled();
                        float Value = Dist[i];
                        if (ImGui::DragFloat(Lbl, &Value, 1.0f, 0.0f, 100000.0f))
                        {
                            Dist[i]      = (std::max)(0.0f, Value);
                            bDistChanged = true;
                        }
                        if (i == 0) ImGui::EndDisabled();
                        if (i > 0)
                        {
                            ImGui::SameLine();
                            if (ImGui::SmallButton("x")) { RemoveAt = i; }
                        }
                        ImGui::PopID();
                    }
                    if (RemoveAt > 0)
                    {
                        RemoveLODAt(RemoveAt);
                        bDistChanged = false;
                    }
                    if (ImGui::SmallButton("+ LOD"))
                    {
                        SelectLOD(GetParticleSystemLODCount(ParticleSystem) - 1);
                        AddLODAfterSelected();
                        bDistChanged = false;
                    }
                    if (bDistChanged) MarkDirty();
                }
            }
        }
        else if (!SelectedModule)
        {
            // 이미터 자체.
            ImGui::SetNextItemOpen(true, ImGuiCond_Once);
            if (ImGui::CollapsingHeader("Emitter"))
            {
                bool bChanged = false;

                // 이름 버퍼는 선택이 바뀔 때만 동기화.
                if (EmitterNameBufFor != SelectedEmitterIndex && SelectedEmitter)
                {
                    const FString s   = SelectedEmitter->EmitterName.ToString();
                    const size_t  len = (std::min)(s.size(), sizeof(EmitterNameBuf) - 1);
                    std::memcpy(EmitterNameBuf, s.c_str(), len);
                    EmitterNameBuf[len] = '\0';
                    EmitterNameBufFor   = SelectedEmitterIndex;
                }
                if (ImGui::InputText("Name", EmitterNameBuf, sizeof(EmitterNameBuf)))
                {
                    if (SelectedEmitter)
                    {
                        SelectedEmitter->EmitterName = FName(FString(EmitterNameBuf));
                        bChanged                     = true;
                    }
                }

                bool bEnabled = SelectedEmitter ? SelectedEmitter->IsEnabled() : true;
                if (ImGui::Checkbox("Enabled", &bEnabled))
                {
                    PushUndoSnapshot();
                    if (SelectedEmitter) SelectedEmitter->SetEnabled(bEnabled);
                    bChanged = true;
                    RestartPreviewSimulation();
                }

                if (SelectedEmitter)
                {
                    bChanged |= ImGui::DragInt(
                        "Initial Alloc Count",
                        &SelectedEmitter->InitialAllocationCount,
                        1.0f,
                        0,
                        100000
                    );
                    bChanged |= ImGui::DragFloat3("Pivot Offset", SelectedEmitter->PivotOffset.Data, 0.01f);
                }

                if (bChanged) MarkDirty();
            }

            ImGui::SetNextItemOpen(true, ImGuiCond_Once);
            if (ImGui::CollapsingHeader("LOD"))
            {
                const int32 LODCount = SelectedEmitter ? static_cast<int32>(SelectedEmitter->GetLODLevels().size()) : 0;
                ImGui::Text("Levels: %d", LODCount);
                ImGui::Text("Modules in LOD0: %d", static_cast<int32>(ModuleList.size()));
            }
        }
        else
        {
            // 모듈 편집. sub-LOD에서 공유 중인 모듈은 편집 금지 — 안내 + BeginDisabled.
            const bool bIsShared = IsModuleSharedWithHigher(SelectedEmitter, SelectedLODIndex, SelectedModuleIndex);
            if (bIsShared)
            {
                ImGui::TextColored(
                    ImVec4(1.0f, 0.85f, 0.40f, 1.0f),
                    "이 모듈은 LOD %d 와 공유 중 — 편집하려면 우클릭 > '상위에서 복제'를 선택하세요.",
                    SelectedLODIndex - 1
                );
                ImGui::Spacing();
                ImGui::BeginDisabled();
            }
            RenderModuleProperties(SelectedModule);
            if (bIsShared) ImGui::EndDisabled();
        }

        ImGui::PopItemWidth();
    }
    EndPanel();
}

void FParticleSystemEditorWidget::RenderModuleProperties(UParticleModule* Module)
{
    if (!Module)
    {
        return;
    }

    bool bChanged       = false;
    bool bMaterialDirty = false;
    bool bApplyPreviewBeamSourcePoint = false;
    bool bApplyPreviewBeamTargetPoint = false;
    FVector PreviewBeamSourcePoint = FVector::ZeroVector;
    FVector PreviewBeamTargetPoint = FVector::ZeroVector;

    // Required/Spawn/TypeData는 이미터 동작에 필수라 disable 토글이 무의미하다. 그 외 모듈만 노출.
    const bool bIsCoreModule = Cast<UParticleModuleRequired>(Module)
        || Cast<UParticleModuleSpawn>(Module)
        || Cast<UParticleModuleTypeDataBase>(Module);
    if (!bIsCoreModule)
    {
        bool bModuleEnabled = Module->bEnabled != 0;
        if (ImGui::Checkbox("Module Enabled", &bModuleEnabled))
        {
            PushUndoSnapshot();
            Module->bEnabled = bModuleEnabled ? 1 : 0;
            bChanged         = true;
            RestartPreviewSimulation();
        }
        ImGui::Spacing();
    }

    if (UParticleModuleRequired* Required = Cast<UParticleModuleRequired>(Module))
    {
        // ── Material (헤더 위) ──
        const FString CurrentSlot = Required->MaterialSlot.ToString();
        const bool    bSlotNone   = (CurrentSlot.empty() || CurrentSlot == "None");
        const FString Preview     = bSlotNone ? FString("None") : CurrentSlot;

        if (ImGui::BeginCombo("Material", Preview.c_str()))
        {
            if (ImGui::Selectable("None", bSlotNone))
            {
                Required->MaterialSlot = "None";
                Required->ResolveMaterialFromSlot();
                bChanged = bMaterialDirty = true;
            }
            if (bSlotNone) ImGui::SetItemDefaultFocus();

            const TArray<FMaterialAssetListItem>& MatFiles = FMaterialManager::Get().GetAvailableMaterialFiles();
            for (const FMaterialAssetListItem& Item : MatFiles)
            {
                const bool bSelected = (CurrentSlot == Item.FullPath);
                if (ImGui::Selectable(Item.DisplayName.c_str(), bSelected))
                {
                    // SetMaterial은 MaterialSlot을 머티리얼 JSON 내부의 PathFileName으로 덮어쓰는데,
                    // 그 값이 실제 파일 경로와 다르면 다음 Tick의 ResolveMaterialFromSlot에서
                    // 폴백 머티리얼로 떨어진다. 파일 경로를 그대로 슬롯에 저장해 자기참조 일관성을 유지한다.
                    Required->MaterialSlot = Item.FullPath;
                    Required->ResolveMaterialFromSlot();
                    bChanged = bMaterialDirty = true;
                }
                if (bSelected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        if (ImGui::Button("Open Material"))
        {
            OpenMaterialForRequired(Required);
        }
        ImGui::SameLine();
        if (ImGui::Button("Duplicate Material For This Emitter"))
        {
            DuplicateMaterialForRequired(Required);
            bMaterialDirty = true;
        }
        UParticleEmitter* OwnerEmitter = nullptr;
        if (UParticleSystem* ParticleSystem = GetParticleSystem())
        {
            if (SelectedEmitterIndex >= 0 && SelectedEmitterIndex < static_cast<int32>(ParticleSystem->GetEmitters().size()))
            {
                OwnerEmitter = ParticleSystem->GetEmitters()[SelectedEmitterIndex];
            }
        }
        const UParticleLODLevel* OwnerLOD = OwnerEmitter ? OwnerEmitter->GetLODLevel(SelectedLODIndex) : nullptr;
        const bool bMeshEmitter = OwnerLOD && Cast<UParticleModuleTypeDataMesh>(OwnerLOD->TypeDataModule);
        const EMaterialDomain ExpectedDomain = bMeshEmitter ? EMaterialDomain::ParticleMesh : EMaterialDomain::ParticleSprite;
        if (Required->Material && Required->Material->GetDomain() != ExpectedDomain)
        {
            ImGui::TextColored(
                ImVec4(1.0f, 0.80f, 0.25f, 1.0f),
                "Selected material domain is %s (expected %s).",
                MaterialDomainName(Required->Material->GetDomain()),
                MaterialDomainName(ExpectedDomain)
            );
        }
        ImGui::Spacing();

        // ── Emitter ──
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        if (ImGui::CollapsingHeader("Emitter##Req"))
        {
            bChanged |= ImGui::DragFloat3("Origin", Required->EmitterOrigin.Data, 0.1f);

            float RotPYR[3] = { Required->EmitterRotation.Pitch,
                                Required->EmitterRotation.Yaw,
                                Required->EmitterRotation.Roll
            };
            if (ImGui::DragFloat3("Rotation P/Y/R", RotPYR, 0.5f))
            {
                Required->EmitterRotation.Pitch = RotPYR[0];
                Required->EmitterRotation.Yaw   = RotPYR[1];
                Required->EmitterRotation.Roll  = RotPYR[2];
                bChanged                        = true;
            }
            bChanged |= ImGui::DragFloat("Duration", &Required->EmitterDuration, 0.05f, 0.0f, 10000.0f);
            bChanged |= ImGui::DragFloat("Delay", &Required->EmitterDelay, 0.05f, 0.0f, 10000.0f);
            bChanged |= ImGui::DragInt("Loops", &Required->EmitterLoops, 1.0f, 0, 10000);
        }

        // ── Rendering ──
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        if (ImGui::CollapsingHeader("Rendering##Req"))
        {
            bChanged |= ImGui::Checkbox("Use Max Draw Count", &Required->bUseMaxDrawCount);
            if (!Required->bUseMaxDrawCount) ImGui::BeginDisabled();
            bChanged |= ImGui::DragInt("Max Draw Count", &Required->MaxDrawCount, 1.0f, 0, 100000);
            if (Required->MaxDrawCount < 0)
            {
                Required->MaxDrawCount = 0;
                bChanged               = true;
            }
            if (!Required->bUseMaxDrawCount) ImGui::EndDisabled();
        }

        // ── Flags ──
        if (ImGui::CollapsingHeader("Flags##Req"))
        {
            bool bUseLocal = Required->bUseLocalSpace;
            if (ImGui::Checkbox("Use Local Space", &bUseLocal))
            {
                Required->bUseLocalSpace = bUseLocal ? 1 : 0;
                bChanged                 = true;
            }
        }
    }
    else if (UParticleModuleSpawn* Spawn = Cast<UParticleModuleSpawn>(Module))
    {
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        if (ImGui::CollapsingHeader("Spawn"))
        {
            bChanged |= ImGui::DragFloat("Spawn Rate", &Spawn->SpawnRate, 0.1f, 0.0f, 10000.0f);
            bChanged |= ImGui::DragFloat("Spawn Rate Scale", &Spawn->SpawnRateScale, 0.01f, 0.0f, 100.0f);
        }
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        if (ImGui::CollapsingHeader("Bursts"))
        {
            bChanged |= ImGui::DragFloat("Burst Scale", &Spawn->BurstScale, 0.01f, 0.0f, 100.0f);
            ImGui::Spacing();
            RenderBurstList(Spawn->BurstList);
        }
    }
    else if (UParticleModuleLifetime* Lifetime = Cast<UParticleModuleLifetime>(Module))
    {
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        if (ImGui::CollapsingHeader("Lifetime"))
        {
            bChanged |= ImGui::DragFloat("Min", &Lifetime->LifetimeMin, 0.05f, 0.0f, 10000.0f);
            bChanged |= ImGui::DragFloat("Max", &Lifetime->LifetimeMax, 0.05f, 0.0f, 10000.0f);
            if (Lifetime->LifetimeMax < Lifetime->LifetimeMin)
            {
                Lifetime->LifetimeMax = Lifetime->LifetimeMin;
                bChanged              = true;
            }
        }
    }
    else if (UParticleModuleLocation* Location = Cast<UParticleModuleLocation>(Module))
    {
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        if (ImGui::CollapsingHeader("Location"))
        {
            DrawRawDistributionVector("Start Location", Location->StartLocation, bChanged, Location);
        }
    }
    else if (UParticleModuleVelocity* Velocity = Cast<UParticleModuleVelocity>(Module))
    {
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        if (ImGui::CollapsingHeader("Velocity"))
        {
            DrawRawDistributionVector("Start Velocity", Velocity->StartVelocity, bChanged, Velocity);
            DrawRawDistributionFloat("Start Velocity Radial", Velocity->StartVelocityRadial, bChanged, Velocity);
            bool bWorld = Velocity->bInWorldSpace;
            if (ImGui::Checkbox("In World Space", &bWorld))
            {
                Velocity->bInWorldSpace = bWorld ? 1 : 0;
                bChanged = true;
            }
            bool bOwnerScale = Velocity->bApplyOwnerScale;
            if (ImGui::Checkbox("Apply Owner Scale", &bOwnerScale))
            {
                Velocity->bApplyOwnerScale = bOwnerScale ? 1 : 0;
                bChanged = true;
            }
        }
    }
    else if (UParticleModuleSize* Size = Cast<UParticleModuleSize>(Module))
    {
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        if (ImGui::CollapsingHeader("Size"))
        {
            DrawRawDistributionVector("Start Size", Size->StartSize, bChanged, Size);
        }
    }
    else if (UParticleModuleColorOverLife* ColorOverLife = Cast<UParticleModuleColorOverLife>(Module))
    {
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        if (ImGui::CollapsingHeader("Color"))
        {
            DrawRawDistributionVector("Color Over Life", ColorOverLife->ColorOverLife, bChanged, ColorOverLife);
            DrawRawDistributionFloat("Alpha Over Life", ColorOverLife->AlphaOverLife, bChanged, ColorOverLife);
            bool bClamp = ColorOverLife->bClampAlpha;
            if (ImGui::Checkbox("Clamp Alpha", &bClamp))
            {
                ColorOverLife->bClampAlpha = bClamp ? 1 : 0;
                bChanged                   = true;
            }
        }
    }
    else if (UParticleModuleColor* Color = Cast<UParticleModuleColor>(Module))
    {
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        if (ImGui::CollapsingHeader("Color"))
        {
            // Initial Color is sampled once in UParticleModuleColor::Spawn() and stored as BaseColor.
            // Do not expose it as a life curve; ColorOverLife owns time-varying color.
            DrawRawDistributionVector("Start Color", Color->StartColor, bChanged, Color, false);
            DrawRawDistributionFloat("Start Alpha", Color->StartAlpha, bChanged, Color, false);
            bool bClamp = Color->bClampAlpha;
            if (ImGui::Checkbox("Clamp Alpha", &bClamp))
            {
                Color->bClampAlpha = bClamp ? 1 : 0;
                bChanged           = true;
            }
        }
    }
    else if (UParticleModuleEventGenerator* Generator = Cast<UParticleModuleEventGenerator>(Module))
    {
        (void)Generator;
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        if (ImGui::CollapsingHeader("Event"))
        {
            ImGui::TextColored(PSE::DimTextV, "Event Generator UI is hidden until dispatch path is fully connected.");
        }
    }
    else if (UParticleModuleCollision* Collision = Cast<UParticleModuleCollision>(Module))
    {
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        if (ImGui::CollapsingHeader("Collision"))
        {
            bool bKillOnCollision = Collision->bKillOnCollision;
            if (ImGui::Checkbox("Kill On Collision", &bKillOnCollision))
            {
                Collision->bKillOnCollision = bKillOnCollision;
                bChanged                    = true;
            }
        }
    }
    else if (UParticleModuleMeshMaterial* MeshMaterial = Cast<UParticleModuleMeshMaterial>(Module))
    {
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        if (ImGui::CollapsingHeader("Mesh Material"))
        {
            int32 SectionCount = 1;
            if (UParticleSystem* PS = GetParticleSystem())
            {
                if (SelectedEmitterIndex >= 0 && SelectedEmitterIndex < static_cast<int32>(PS->GetEmitters().size()))
                {
                    if (UParticleEmitter* Emitter = PS->GetEmitters()[SelectedEmitterIndex])
                    {
                        if (UParticleLODLevel* LOD = Emitter->GetLODLevel(SelectedLODIndex))
                        {
                            if (UParticleModuleTypeDataMesh* MeshType = Cast<UParticleModuleTypeDataMesh>(LOD->TypeDataModule))
                            {
                                if (UStaticMesh* StaticMesh = MeshType->GetStaticMesh())
                                {
                                    SectionCount = (std::max)(1, static_cast<int32>(StaticMesh->GetLODSections(0).size()));
                                }
                            }
                        }
                    }
                }
            }

            if (static_cast<int32>(MeshMaterial->MeshMaterialSlots.size()) < SectionCount)
            {
                MeshMaterial->MeshMaterialSlots.resize(SectionCount, FSoftObjectPtr("None"));
                MeshMaterial->MeshMaterials.resize(SectionCount, nullptr);
            }

            const TArray<FMaterialAssetListItem>& MatFiles = FMaterialManager::Get().GetAvailableMaterialFiles();
            for (int32 SectionIdx = 0; SectionIdx < SectionCount; ++SectionIdx)
            {
                ImGui::PushID(SectionIdx);
                FString CurrentSlot = MeshMaterial->MeshMaterialSlots[SectionIdx].ToString();
                if (CurrentSlot.empty()) CurrentSlot = "None";
                if (ImGui::BeginCombo("Material", CurrentSlot.c_str()))
                {
                    const bool bNoneSelected = (CurrentSlot == "None");
                    if (ImGui::Selectable("None", bNoneSelected))
                    {
                        MeshMaterial->MeshMaterialSlots[SectionIdx] = "None";
                        MeshMaterial->MeshMaterials[SectionIdx] = nullptr;
                        bChanged = true;
                    }

                    for (const FMaterialAssetListItem& Item : MatFiles)
                    {
                        const bool bSelected = (CurrentSlot == Item.FullPath);
                        if (ImGui::Selectable(Item.DisplayName.c_str(), bSelected))
                        {
                            MeshMaterial->MeshMaterialSlots[SectionIdx] = Item.FullPath;
                            bChanged = true;
                        }
                    }
                    ImGui::EndCombo();
                }
                ImGui::SameLine();
                ImGui::TextColored(PSE::DimTextV, "Section %d", SectionIdx);

                if (SectionIdx < static_cast<int32>(MeshMaterial->MeshMaterials.size()))
                {
                    UMaterial* Mat = MeshMaterial->MeshMaterials[SectionIdx];
                    if (Mat && Mat->GetDomain() != EMaterialDomain::ParticleMesh)
                    {
                        ImGui::TextColored(
                            ImVec4(1.0f, 0.80f, 0.25f, 1.0f),
                            "Domain is %s (expected ParticleMesh).",
                            MaterialDomainName(Mat->GetDomain())
                        );
                    }
                }
                ImGui::PopID();
            }

            if (bChanged)
            {
                MeshMaterial->ResolveMaterials();
            }
        }
    }
    else if (UParticleModuleMeshRotation* MeshRotation = Cast<UParticleModuleMeshRotation>(Module))
    {
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        if (ImGui::CollapsingHeader("Mesh Rotation"))
        {
            DrawRawDistributionVector("Start Rotation", MeshRotation->StartRotation, bChanged, MeshRotation);
            bool bInherit = MeshRotation->bInheritParent;
            if (ImGui::Checkbox("Inherit Parent", &bInherit))
            {
                MeshRotation->bInheritParent = bInherit ? 1 : 0;
                bChanged = true;
            }
        }
    }
    else if (UParticleModuleMeshRotationRate* MeshRotationRate = Cast<UParticleModuleMeshRotationRate>(Module))
    {
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        if (ImGui::CollapsingHeader("Mesh Rotation Rate"))
        {
            DrawRawDistributionVector("Start Rotation Rate", MeshRotationRate->StartRotationRate, bChanged, MeshRotationRate);
        }
    }
    else if (UParticleModuleSpawnPerUnit* SpawnPerUnit = Cast<UParticleModuleSpawnPerUnit>(Module))
    {
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        if (ImGui::CollapsingHeader("Spawn Per Unit Module"))
        {
            bChanged |= ImGui::DragFloat("Unit Scalar", &SpawnPerUnit->UnitScalar, 0.01f, 0.0f, 100000.0f);
            bChanged |= ImGui::DragFloat("Movement Tolerance", &SpawnPerUnit->MovementTolerance, 0.01f, 0.0f, 100000.0f);
            bChanged |= ImGui::DragFloat("Spawn Per Unit", &SpawnPerUnit->SpawnPerUnit, 0.01f, 0.0f, 100000.0f);
            bChanged |= ImGui::DragFloat("Max Frame Distance", &SpawnPerUnit->MaxFrameDistance, 0.01f, 0.0f, 100000.0f);
            bool bFlag = SpawnPerUnit->bIgnoreSpawnRateWhenMoving;
            if (ImGui::Checkbox("Ignore Spawn Rate When Moving", &bFlag)) { SpawnPerUnit->bIgnoreSpawnRateWhenMoving = bFlag ? 1 : 0; bChanged = true; }
            bFlag = SpawnPerUnit->bIgnoreMovementAlongX;
            if (ImGui::Checkbox("Ignore Movement Along X", &bFlag)) { SpawnPerUnit->bIgnoreMovementAlongX = bFlag ? 1 : 0; bChanged = true; }
            bFlag = SpawnPerUnit->bIgnoreMovementAlongY;
            if (ImGui::Checkbox("Ignore Movement Along Y", &bFlag)) { SpawnPerUnit->bIgnoreMovementAlongY = bFlag ? 1 : 0; bChanged = true; }
            bFlag = SpawnPerUnit->bIgnoreMovementAlongZ;
            if (ImGui::Checkbox("Ignore Movement Along Z", &bFlag)) { SpawnPerUnit->bIgnoreMovementAlongZ = bFlag ? 1 : 0; bChanged = true; }
            bFlag = SpawnPerUnit->bProcessSpawnRate;
            if (ImGui::Checkbox("Process Spawn Rate", &bFlag)) { SpawnPerUnit->bProcessSpawnRate = bFlag ? 1 : 0; bChanged = true; }
            bFlag = SpawnPerUnit->bProcessBurstList;
            if (ImGui::Checkbox("Process Burst List", &bFlag)) { SpawnPerUnit->bProcessBurstList = bFlag ? 1 : 0; bChanged = true; }
        }
    }
    else if (UParticleModuleTrailSource* TrailSource = Cast<UParticleModuleTrailSource>(Module))
    {
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        if (ImGui::CollapsingHeader("Trail Source"))
        {
            if (TrailSource->SourceMethod != PET2SRCM_Default)
            {
                ImGui::TextColored(
                    ImVec4(1.0f, 0.80f, 0.25f, 1.0f),
                    "Particle/Actor source methods are not supported yet."
                );
                ImGui::SameLine();
                if (ImGui::SmallButton("Reset to Default"))
                {
                    TrailSource->SourceMethod = PET2SRCM_Default;
                    bChanged = true;
                }
            }
            ImGui::TextColored(PSE::DimTextV, "Source Method: Default");

            DrawRawDistributionFloat("Source Strength", TrailSource->SourceStrength, bChanged, TrailSource);
            bool bLock = TrailSource->bLockSourceStrength;
            if (ImGui::Checkbox("Lock Source Strength", &bLock))
            {
                TrailSource->bLockSourceStrength = bLock ? 1 : 0;
                bChanged = true;
            }

            int32 OffsetCount = TrailSource->SourceOffsetCount;
            if (ImGui::DragInt("Source Offset Count", &OffsetCount, 1.0f, 0, 64))
            {
                TrailSource->SourceOffsetCount = (std::max)(0, OffsetCount);
                TrailSource->SourceOffsetDefaults.resize(TrailSource->SourceOffsetCount, FVector::ZeroVector);
                bChanged = true;
            }
            for (int32 OffsetIdx = 0; OffsetIdx < TrailSource->SourceOffsetCount; ++OffsetIdx)
            {
                ImGui::PushID(OffsetIdx);
                if (OffsetIdx >= static_cast<int32>(TrailSource->SourceOffsetDefaults.size()))
                {
                    TrailSource->SourceOffsetDefaults.resize(OffsetIdx + 1, FVector::ZeroVector);
                }
                bChanged |= ImGui::DragFloat3("Offset", TrailSource->SourceOffsetDefaults[OffsetIdx].Data, 0.1f);
                ImGui::SameLine();
                ImGui::TextColored(PSE::DimTextV, "#%d", OffsetIdx);
                ImGui::PopID();
            }

            int32 SelectionMethod = static_cast<int32>(TrailSource->SelectionMethod);
            if (ImGui::Combo("Selection Method", &SelectionMethod, "Random\0Sequential\0"))
            {
                TrailSource->SelectionMethod = static_cast<EParticleSourceSelectionMethod>(SelectionMethod);
                bChanged = true;
            }
            bool bInherit = TrailSource->bInheritRotation;
            if (ImGui::Checkbox("Inherit Rotation", &bInherit))
            {
                TrailSource->bInheritRotation = bInherit ? 1 : 0;
                bChanged = true;
            }
        }
    }
    else if (UParticleModuleBeamSource* BeamSource = Cast<UParticleModuleBeamSource>(Module))
    {
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        if (ImGui::CollapsingHeader("Beam Source"))
        {
            if (BeamSource->SourceMethod != PEB2STM_Default && BeamSource->SourceMethod != PEB2STM_UserSet)
            {
                BeamSource->SourceMethod = PEB2STM_Default;
                bChanged = true;
            }
            if (BeamSource->bLockSource ||
                BeamSource->bLockSourceTangent || BeamSource->bLockSourceStrength ||
                BeamSource->SourceTangentMethod != PEB2STTM_Distribution)
            {
                BeamSource->bLockSource = 0;
                BeamSource->bLockSourceTangent = 0;
                BeamSource->bLockSourceStrength = 0;
                BeamSource->SourceTangentMethod = PEB2STTM_Distribution;
                bChanged = true;
            }

            int32 Method = (BeamSource->SourceMethod == PEB2STM_UserSet) ? 1 : 0;
            if (ImGui::Combo("Source Method", &Method, "Default\0UserSet\0"))
            {
                BeamSource->SourceMethod = (Method == 0) ? PEB2STM_Default : PEB2STM_UserSet;
                bChanged = true;
            }

            if (BeamSource->SourceMethod == PEB2STM_UserSet)
            {
                FVector& UserSetPoint = GetPreviewBeamUserSetPoint(
                    GPreviewBeamUserSetSourcePoints, BeamSource, PreviewPSC, SelectedEmitterIndex, true);
                ImGui::DragFloat3("UserSet Source Point", UserSetPoint.Data, 0.1f);
                PreviewBeamSourcePoint = UserSetPoint;
                bApplyPreviewBeamSourcePoint = true;
            }
            else
            {
                DrawRawDistributionVector("Source", BeamSource->Source, bChanged, BeamSource);
            }

            bool bSourceAbsolute = BeamSource->bSourceAbsolute != 0;
            if (ImGui::Checkbox("Source Absolute", &bSourceAbsolute))
            {
                BeamSource->bSourceAbsolute = bSourceAbsolute ? 1 : 0;
                bChanged                    = true;
            }
            
            DrawRawDistributionVector("Source Tangent", BeamSource->SourceTangent, bChanged, BeamSource);
            DrawRawDistributionFloat("Source Strength", BeamSource->SourceStrength, bChanged, BeamSource);
        }
    }
    else if (UParticleModuleBeamTarget* BeamTarget = Cast<UParticleModuleBeamTarget>(Module))
    {
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        if (ImGui::CollapsingHeader("Beam Target"))
        {
            if (BeamTarget->TargetMethod != PEB2STM_Default && BeamTarget->TargetMethod != PEB2STM_UserSet)
            {
                BeamTarget->TargetMethod = PEB2STM_Default;
                bChanged = true;
            }
            if (BeamTarget->bLockTarget ||
                BeamTarget->bLockTargetTangent || BeamTarget->bLockTargetStrength ||
                BeamTarget->TargetTangentMethod != PEB2STTM_Distribution)
            {
                BeamTarget->bLockTarget = 0;
                BeamTarget->bLockTargetTangent = 0;
                BeamTarget->bLockTargetStrength = 0;
                BeamTarget->TargetTangentMethod = PEB2STTM_Distribution;
                bChanged = true;
            }

            int32 Method = (BeamTarget->TargetMethod == PEB2STM_UserSet) ? 1 : 0;
            if (ImGui::Combo("Target Method", &Method, "Default\0UserSet\0"))
            {
                BeamTarget->TargetMethod = (Method == 0) ? PEB2STM_Default : PEB2STM_UserSet;
                bChanged = true;
            }

            if (BeamTarget->TargetMethod == PEB2STM_UserSet)
            {
                FVector& UserSetPoint = GetPreviewBeamUserSetPoint(
                    GPreviewBeamUserSetTargetPoints, BeamTarget, PreviewPSC, SelectedEmitterIndex, false);
                ImGui::DragFloat3("UserSet Target Point", UserSetPoint.Data, 0.1f);
                PreviewBeamTargetPoint = UserSetPoint;
                bApplyPreviewBeamTargetPoint = true;
            }
            else
            {
                DrawRawDistributionVector("Target", BeamTarget->Target, bChanged, BeamTarget);
            }
            bool bTargetAbsolute = BeamTarget->bTargetAbsolute != 0;
            if (ImGui::Checkbox("Target Absolute", &bTargetAbsolute))
            {
                BeamTarget->bTargetAbsolute = bTargetAbsolute ? 1 : 0;
                bChanged                    = true;
            }
            
            DrawRawDistributionVector("Target Tangent", BeamTarget->TargetTangent, bChanged, BeamTarget);
            DrawRawDistributionFloat("Target Strength", BeamTarget->TargetStrength, bChanged, BeamTarget);
        }
    }
    else if (UParticleModuleBeamNoise* BeamNoise = Cast<UParticleModuleBeamNoise>(Module))
    {
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        if (ImGui::CollapsingHeader("Beam Noise"))
        {
            bool bFlag = BeamNoise->bLowFreq_Enabled;
            if (ImGui::Checkbox("Low Freq Enabled", &bFlag)) { BeamNoise->bLowFreq_Enabled = bFlag ? 1 : 0; bChanged = true; }
            bChanged |= ImGui::DragInt("Frequency", &BeamNoise->Frequency, 1.0f, 0, 4096);
            bChanged |= ImGui::DragInt("Frequency Low Range", &BeamNoise->Frequency_LowRange, 1.0f, 0, 4096);
            DrawRawDistributionVector("Noise Range", BeamNoise->NoiseRange, bChanged, BeamNoise);
            DrawRawDistributionFloat("Noise Range Scale", BeamNoise->NoiseRangeScale, bChanged, BeamNoise);
            bFlag = BeamNoise->bNRScaleEmitterTime;
            if (ImGui::Checkbox("NR Scale Emitter Time", &bFlag)) { BeamNoise->bNRScaleEmitterTime = bFlag ? 1 : 0; bChanged = true; }
            DrawRawDistributionVector("Noise Speed", BeamNoise->NoiseSpeed, bChanged, BeamNoise);
            bFlag = BeamNoise->bSmooth;
            if (ImGui::Checkbox("Smooth", &bFlag)) { BeamNoise->bSmooth = bFlag ? 1 : 0; bChanged = true; }
            bChanged |= ImGui::DragFloat("Noise Lock Radius", &BeamNoise->NoiseLockRadius, 0.1f, 0.0f, 100000.0f);
            bFlag = BeamNoise->bNoiseLock;
            if (ImGui::Checkbox("Noise Lock", &bFlag)) { BeamNoise->bNoiseLock = bFlag ? 1 : 0; bChanged = true; }
            bFlag = BeamNoise->bOscillate;
            if (ImGui::Checkbox("Oscillate", &bFlag)) { BeamNoise->bOscillate = bFlag ? 1 : 0; bChanged = true; }
            bChanged |= ImGui::DragFloat("Noise Lock Time", &BeamNoise->NoiseLockTime, 0.01f, -1.0f, 100000.0f);
            bChanged |= ImGui::DragFloat("Noise Tension", &BeamNoise->NoiseTension, 0.01f, 0.0f, 1000.0f);
            bFlag = BeamNoise->bUseNoiseTangents;
            if (ImGui::Checkbox("Use Noise Tangents", &bFlag)) { BeamNoise->bUseNoiseTangents = bFlag ? 1 : 0; bChanged = true; }
            DrawRawDistributionFloat("Noise Tangent Strength", BeamNoise->NoiseTangentStrength, bChanged, BeamNoise);
            bChanged |= ImGui::DragInt("Noise Tessellation", &BeamNoise->NoiseTessellation, 1.0f, 0, 4096);
            bFlag = BeamNoise->bTargetNoise;
            if (ImGui::Checkbox("Target Noise", &bFlag)) { BeamNoise->bTargetNoise = bFlag ? 1 : 0; bChanged = true; }
            bChanged |= ImGui::DragFloat("Frequency Distance", &BeamNoise->FrequencyDistance, 0.1f, 0.0f, 100000.0f);
            bFlag = BeamNoise->bApplyNoiseScale;
            if (ImGui::Checkbox("Apply Noise Scale", &bFlag)) { BeamNoise->bApplyNoiseScale = bFlag ? 1 : 0; bChanged = true; }
            DrawRawDistributionFloat("Noise Scale", BeamNoise->NoiseScale, bChanged, BeamNoise);
        }
    }
    else if (UParticleModuleBeamModifier* BeamModifier = Cast<UParticleModuleBeamModifier>(Module))
    {
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        if (ImGui::CollapsingHeader("Beam Modifier"))
        {
            (void)BeamModifier;
            ImGui::TextColored(PSE::DimTextV, "Hidden in the simplified Beam UI. Existing values are preserved.");
        }
    }
    else if (UParticleModuleTypeDataMesh* MeshT = Cast<UParticleModuleTypeDataMesh>(Module))
    {
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        if (ImGui::CollapsingHeader("Mesh"))
        {
            FString CurrentPath = MeshT->MeshAssetPath.ToString();
            if (CurrentPath.empty()) CurrentPath = "None";

            const TArray<FAssetListItem>& MeshFiles = FMeshManager::GetAvailableStaticMeshFiles();
            if (ImGui::BeginCombo("Static Mesh", CurrentPath.c_str()))
            {
                const bool bNoneSelected = (CurrentPath == "None");
                if (ImGui::Selectable("None", bNoneSelected))
                {
                    MeshT->MeshAssetPath = FString("None");
                    MeshT->Mesh = nullptr;
                    bChanged = true;
                }
                for (const FAssetListItem& Item : MeshFiles)
                {
                    const bool bSelected = (CurrentPath == Item.FullPath);
                    if (ImGui::Selectable(Item.DisplayName.c_str(), bSelected))
                    {
                        MeshT->MeshAssetPath = Item.FullPath;
                        ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
                        MeshT->Mesh = FMeshManager::LoadStaticMesh(Item.FullPath, Device);
                        bChanged = true;
                    }
                }
                ImGui::EndCombo();
            }

            // 저장된 자산을 다시 열었을 때 캐시 미스로 Mesh=null 이면 자동 로드.
            if (!MeshT->Mesh && !CurrentPath.empty() && CurrentPath != "None")
            {
                ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
                if (Device)
                {
                    MeshT->Mesh = FMeshManager::LoadStaticMesh(CurrentPath, Device);
                }
            }

            bool bOM = MeshT->bOverrideMaterial;   if (ImGui::Checkbox("Override Material", &bOM))   { MeshT->bOverrideMaterial  = bOM ? 1 : 0; bChanged = true; }
            ImGui::TextColored(PSE::DimTextV, "Resolved Mesh: %s", MeshT->Mesh ? "Yes" : "No");
        }
    }
    else if (UParticleModuleTypeDataRibbon* RibT = Cast<UParticleModuleTypeDataRibbon>(Module))
    {
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        if (ImGui::CollapsingHeader("Ribbon"))
        {
            bChanged |= ImGui::DragInt("Max Tessellation Between Particles", &RibT->MaxTessellationBetweenParticles, 1.0f, 0, 64);
            bChanged |= ImGui::DragInt("Sheets Per Trail",  &RibT->SheetsPerTrail,  1.0f, 1, 32);
            bChanged |= ImGui::DragInt("Max Trail Count",   &RibT->MaxTrailCount,   1.0f, 1, 1024);
            bChanged |= ImGui::DragInt("Max Particles In Trail Count", &RibT->MaxParticleInTrailCount, 1.0f, 0, 100000);
            bChanged |= ImGui::DragFloat("Tangent Spawning Scalar",     &RibT->TangentSpawningScalar,     0.01f, 0.0f, 100.0f);
            bChanged |= ImGui::DragFloat("Tiling Distance",             &RibT->TilingDistance,            0.1f, 0.0f, 10000.0f);
            bChanged |= ImGui::DragFloat("Distance Tessellation Step",  &RibT->DistanceTessellationStepSize, 0.1f, 0.1f, 1000.0f);
            bChanged |= ImGui::DragFloat("Tangent Tessellation Scalar", &RibT->TangentTessellationScalar, 0.1f, 0.0f, 1000.0f);
            int32 Ax = static_cast<int32>(RibT->RenderAxis);
            if (ImGui::Combo("Render Axis", &Ax, "CameraUp\0SourceUp\0WorldUp\0"))
            { RibT->RenderAxis = static_cast<ETrailsRenderAxisOption>(Ax); bChanged = true; }
            bool bA;
            bA = RibT->bDeadTrailsOnDeactivate; if (ImGui::Checkbox("Dead Trails On Deactivate", &bA))   { RibT->bDeadTrailsOnDeactivate = bA ? 1 : 0; bChanged = true; }
            bA = RibT->bDeadTrailsOnSourceLoss; if (ImGui::Checkbox("Dead Trails On Source Loss", &bA))  { RibT->bDeadTrailsOnSourceLoss = bA ? 1 : 0; bChanged = true; }
            bA = RibT->bClipSourceSegement;     if (ImGui::Checkbox("Clip Source Segment", &bA))        { RibT->bClipSourceSegement     = bA ? 1 : 0; bChanged = true; }
            bA = RibT->bSpawnInitialParticle;   if (ImGui::Checkbox("Spawn Initial Particle", &bA))     { RibT->bSpawnInitialParticle   = bA ? 1 : 0; bChanged = true; }
            bA = RibT->bRenderGeometry;         if (ImGui::Checkbox("Render Geometry", &bA))            { RibT->bRenderGeometry         = bA ? 1 : 0; bChanged = true; }
            bA = RibT->bEnableTangentDiffInterpScale;
            if (ImGui::Checkbox("Enable Tangent Diff Interp Scale", &bA))
            {
                RibT->bEnableTangentDiffInterpScale = bA ? 1 : 0;
                bChanged = true;
            }
        }
    }
    else if (UParticleModuleTypeDataBeam2* BeamT = Cast<UParticleModuleTypeDataBeam2>(Module))
    {
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        if (ImGui::CollapsingHeader("Beam"))
        {
            int32 Method = static_cast<int32>(BeamT->BeamMethod);
            if (Method > 1)
            {
                ImGui::TextColored(PSE::DimTextV, "Branch beam method is not supported yet.");
            }
            int32 ComboMethod = (Method > 1) ? 0 : Method;
            if (ImGui::Combo("Beam Method", &ComboMethod, "Distance\0Target\0"))
            { BeamT->BeamMethod = static_cast<EBeam2Method>(ComboMethod); bChanged = true; }
            bChanged |= ImGui::DragInt  ("Texture Tile",          &BeamT->TextureTile,          1.0f, 1, 64);
            bChanged |= ImGui::DragFloat("Texture Tile Distance", &BeamT->TextureTileDistance,  1.0f, 0.0f, 100000.0f);
            bChanged |= ImGui::DragInt  ("Sheets",                &BeamT->Sheets,               1.0f, 1, 32);
            bChanged |= ImGui::DragInt  ("Max Beam Count",        &BeamT->MaxBeamCount,         1.0f, 1, 1024);
            bChanged |= ImGui::DragFloat("Speed",                 &BeamT->Speed,                0.1f, 0.0f, 100000.0f);
            bChanged |= ImGui::DragInt  ("Interpolation Points",  &BeamT->InterpolationPoints,  1.0f, 0, 100);
            bChanged |= ImGui::DragInt  ("Up Vector Step Size",   &BeamT->UpVectorStepSize,     1.0f, 0, 100);
            bool bAO = BeamT->bAlwaysOn;         if (ImGui::Checkbox("Always On",          &bAO)) { BeamT->bAlwaysOn         = bAO ? 1 : 0; bChanged = true; }
            DrawRawDistributionFloat("Distance", BeamT->Distance, bChanged, BeamT);
            int32 Taper = static_cast<int32>(BeamT->TaperMethod);
            if (ImGui::Combo("Taper Method", &Taper, "None\0Full\0Partial\0"))
            { BeamT->TaperMethod = static_cast<EBeamTaperMethod>(Taper); bChanged = true; }
            DrawRawDistributionFloat("Taper Factor", BeamT->TaperFactor, bChanged, BeamT);
            DrawRawDistributionFloat("Taper Scale", BeamT->TaperScale, bChanged, BeamT);
            bool bRG = BeamT->RenderGeometry;    if (ImGui::Checkbox("Render Geometry",    &bRG)) { BeamT->RenderGeometry    = bRG ? 1 : 0; bChanged = true; }
        }
    }
    else if (Cast<UParticleModuleTypeDataBase>(Module))
    {
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        if (ImGui::CollapsingHeader("TypeData"))
        {
            ImGui::TextColored(PSE::DimTextV, "TypeData modules expose no editable properties yet.");
        }
    }
    else
    {
        ImGui::TextColored(PSE::DimTextV, "No editable properties exposed for this module.");
    }

    if (bChanged || bMaterialDirty)
    {
        MarkDirty();
        RestartPreviewSimulation();
    }
    if (bApplyPreviewBeamSourcePoint)
    {
        ApplyPreviewBeamUserSetPoint(PreviewPSC, SelectedEmitterIndex, true, PreviewBeamSourcePoint);
    }
    if (bApplyPreviewBeamTargetPoint)
    {
        ApplyPreviewBeamUserSetPoint(PreviewPSC, SelectedEmitterIndex, false, PreviewBeamTargetPoint);
    }
}

void FParticleSystemEditorWidget::RenderBurstList(TArray<FParticleBurst>& Bursts)
{
    bool  bChanged = false;
    int32 ToRemove = -1;

    constexpr ImGuiTableFlags TableFlags = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg |
    ImGuiTableFlags_SizingStretchSame;

    if (ImGui::BeginTable("##Bursts", 3, TableFlags))
    {
        ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthStretch, 1.2f);
        ImGui::TableSetupColumn("Count", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 24.0f);
        ImGui::TableHeadersRow();

        for (int32 i = 0; i < static_cast<int32>(Bursts.size()); ++i)
        {
            ImGui::PushID(i);
            FParticleBurst& B = Bursts[i];

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-1.0f);
            bChanged |= ImGui::DragFloat("##t", &B.Time, 0.005f, 0.0f, 1.0f);

            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-1.0f);
            bChanged |= ImGui::DragInt("##c", &B.Count, 1.0f, 0, 100000);

            ImGui::TableNextColumn();
            if (ImGui::SmallButton("x"))
            {
                ToRemove = i;
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    if (ToRemove >= 0)
    {
        Bursts.erase(Bursts.begin() + ToRemove);
        bChanged = true;
    }

    if (ImGui::SmallButton("+ Burst"))
    {
        FParticleBurst NewBurst;
        NewBurst.Time     = 0.0f;
        NewBurst.Count    = 0;
        NewBurst.CountLow = -1;
        Bursts.push_back(NewBurst);
        bChanged = true;
    }

    if (bChanged)
    {
        MarkDirty();
        RestartPreviewSimulation();
    }
}

// -----------------------------------------------------------------------------
// Curve editor panel
// -----------------------------------------------------------------------------

namespace
{
    constexpr float CurveIconSize = 22.0f;
    constexpr float CurveToolbarH = 48.0f;
    constexpr float TrackListW = 240.0f;

    const char* CurvePropertyKindName(EParticleCurvePropertyKind Kind)
    {
        switch (Kind)
        {
        case EParticleCurvePropertyKind::Float:
            return "Float";
        case EParticleCurvePropertyKind::VectorRGB:
            return "Color RGB";
        case EParticleCurvePropertyKind::VectorXYZ:
        default:
            return "Vector XYZ";
        }
    }
}

void FParticleSystemEditorWidget::RenderCurveEditorPanel(float Width, float Height)
{
    UParticleSystem* ParticleSystem = GetParticleSystem();

    UParticleEmitter* SelectedEmitter = nullptr;
    if (ParticleSystem && SelectedEmitterIndex >= 0 && SelectedEmitterIndex < static_cast<int32>(ParticleSystem->GetEmitters().size()))
    {
        SelectedEmitter = ParticleSystem->GetEmitters()[SelectedEmitterIndex];
    }

    TArray<FEmitterModuleEntry> ModuleList;
    BuildEmitterModuleListAt(SelectedEmitter, SelectedLODIndex, ModuleList);

    const FEmitterModuleEntry* SelectedEntry = nullptr;
    if (SelectedModuleIndex >= 0 && SelectedModuleIndex < static_cast<int32>(ModuleList.size()))
    {
        SelectedEntry = &ModuleList[SelectedModuleIndex];
    }

    UParticleModule* SelectedModule = SelectedEntry ? SelectedEntry->Module : nullptr;
    if (SelectedModule != SelectedCurveTrackModule)
    {
        SelectedCurveTrackIndex = -1;
        SelectedCurveTrackModule = SelectedModule;
        InlineCurveEditor.ResetSelection();
    }

    TArray<FParticleCurveProperty> Properties;
    TArray<FParticleCurveTrack> Tracks;
    CollectParticleCurveProperties(SelectedModule, Properties);
    CollectParticleCurveTracks(SelectedModule, Tracks);

    auto RefreshTracks = [&]()
    {
        CollectParticleCurveProperties(SelectedModule, Properties);
        CollectParticleCurveTracks(SelectedModule, Tracks);
        if (SelectedCurveTrackIndex >= static_cast<int32>(Tracks.size()))
        {
            SelectedCurveTrackIndex = -1;
            InlineCurveEditor.ResetSelection();
        }
    };

    auto ApplyCurveStructureChange = [&]()
    {
        MarkDirty();
        if (SelectedModule)
        {
            SelectedModule->RefreshModule();
        }
        RestartPreviewSimulation();
    };

    auto SelectFirstTrackForProperty = [&](const FString& PropertyName)
    {
        SelectedCurveTrackIndex = -1;
        for (int32 TrackIndex = 0; TrackIndex < static_cast<int32>(Tracks.size()); ++TrackIndex)
        {
            const FString& TrackName = Tracks[TrackIndex].Name;
            if (TrackName == PropertyName || TrackName.find(PropertyName + FString(".")) == 0)
            {
                SelectedCurveTrackIndex = TrackIndex;
                InlineCurveEditor.ResetSelection();
                if (Tracks[TrackIndex].Curve)
                {
                    InlineCurveEditor.FitViewToCurve(*Tracks[TrackIndex].Curve);
                }
                return;
            }
        }
    };

    auto CreateCurveForProperty = [&](const FParticleCurveProperty& Property)
    {
        if (IsParticleCurvePropertyActive(Property))
        {
            return false;
        }

        const FString PropertyName = Property.Name;
        PushUndoSnapshot();
        if (!ConvertParticleCurvePropertyToCurve(Property))
        {
            return false;
        }

        ApplyCurveStructureChange();
        RefreshTracks();
        SelectFirstTrackForProperty(PropertyName);
        return true;
    };

    if (SelectedCurveTrackIndex >= static_cast<int32>(Tracks.size()))
    {
        SelectedCurveTrackIndex = -1;
        InlineCurveEditor.ResetSelection();
    }
    if (SelectedCurveTrackIndex < 0 && !Tracks.empty())
    {
        SelectedCurveTrackIndex = 0;
        InlineCurveEditor.ResetSelection();
        if (Tracks[0].Curve)
        {
            InlineCurveEditor.FitViewToCurve(*Tracks[0].Curve);
        }
    }

    FParticleCurveTrack* SelectedTrack = nullptr;
    if (SelectedCurveTrackIndex >= 0 && SelectedCurveTrackIndex < static_cast<int32>(Tracks.size()))
    {
        SelectedTrack = &Tracks[SelectedCurveTrackIndex];
    }

    const char* Context = SelectedEntry ? SelectedEntry->Name : "no module selected";

    if (BeginPanel("##PSECurveEditor", "Curve Editor", Width, Height, Context))
    {
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2.0f, 2.0f));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 1.0f, 1.0f, 0.12f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0f, 1.0f, 1.0f, 0.22f));

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(2.0f, 2.0f));
        if (ImGui::BeginChild(
            "##PSECurveToolbar",
            ImVec2(0.0f, CurveToolbarH),
            false,
            ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoBackground))
        {
            const bool bHasTrack = SelectedTrack && SelectedTrack->Curve;
            const bool bHasSelectedKey = bHasTrack && InlineCurveEditor.HasSelectedKey(*SelectedTrack->Curve);

            if (IconToolButton("FitHorizontal", LoadToolIcon(L"icon_CurveEditor_Horizontal_40x.png"), "H", "Fit horizontal", bHasTrack, CurveIconSize) && bHasTrack)
            {
                InlineCurveEditor.FitHorizontalToCurve(*SelectedTrack->Curve);
            }
            ImGui::SameLine();
            if (IconToolButton("FitVertical", LoadToolIcon(L"icon_CurveEditor_Vertical_40x.png"), "V", "Fit vertical", bHasTrack, CurveIconSize) && bHasTrack)
            {
                InlineCurveEditor.FitVerticalToCurve(*SelectedTrack->Curve);
            }
            ImGui::SameLine();
            if (IconToolButton("FitAll", LoadToolIcon(L"icon_CurveEditor_ShowAll_40x.png"), "All", "Fit all", bHasTrack, CurveIconSize) && bHasTrack)
            {
                InlineCurveEditor.FitViewToCurve(*SelectedTrack->Curve);
            }
            ImGui::SameLine();
            if (IconToolButton("FitSelected", LoadToolIcon(L"icon_CurveEditor_ZoomToFit_40x.png"), "Sel", "Fit selected track", bHasTrack, CurveIconSize) && bHasTrack)
            {
                InlineCurveEditor.FitViewToCurve(*SelectedTrack->Curve);
            }
            ImGui::SameLine();

            const bool bPanActive = (CurveMode == ECurveInteractionMode::Pan);
            const bool bZoomActive = (CurveMode == ECurveInteractionMode::Zoom);
            if (bPanActive) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.29f, 0.56f, 1.0f, 0.40f));
            if (IconToolButton("PanMode", LoadToolIcon(L"icon_CurveEditor_Pan_40x.png"), "Pan", "Pan mode", true, CurveIconSize))
            {
                CurveMode = ECurveInteractionMode::Pan;
            }
            if (bPanActive) ImGui::PopStyleColor();
            ImGui::SameLine();
            if (bZoomActive) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.29f, 0.56f, 1.0f, 0.40f));
            if (IconToolButton("ZoomMode", LoadToolIcon(L"icon_CurveEditor_Zoom_40x.png"), "Zm", "Zoom mode", true, CurveIconSize))
            {
                CurveMode = ECurveInteractionMode::Zoom;
            }
            if (bZoomActive) ImGui::PopStyleColor();
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("%s", CurveMode == ECurveInteractionMode::Zoom ? "Zoom mode: drag an empty area to zoom into a rectangle. Wheel still zooms around cursor." : "Pan mode: right-drag the graph to pan. Wheel zooms around cursor.");
            }
            ImGui::SameLine();

            const ImVec2 SepPos = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddLine(ImVec2(SepPos.x + 3.0f, SepPos.y + 3.0f), ImVec2(SepPos.x + 3.0f, SepPos.y + 25.0f), PSE::Border32);
            ImGui::Dummy(ImVec2(7.0f, 0.0f));
            ImGui::SameLine();

            auto ApplySelectedCurveChange = [&]()
            {
                MarkDirty();
                if (SelectedTrack && SelectedTrack->OwnerModule)
                {
                    SelectedTrack->OwnerModule->RefreshModule();
                }
                RestartPreviewSimulation();
            };

            if (IconToolButton("AutoTangent", LoadToolIcon(L"icon_CurveEditor_Auto_40x.png"), "A", "Auto tangent", bHasSelectedKey, CurveIconSize) && bHasSelectedKey)
            {
                PushUndoSnapshot();
                InlineCurveEditor.SetSelectedKeyTangentMode(*SelectedTrack->Curve, ECurveTangentMode::Auto);
                ApplySelectedCurveChange();
            }
            ImGui::SameLine();
            IconToolButton("AutoClampedTangent", LoadToolIcon(L"icon_CurveEditor_AutoClamped_40x.png"), "AC", "Auto/Clamped tangent is not a separate mode in this curve type", false, CurveIconSize);
            ImGui::SameLine();
            if (IconToolButton("UserTangent", LoadToolIcon(L"icon_CurveEditor_User_40x.png"), "U", "User tangent", bHasSelectedKey, CurveIconSize) && bHasSelectedKey)
            {
                PushUndoSnapshot();
                InlineCurveEditor.SetSelectedKeyTangentMode(*SelectedTrack->Curve, ECurveTangentMode::User);
                ApplySelectedCurveChange();
            }
            ImGui::SameLine();
            if (IconToolButton("BreakTangent", LoadToolIcon(L"icon_CurveEditor_Break_40x.png"), "Br", "Break tangent", bHasSelectedKey, CurveIconSize) && bHasSelectedKey)
            {
                PushUndoSnapshot();
                InlineCurveEditor.SetSelectedKeyTangentMode(*SelectedTrack->Curve, ECurveTangentMode::Break);
                ApplySelectedCurveChange();
            }
            ImGui::SameLine();
            if (IconToolButton("LinearInterp", LoadToolIcon(L"icon_CurveEditor_Linear_40x.png"), "Ln", "Linear interpolation", bHasSelectedKey, CurveIconSize) && bHasSelectedKey)
            {
                PushUndoSnapshot();
                InlineCurveEditor.SetSelectedKeyInterpMode(*SelectedTrack->Curve, ECurveInterpMode::Linear);
                ApplySelectedCurveChange();
            }
            ImGui::SameLine();
            if (IconToolButton("ConstantInterp", LoadToolIcon(L"icon_CurveEditor_Constant_40x.png"), "Cn", "Constant interpolation", bHasSelectedKey, CurveIconSize) && bHasSelectedKey)
            {
                PushUndoSnapshot();
                InlineCurveEditor.SetSelectedKeyInterpMode(*SelectedTrack->Curve, ECurveInterpMode::Constant);
                ApplySelectedCurveChange();
            }
            ImGui::SameLine();
            if (IconToolButton("FlattenTangent", LoadToolIcon(L"icon_CurveEditor_Flatten_40x.png"), "Fl", "Flatten tangent", bHasSelectedKey, CurveIconSize) && bHasSelectedKey)
            {
                PushUndoSnapshot();
                InlineCurveEditor.FlattenSelectedKeyTangents(*SelectedTrack->Curve);
                ApplySelectedCurveChange();
            }
            ImGui::SameLine();
            if (IconToolButton("StraightenTangent", LoadToolIcon(L"icon_CurveEditor_Straighten_40x.png"), "St", "Straighten tangent", bHasSelectedKey, CurveIconSize) && bHasSelectedKey)
            {
                PushUndoSnapshot();
                InlineCurveEditor.StraightenSelectedKeyTangents(*SelectedTrack->Curve);
                ApplySelectedCurveChange();
            }
            ImGui::SameLine();

            const ImVec2 SepPos2 = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddLine(ImVec2(SepPos2.x + 3.0f, SepPos2.y + 3.0f), ImVec2(SepPos2.x + 3.0f, SepPos2.y + 25.0f), PSE::Border32);
            ImGui::Dummy(ImVec2(7.0f, 0.0f));
            ImGui::SameLine();

            int32 FirstInactivePropertyIndex = -1;
            for (int32 PropertyIndex = 0; PropertyIndex < static_cast<int32>(Properties.size()); ++PropertyIndex)
            {
                if (!IsParticleCurvePropertyActive(Properties[PropertyIndex]))
                {
                    FirstInactivePropertyIndex = PropertyIndex;
                    break;
                }
            }
            const bool bCanCreateCurve = FirstInactivePropertyIndex >= 0;
            if (IconToolButton("Create", LoadToolIcon(L"icon_CurveEditor_Create_40x.png"), "+", "Create a Curve distribution for the first available property", bCanCreateCurve, CurveIconSize) && bCanCreateCurve)
            {
                CreateCurveForProperty(Properties[FirstInactivePropertyIndex]);
                SelectedTrack = nullptr;
                if (SelectedCurveTrackIndex >= 0 && SelectedCurveTrackIndex < static_cast<int32>(Tracks.size()))
                {
                    SelectedTrack = &Tracks[SelectedCurveTrackIndex];
                }
            }
            ImGui::SameLine();
            IconToolButton("Delete", LoadToolIcon(L"icon_CurveEditor_DeleteTab_40x.png"), "x", "Use the Details Type combo to change this property away from Curve", false, CurveIconSize);
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();

        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar(2);
        ImGui::Spacing();

        if (ImGui::BeginChild("##PSECurveTracks", ImVec2(TrackListW, 0.0f), true))
        {
            if (!SelectedEntry)
            {
                ImGui::TextColored(PSE::DimTextV, "Select a\nmodule");
            }
            else if (Properties.empty())
            {
                ImGui::TextColored(PSE::DimTextV, "No curve-capable\nproperties");
            }
            else
            {
                if (!Tracks.empty())
                {
                    ImGui::TextColored(PSE::DimTextV, "Active Curves");
                    for (int32 TrackIndex = 0; TrackIndex < static_cast<int32>(Tracks.size()); ++TrackIndex)
                    {
                        const FParticleCurveTrack& Track = Tracks[TrackIndex];
                        ImGui::PushID(TrackIndex);
                        ImDrawList* DL = ImGui::GetWindowDrawList();
                        const ImVec2 Pos = ImGui::GetCursorScreenPos();
                        DL->AddCircleFilled(ImVec2(Pos.x + 6.0f, Pos.y + 8.0f), 4.0f, Track.Color);
                        ImGui::Dummy(ImVec2(14.0f, 16.0f));
                        ImGui::SameLine();
                        if (ImGui::Selectable(Track.Name.c_str(), SelectedCurveTrackIndex == TrackIndex))
                        {
                            SelectedCurveTrackIndex = TrackIndex;
                            InlineCurveEditor.ResetSelection();
                            if (Track.Curve)
                            {
                                InlineCurveEditor.FitViewToCurve(*Track.Curve);
                            }
                        }
                        if (ImGui::IsItemHovered())
                        {
                            ImGui::SetTooltip("%s\n%s", Track.Name.c_str(), ParticleCurveDomainName(Track.Domain));
                        }
                        ImGui::PopID();
                    }
                    ImGui::Separator();
                }

                ImGui::TextColored(PSE::DimTextV, "Available Distributions");
                for (int32 PropertyIndex = 0; PropertyIndex < static_cast<int32>(Properties.size()); ++PropertyIndex)
                {
                    FParticleCurveProperty& Property = Properties[PropertyIndex];
                    const bool bActive = IsParticleCurvePropertyActive(Property);
                    ImGui::PushID(1000 + PropertyIndex);

                    if (bActive)
                    {
                        ImGui::TextColored(ImVec4(0.45f, 0.80f, 0.55f, 1.0f), "Curve");
                    }
                    else
                    {
                        if (ImGui::SmallButton("Create"))
                        {
                            CreateCurveForProperty(Property);
                            SelectedTrack = nullptr;
                            if (SelectedCurveTrackIndex >= 0 && SelectedCurveTrackIndex < static_cast<int32>(Tracks.size()))
                            {
                                SelectedTrack = &Tracks[SelectedCurveTrackIndex];
                            }
                        }
                    }
                    ImGui::SameLine();
                    if (ImGui::Selectable(Property.Name.c_str(), false, ImGuiSelectableFlags_AllowOverlap))
                    {
                        if (bActive)
                        {
                            SelectFirstTrackForProperty(Property.Name);
                        }
                        else
                        {
                            CreateCurveForProperty(Property);
                            SelectedTrack = nullptr;
                            if (SelectedCurveTrackIndex >= 0 && SelectedCurveTrackIndex < static_cast<int32>(Tracks.size()))
                            {
                                SelectedTrack = &Tracks[SelectedCurveTrackIndex];
                            }
                        }
                    }
                    if (ImGui::IsItemHovered())
                    {
                        ImGui::SetTooltip("%s\n%s\n%s", Property.Name.c_str(), CurvePropertyKindName(Property.Kind), ParticleCurveDomainName(Property.Domain));
                    }
                    ImGui::PopID();
                }
            }
        }
        ImGui::EndChild();

        ImGui::SameLine();

        const ImVec2 EditorRegionSize = ImGui::GetContentRegionAvail();
        if (!SelectedEntry)
        {
            const ImVec2 CanvasMin = ImGui::GetCursorScreenPos();
            ImGui::Dummy(EditorRegionSize);
            CanvasHint(ImGui::GetWindowDrawList(), CanvasMin, ImVec2(CanvasMin.x + EditorRegionSize.x, CanvasMin.y + EditorRegionSize.y), "Select a module to edit its curves");
        }
        else if (Properties.empty())
        {
            const ImVec2 CanvasMin = ImGui::GetCursorScreenPos();
            ImGui::Dummy(EditorRegionSize);
            CanvasHint(ImGui::GetWindowDrawList(), CanvasMin, ImVec2(CanvasMin.x + EditorRegionSize.x, CanvasMin.y + EditorRegionSize.y), "This module has no curve-capable distribution");
        }
        else if (Tracks.empty())
        {
            const ImVec2 CanvasMin = ImGui::GetCursorScreenPos();
            ImGui::Dummy(EditorRegionSize);
            CanvasHint(ImGui::GetWindowDrawList(), CanvasMin, ImVec2(CanvasMin.x + EditorRegionSize.x, CanvasMin.y + EditorRegionSize.y), "Create a Curve distribution from the left list or the + toolbar button");
        }
        else if (!SelectedTrack || !SelectedTrack->Curve)
        {
            const ImVec2 CanvasMin = ImGui::GetCursorScreenPos();
            ImGui::Dummy(EditorRegionSize);
            CanvasHint(ImGui::GetWindowDrawList(), CanvasMin, ImVec2(CanvasMin.x + EditorRegionSize.x, CanvasMin.y + EditorRegionSize.y), "Select a curve track");
        }
        else
        {
            ImGui::BeginChild("##PSECurveGraphAndInspector", EditorRegionSize, false);
            {
                ImGui::Text("%s", SelectedTrack->Name.c_str());
                ImGui::SameLine();
                ImGui::TextColored(PSE::DimTextV, "(%s)", ParticleCurveDomainName(SelectedTrack->Domain));
                ImGui::Separator();

                const ImVec2 Available = ImGui::GetContentRegionAvail();
                const float InspectorWidth = 230.0f;
                const ImVec2 CurveCanvasSize((std::max)(Available.x - InspectorWidth - 12.0f, 160.0f), (std::max)(Available.y, 120.0f));

                FInlineFloatCurveEditor::EInteractionMode InlineMode = CurveMode == ECurveInteractionMode::Zoom
                    ? FInlineFloatCurveEditor::EInteractionMode::Zoom
                    : FInlineFloatCurveEditor::EInteractionMode::Pan;

                FInlineCurveEditResult EditResult = InlineCurveEditor.Render(
                    "ParticleCurveEditor",
                    *SelectedTrack->Curve,
                    CurveCanvasSize,
                    InlineMode,
                    true);

                if (EditResult.bEditStarted)
                {
                    PushUndoSnapshot();
                }
                if (EditResult.bChanged)
                {
                    MarkDirty();
                    if (SelectedTrack->OwnerModule)
                    {
                        SelectedTrack->OwnerModule->RefreshModule();
                    }
                    RestartPreviewSimulation();
                }
            }
            ImGui::EndChild();
        }
    }

    EndPanel();
}

