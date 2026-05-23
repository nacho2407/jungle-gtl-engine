#include "ParticleSystemEditorWidget.h"

#include "imgui.h"
#include "Object/Object.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModule.h"
#include "Particles/ParticleModuleRequired.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemManager.h"
#include "Particles/Color/ParticleModuleColor.h"
#include "Particles/Lifetime/ParticleModuleLifetime.h"
#include "Particles/Location/ParticleModuleLocation.h"
#include "Particles/Size/ParticleModuleSize.h"
#include "Particles/Spawn/ParticleModuleSpawn.h"
#include "Particles/TypeData/ParticleModuleTypeDataBase.h"
#include "Particles/Velocity/ParticleModuleVelocity.h"
#include "Materials/Material.h"
#include "Materials/MaterialManager.h"

#include <algorithm>
#include <cstdio>

#include "Component/Light/DirectionalLightComponent.h"
#include "Component/Primitive/ParticleSystemComponent.h"
#include "GameFramework/WorldContext.h"
#include "GameFramework/Light/DirectionalLightActor.h"
#include "Runtime/Engine.h"
#include "Slate/SlateApplication.h"
#include "Object/Reflection/ObjectFactory.h"
#include "Viewport/Viewport.h"

namespace
{
    // ── 팔레트 ───────────────────────────────────────────────────────────────
    namespace PSE
    {
        const ImVec4 WindowBg = ImVec4(0.086f, 0.090f, 0.102f, 1.0f);
        const ImVec4 PanelBg  = ImVec4(0.122f, 0.128f, 0.145f, 1.0f);
        const ImVec4 Border   = ImVec4(0.224f, 0.235f, 0.267f, 1.0f);
        const ImVec4 FrameBg  = ImVec4(0.067f, 0.071f, 0.082f, 1.0f);

        constexpr ImU32 HeaderText = IM_COL32(228, 231, 238, 255);
        constexpr ImU32 DimText    = IM_COL32(122, 128, 140, 255);
        constexpr ImU32 Accent     = IM_COL32(74, 144, 255, 255);
        constexpr ImU32 AccentSoft = IM_COL32(74, 144, 255, 70);
        constexpr ImU32 Border32   = IM_COL32(57, 60, 70, 255);
        constexpr ImU32 ViewportBg = IM_COL32(17, 18, 22, 255);
        constexpr ImU32 GridMinor  = IM_COL32(35, 37, 44, 255);
        constexpr ImU32 GridMajor  = IM_COL32(52, 55, 64, 255);

        const ImVec4 DimTextV = ImVec4(0.478f, 0.502f, 0.549f, 1.0f);
    }

    float Clamp01(float V, float Lo, float Hi)
    {
        return V < Lo ? Lo : (V > Hi ? Hi : V);
    }

    // ── 모듈 목록 헬퍼 ───────────────────────────────────────────────────────
    // 이미터의 LOD0가 실제로 보유한 모듈을 표준 순서(Required → Spawn → 나머지 → TypeData)로
    // 풀어, UI가 정적 표 대신 실제 데이터를 표시·편집하게 한다.
    struct FEmitterModuleEntry
    {
        const char*      Name;
        UParticleModule* Module;
    };

    const char* GetModuleDisplayName(const UParticleModule* Module)
    {
        if (!Module) return "Module";
        if (Cast<UParticleModuleRequired>(Module)) return "Required";
        if (Cast<UParticleModuleSpawn>(Module))    return "Spawn";
        if (Cast<UParticleModuleLifetime>(Module)) return "Lifetime";
        if (Cast<UParticleModuleLocation>(Module)) return "Location";
        if (Cast<UParticleModuleVelocity>(Module)) return "Velocity";
        if (Cast<UParticleModuleSize>(Module))     return "Size";
        if (Cast<UParticleModuleColor>(Module))    return "Color";
        return "Module";
    }

    void BuildEmitterModuleList(UParticleEmitter* Emitter, TArray<FEmitterModuleEntry>& OutList)
    {
        OutList.clear();
        if (!Emitter) return;

        UParticleLODLevel* LOD0 = Emitter->GetLODLevel(0);
        if (!LOD0) return;

        if (LOD0->RequiredModule)
        {
            OutList.push_back({ "Required", LOD0->RequiredModule });
        }
        if (LOD0->SpawnModule)
        {
            OutList.push_back({ "Spawn", LOD0->SpawnModule });
        }
        for (UParticleModule* Module : LOD0->Modules)
        {
            if (!Module) continue;
            OutList.push_back({ GetModuleDisplayName(Module), Module });
        }
        if (LOD0->TypeDataModule)
        {
            OutList.push_back({ "TypeData", static_cast<UParticleModule*>(LOD0->TypeDataModule) });
        }
    }

    // ── 공용 위젯 헬퍼 ───────────────────────────────────────────────────────

    // 패널 상단 헤더. Context 는 우측에 흐린 글씨로 (현재 바인딩 대상 등) 표시.
    void PanelHeader(const char* Title, const char* Context = nullptr)
    {
        ImDrawList*  DrawList = ImGui::GetWindowDrawList();
        const ImVec2 Pos      = ImGui::GetCursorScreenPos();
        const float  Width    = ImGui::GetContentRegionAvail().x;
        const float  TextH    = ImGui::GetTextLineHeight();

        DrawList->AddRectFilled(ImVec2(Pos.x, Pos.y + 1.0f), ImVec2(Pos.x + 3.0f, Pos.y + TextH), PSE::Accent, 1.0f);
        DrawList->AddText(ImVec2(Pos.x + 11.0f, Pos.y), PSE::HeaderText, Title);

        if (Context && Context[0])
        {
            const float ContextW = ImGui::CalcTextSize(Context).x;
            DrawList->AddText(ImVec2(Pos.x + Width - ContextW, Pos.y + 1.0f), PSE::DimText, Context);
        }

        const float LineY = Pos.y + TextH + 6.0f;
        DrawList->AddLine(ImVec2(Pos.x, LineY), ImVec2(Pos.x + Width, LineY), PSE::Border32);
        ImGui::Dummy(ImVec2(Width, TextH + 13.0f));
    }

    // 테두리 + 둥근 모서리를 가진 패널 차일드를 연다.
    bool BeginPanel(const char* StrId, const char* Title, float Width, float Height, const char* Context = nullptr)
    {
        Width  = (std::max)(Width, 48.0f);
        Height = (std::max)(Height, 48.0f);

        ImGui::PushStyleColor(ImGuiCol_ChildBg, PSE::PanelBg);
        ImGui::PushStyleColor(ImGuiCol_Border, PSE::Border);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 9.0f));

        const bool bVisible = ImGui::BeginChild(StrId, ImVec2(Width, Height), true);
        if (bVisible)
        {
            PanelHeader(Title, Context);
        }
        return bVisible;
    }

    void EndPanel()
    {
        ImGui::EndChild();
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(2);
    }

    // 드래그 가능한 분할자. Ratio(첫 영역 비율)를 제자리에서 갱신한다.
    void Splitter(const char* StrId, bool bVertical, float FullExtent, float CrossExtent, float& Ratio)
    {
        constexpr float Thickness = 7.0f;
        const ImVec2    Size      = bVertical ? ImVec2(Thickness, CrossExtent) : ImVec2(CrossExtent, Thickness);
        const ImVec2    Pos       = ImGui::GetCursorScreenPos();

        ImGui::InvisibleButton(StrId, Size);
        const bool bHovered = ImGui::IsItemHovered();
        const bool bActive  = ImGui::IsItemActive();

        if (bActive && FullExtent > 1.0f)
        {
            const float Delta = bVertical ? ImGui::GetIO().MouseDelta.x : ImGui::GetIO().MouseDelta.y;
            Ratio             = Clamp01(Ratio + Delta / FullExtent, 0.18f, 0.82f);
        }
        if (bHovered || bActive)
        {
            ImGui::SetMouseCursor(bVertical ? ImGuiMouseCursor_ResizeEW : ImGuiMouseCursor_ResizeNS);
        }

        ImDrawList*  DrawList = ImGui::GetWindowDrawList();
        const ImU32  Color    = bActive ? PSE::Accent : (bHovered ? PSE::AccentSoft : PSE::Border32);
        const ImVec2 Center(Pos.x + Size.x * 0.5f, Pos.y + Size.y * 0.5f);
        for (int32 i = -1; i <= 1; ++i)
        {
            const ImVec2 Dot = bVertical ? ImVec2(Center.x, Center.y + i * 6.0f)
            : ImVec2(Center.x + i * 6.0f, Center.y);
            DrawList->AddCircleFilled(Dot, 1.6f, Color);
        }
    }

    // 캔버스 중앙에 흐린 안내 문구를 그린다 (빈 상태 표시용).
    void CanvasHint(ImDrawList* DrawList, const ImVec2& Min, const ImVec2& Max, const char* Text)
    {
        const ImVec2 Size = ImGui::CalcTextSize(Text);
        DrawList->AddText(ImVec2((Min.x + Max.x - Size.x) * 0.5f, (Min.y + Max.y - Size.y) * 0.5f), PSE::DimText, Text);
    }

    void KeyValueRow(const char* Key, const FString& Value)
    {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(Key);
        ImGui::TableNextColumn();
        ImGui::TextColored(PSE::DimTextV, "%s", Value.empty() ? "-" : Value.c_str());
    }
}

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

    SelectedEmitterIndex = -1;
    SelectedModuleIndex  = -1;
    bSimulating          = false;
    PreviewTime          = 0.0f;
    PropertySearch[0]    = '\0';
    PreviewPSC           = nullptr;
    EmitterEnabled.clear();

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
    FAssetEditorWidget::Close();

    if (UWorld* PreviewWorld = ViewportClient.GetPreviewWorld())
    {
        FScene& PreviewScene = PreviewWorld->GetScene();
        GEngine->GetRenderer().GetResources().ReleaseShadowResourcesForScene(&PreviewScene);

        if (PreviewWorldHandle.IsValid())
        {
            GEngine->DestroyWorldContext(PreviewWorldHandle);
        }
    }

    PreviewPSC           = nullptr;
    PreviewTextureHandle = nullptr;

    FSlateApplication::Get().UnregisterViewport(&ViewportClient);
    ViewportClient.Release();
}

void FParticleSystemEditorWidget::Tick(float DeltaTime)
{
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
    ImGuiWindowFlags WindowFlags = ImGuiWindowFlags_MenuBar;
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
            Close();
        }
        return;
    }

    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
    {
        FSlateApplication::Get().BringViewportToFront(&ViewportClient);
    }

    SyncEmitterUIState();

    RenderMenuBar();
    RenderToolbar();
    ImGui::Separator();

    // ── 2 x 2 패널 레이아웃 (드래그 분할자 포함) ────────────────────────────
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

    // 좌측 컬럼: 프리뷰 + 프로퍼티.
    ImGui::BeginGroup();
    RenderViewportPanel(LeftW, LeftTopH);
    Splitter("##SplitLeftRow", false, LayoutH, LeftW, LeftRowRatio);
    RenderPropertiesPanel(LeftW, LeftBotH);
    ImGui::EndGroup();

    ImGui::SameLine();
    Splitter("##SplitColumn", true, LayoutW, LayoutH, ColumnRatio);
    ImGui::SameLine();

    // 우측 컬럼: 이미터 + 커브 에디터.
    ImGui::BeginGroup();
    RenderEmittersPanel(RightW, RightTopH);
    Splitter("##SplitRightRow", false, LayoutH, RightW, RightRowRatio);
    RenderCurveEditorPanel(RightW, RightBotH);
    ImGui::EndGroup();

    ImGui::PopStyleVar();

    ImGui::Spacing();
    RenderStatusBar();

    ImGui::End();

    if (!bWindowOpen)
    {
        Close();
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
        }
    }
}

void FParticleSystemEditorWidget::SelectEmitter(int32 EmitterIndex, int32 ModuleIndex)
{
    SelectedEmitterIndex = EmitterIndex;
    SelectedModuleIndex  = ModuleIndex;

    // 새 모듈을 선택하면 커브 트랙은 기본적으로 모두 표시한다.
    for (bool& Visible : CurveTrackVisible)
    {
        Visible = true;
    }
}

void FParticleSystemEditorWidget::AddEmitter()
{
    UParticleSystem* ParticleSystem = GetParticleSystem();
    if (!ParticleSystem)
    {
        return;
    }

    UParticleEmitter* NewEmitter = UObjectManager::Get().CreateObject<UParticleEmitter>(ParticleSystem);
    if (!NewEmitter)
    {
        return;
    }

    NewEmitter->InitializeDefaultSpriteEmitter();

    TArray<UParticleEmitter*>& Emitters = ParticleSystem->GetEmitters();
    Emitters.push_back(NewEmitter);

    SyncEmitterUIState();

    const int32 NewEmitterIndex = static_cast<int32>(Emitters.size()) - 1;
    SelectEmitter(NewEmitterIndex, -1);

    MarkDirty();
    RestartPreviewSimulation();
}

void FParticleSystemEditorWidget::DeleteSelectedEmitter()
{
    UParticleSystem* ParticleSystem = GetParticleSystem();
    if (!ParticleSystem)
    {
        return;
    }

    TArray<UParticleEmitter*>& Emitters = ParticleSystem->GetEmitters();

    if (SelectedEmitterIndex < 0 || SelectedEmitterIndex >= static_cast<int32>(Emitters.size()))
    {
        return;
    }

    UParticleEmitter* RemovedEmitter = Emitters[SelectedEmitterIndex];

    Emitters.erase(Emitters.begin() + SelectedEmitterIndex);

    if (RemovedEmitter)
    {
        UObjectManager::Get().DestroyObject(RemovedEmitter);
    }

    SelectedEmitterIndex = -1;
    SelectedModuleIndex  = -1;

    SyncEmitterUIState();

    MarkDirty();
    RestartPreviewSimulation();
}

void FParticleSystemEditorWidget::SyncEmitterUIState()
{
    UParticleSystem* ParticleSystem = GetParticleSystem();

    const int32 EmitterCount = ParticleSystem ? static_cast<int32>(ParticleSystem->GetEmitters().size()) : 0;

    EmitterEnabled.resize(EmitterCount);

    if (ParticleSystem)
    {
        const TArray<UParticleEmitter*>& Emitters = ParticleSystem->GetEmitters();

        for (int32 Index = 0; Index < EmitterCount; ++Index)
        {
            UParticleEmitter* Emitter = Emitters[Index];
            EmitterEnabled[Index]     = Emitter ? Emitter->IsEnabled() : true;
        }
    }

    if (EmitterCount <= 0)
    {
        SelectedEmitterIndex = -1;
        SelectedModuleIndex  = -1;
        return;
    }

    if (SelectedEmitterIndex < 0 || SelectedEmitterIndex >= EmitterCount)
    {
        SelectEmitter(0, -1);
    }
}

void FParticleSystemEditorWidget::RestartPreviewSimulation()
{
    bSimulating = true;
    PreviewTime = 0.0f;

    if (PreviewPSC)
    {
        PreviewPSC->ResetSystem();
    }

    if (ViewportClient.IsRenderable())
    {
        ViewportClient.ResetCameraToPreviewBounds();
    }
}

// ── 메뉴 바 (패널 1) ─────────────────────────────────────────────────────────
void FParticleSystemEditorWidget::RenderMenuBar()
{
    if (!ImGui::BeginMenuBar())
    {
        return;
    }

    if (ImGui::BeginMenu("File"))
    {
        if (ImGui::MenuItem("Save", "Ctrl+S", false, IsDirty()))
        {
            SaveAsset();
        }
        ImGui::MenuItem("Save As...", nullptr, false, false); // TODO
        ImGui::Separator();
        if (ImGui::MenuItem("Close"))
        {
            Close();
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Edit"))
    {
        ImGui::MenuItem("Undo", "Ctrl+Z", false, false); // TODO
        ImGui::MenuItem("Redo", "Ctrl+Y", false, false); // TODO
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Asset"))
    {
        ImGui::MenuItem("Find in Content Browser", nullptr, false, false); // TODO
        ImGui::MenuItem("Reimport", nullptr, false, false);                // TODO
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Window"))
    {
        ImGui::MenuItem("Preview", nullptr, true, false);
        ImGui::MenuItem("Emitters", nullptr, true, false);
        ImGui::MenuItem("Properties", nullptr, true, false);
        ImGui::MenuItem("Curve Editor", nullptr, true, false);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Help"))
    {
        ImGui::MenuItem("Documentation", nullptr, false, false); // TODO
        ImGui::EndMenu();
    }

    ImGui::EndMenuBar();
}

// ── 툴바 (패널 2) ────────────────────────────────────────────────────────────
void FParticleSystemEditorWidget::RenderToolbar()
{
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(9.0f, 5.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 4.0f));

    auto Tool = [](const char* Label, const char* Tip, bool bEnabled = true) -> bool
    {
        if (!bEnabled)
        {
            ImGui::BeginDisabled();
        }
        const bool bPressed = ImGui::Button(Label);
        if (!bEnabled)
        {
            ImGui::EndDisabled();
        }
        if (Tip && ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("%s", Tip);
        }
        ImGui::SameLine();
        return bPressed;
    };

    auto Group = []()
    {
        const ImVec2 Pos = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddLine(
            ImVec2(Pos.x + 3.0f, Pos.y + 3.0f),
            ImVec2(Pos.x + 3.0f, Pos.y + 23.0f),
            PSE::Border32
        );
        ImGui::Dummy(ImVec2(7.0f, 0.0f));
        ImGui::SameLine();
    };

    if (Tool("Save", "Save the particle system", IsDirty()))
    {
        SaveAsset();
    }
    Tool("Find in CB", "Find this asset in the Content Browser"); // TODO
    Group();

    if (Tool("Restart Sim", "Restart the preview simulation"))
    {
        RestartPreviewSimulation();
    }
    Tool("Restart Level", "Restart all level instances of this system"); // TODO
    Group();

    Tool("Undo", "Undo", false); // TODO
    Tool("Redo", "Redo", false); // TODO
    Group();

    Tool("Thumbnail", "Capture thumbnail from preview"); // TODO
    Tool("Bounds", "Toggle bounds display");             // TODO
    Tool("Origin Axis", "Toggle origin axis");           // TODO
    Tool("Background", "Set preview background color");  // TODO
    Group();

    Tool("Regen LOD", "Regenerate lowest LOD from highest"); // TODO
    Tool("Add LOD", "Add a new LOD level");                  // TODO

    ImGui::PopStyleVar(3);
}

// ── 상태 바 ──────────────────────────────────────────────────────────────────
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
            BuildEmitterModuleList(StatusEmitter, ModuleList);

            const char* ModuleName = "?";
            if (SelectedModuleIndex < static_cast<int32>(ModuleList.size()))
            {
                ModuleName = ModuleList[SelectedModuleIndex].Name;
            }

            ImGui::TextColored(
                PSE::DimTextV,
                "  |  Selection: Emitter %d > %s",
                SelectedEmitterIndex,
                ModuleName
            );
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ── 프리뷰 뷰포트 (패널 3) ───────────────────────────────────────────────────
void FParticleSystemEditorWidget::RenderViewportPanel(float Width, float Height)
{
    char Context[32];
    std::snprintf(Context, sizeof(Context), "%.2fs", PreviewTime);

    if (BeginPanel("##PSEViewport", "Preview", Width, Height, Context))
    {
        if (ImGui::BeginTabBar("##PSEViewportTabs", ImGuiTabBarFlags_None))
        {
            if (ImGui::BeginTabItem("View")) { ImGui::EndTabItem(); }
            if (ImGui::BeginTabItem("Time")) { ImGui::EndTabItem(); }
            ImGui::EndTabBar();
        }

        // 트랜스포트 바를 위해 하단 공간을 남긴다.
        constexpr float TransportH = 30.0f;
        const ImVec2    CanvasMin  = ImGui::GetCursorScreenPos();
        ImVec2          CanvasSize = ImGui::GetContentRegionAvail();
        CanvasSize.y               = (std::max)(CanvasSize.y - TransportH, 32.0f);
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

            // 정적 기준 격자 (애니메이션 없음).
            for (int32 i = 1; i < 8; ++i)
            {
                const float T = static_cast<float>(i) / 8.0f;
                DrawList->AddLine(
                    ImVec2(CanvasMin.x + CanvasSize.x * T, CanvasMin.y),
                    ImVec2(CanvasMin.x + CanvasSize.x * T, CanvasMax.y),
                    PSE::GridMinor
                );
                DrawList->AddLine(
                    ImVec2(CanvasMin.x, CanvasMin.y + CanvasSize.y * T),
                    ImVec2(CanvasMax.x, CanvasMin.y + CanvasSize.y * T),
                    PSE::GridMinor
                );
            }

            // 좌하단 축 기즈모.
            const ImVec2 Axis(CanvasMin.x + 24.0f, CanvasMax.y - 24.0f);
            DrawList->AddLine(Axis, ImVec2(Axis.x + 16.0f, Axis.y), IM_COL32(214, 90, 90, 255), 2.0f);
            DrawList->AddLine(Axis, ImVec2(Axis.x, Axis.y - 16.0f), IM_COL32(96, 196, 96, 255), 2.0f);
            DrawList->AddLine(Axis, ImVec2(Axis.x - 11.0f, Axis.y + 8.0f), IM_COL32(96, 140, 226, 255), 2.0f);

            CanvasHint(DrawList, CanvasMin, CanvasMax, "Attach a particle viewport to render the preview");
        }

        // 트랜스포트 바 (시뮬레이션 상태와 실제 연결).
        ImGui::Spacing();
        if (ImGui::Button(bSimulating ? "Pause" : "Play", ImVec2(72.0f, 0.0f)))
        {
            bSimulating = !bSimulating;

            if (PreviewPSC)
            {
                if (bSimulating)
                {
                    PreviewPSC->Activate();
                }
                else
                {
                    PreviewPSC->Deactivate();
                }
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Restart", ImVec2(72.0f, 0.0f)))
        {
            RestartPreviewSimulation();
        }
        ImGui::SameLine();
        ImGui::TextColored(PSE::DimTextV, "Sim Time  %.2fs   %s", PreviewTime, bSimulating ? "(playing)" : "(paused)");
    }
    EndPanel();
}

// ── 이미터 + 모듈 (패널 4) ───────────────────────────────────────────────────
void FParticleSystemEditorWidget::RenderEmittersPanel(float Width, float Height)
{
    UParticleSystem* ParticleSystem = GetParticleSystem();
    const int32      EmitterCount   = ParticleSystem ? static_cast<int32>(ParticleSystem->GetEmitters().size()) : 0;

    char Context[32];
    std::snprintf(Context, sizeof(Context), "%d emitter%s", EmitterCount, EmitterCount == 1 ? "" : "s");

    if (BeginPanel("##PSEEmitters", "Emitters", Width, Height, Context))
    {
        if (ImGui::Button("+ Add Emitter"))
        {
            AddEmitter();
            EndPanel();
            return;
        }

        if (EmitterCount == 0)
        {
            ImDrawList*  DrawList = ImGui::GetWindowDrawList();
            const ImVec2 Min      = ImGui::GetCursorScreenPos();
            const ImVec2 Max(Min.x + ImGui::GetContentRegionAvail().x, Min.y + ImGui::GetContentRegionAvail().y);
            CanvasHint(DrawList, Min, Max, "No emitters - use \"+ Add Emitter\" to create one");
            EndPanel();
            return;
        }

        ImGui::Spacing();

        constexpr float ColumnWidth = 168.0f;
        if (ImGui::BeginChild("##PSEEmitterColumns", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_HorizontalScrollbar))
        {
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
                    char EmitterName[32];
                    std::snprintf(EmitterName, sizeof(EmitterName), "Emitter %d", EmitterIndex);

                    // 이미터 헤더 = 이미터 자체 선택 (Module = -1).
                    const bool bHeaderSel = bEmitterSelected && SelectedModuleIndex < 0;
                    if (ImGui::Selectable(EmitterName, bHeaderSel))
                    {
                        SelectEmitter(EmitterIndex, -1);
                    }

                    UParticleEmitter* Emitter = ParticleSystem->GetEmitters()[EmitterIndex];

                    bool bEnabled = Emitter ? Emitter->IsEnabled() : true;
                    if (ImGui::Checkbox("Enabled", &bEnabled))
                    {
                        if (Emitter)
                        {
                            Emitter->SetEnabled(bEnabled);
                        }

                        if (EmitterIndex >= 0 && EmitterIndex < static_cast<int32>(EmitterEnabled.size()))
                        {
                            EmitterEnabled[EmitterIndex] = bEnabled;
                        }
                        
                        MarkDirty();
                        RestartPreviewSimulation();
                    }
                    ImGui::Separator();

                    TArray<FEmitterModuleEntry> ModuleList;
                    BuildEmitterModuleList(Emitter, ModuleList);

                    for (int32 ModuleIndex = 0; ModuleIndex < static_cast<int32>(ModuleList.size()); ++ModuleIndex)
                    {
                        ImGui::PushID(ModuleIndex);
                        const FEmitterModuleEntry& Entry     = ModuleList[ModuleIndex];
                        const bool                 bSelected = bEmitterSelected && (SelectedModuleIndex == ModuleIndex);

                        if (ImGui::Selectable(Entry.Name, bSelected))
                        {
                            SelectEmitter(EmitterIndex, ModuleIndex);
                        }
                        ImGui::PopID();
                    }

                    ImGui::Separator();
                    if (ImGui::SmallButton("+ Module"))
                    {
                        // TODO: 모듈 추가 컨텍스트 메뉴.
                    }
                }
                ImGui::EndChild();
                ImGui::PopStyleVar();
                ImGui::PopStyleColor();
                ImGui::PopID();
            }
        }
        ImGui::EndChild();
    }
    EndPanel();
}

// ── 프로퍼티 (패널 5) ────────────────────────────────────────────────────────
void FParticleSystemEditorWidget::RenderPropertiesPanel(float Width, float Height)
{
    UParticleSystem* ParticleSystem = GetParticleSystem();

    // 헤더 Context = 현재 검사 대상 (Emitters 패널 선택과 직접 연동).
    FString Context = "Particle System";

    UParticleEmitter* SelectedEmitter = nullptr;
    if (ParticleSystem && SelectedEmitterIndex >= 0 &&
        SelectedEmitterIndex < static_cast<int32>(ParticleSystem->GetEmitters().size()))
    {
        SelectedEmitter = ParticleSystem->GetEmitters()[SelectedEmitterIndex];
    }

    TArray<FEmitterModuleEntry> ModuleList;
    BuildEmitterModuleList(SelectedEmitter, ModuleList);

    UParticleModule* SelectedModule = nullptr;
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
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputTextWithHint("##PSEPropertySearch", "Search properties", PropertySearch,
            sizeof(PropertySearch));
        ImGui::Spacing();

        constexpr ImGuiTableFlags TableFlags = ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY;

        if (ImGui::BeginTable("##PSEDetails", 2, TableFlags))
        {
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 138.0f);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            if (SelectedEmitterIndex < 0)
            {
                // 파티클 시스템 자체 — 실제 에셋 값.
                const FString Path = ParticleSystem ? ParticleSystem->GetSourcePath() : FString();
                KeyValueRow("Source Path", Path);
                KeyValueRow("Emitters",
                    ParticleSystem
                    ? std::to_string(ParticleSystem->GetEmitters().size())
                    : FString("0"));
                KeyValueRow("Status", IsDirty() ? FString("Modified") : FString("Saved"));
            }
            else
            {
                // 이미터 — 실제 이미터 데이터.
                const FString EmitterName = "Emitter " + std::to_string(SelectedEmitterIndex);
                KeyValueRow("Emitter", EmitterName);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Enabled");
                ImGui::TableNextColumn();
                
                UParticleEmitter* Emitter = nullptr;
                
                if (ParticleSystem && SelectedEmitterIndex >= 0 && SelectedEmitterIndex < static_cast<int32>(ParticleSystem->GetEmitters().size()))
                {
                    Emitter = ParticleSystem->GetEmitters()[SelectedEmitterIndex];
                }
                
                bool bEnabled = Emitter ? Emitter->IsEnabled() : true;
                
                if (ImGui::Checkbox("##EnabledProp", &bEnabled))
                {
                    if (Emitter)
                    {
                        Emitter->SetEnabled(bEnabled);
                    }
                    
                    if (SelectedEmitterIndex >= 0 && SelectedEmitterIndex < static_cast<int32>(EmitterEnabled.size()))
                    {
                        EmitterEnabled[SelectedEmitterIndex] = bEnabled;
                    }
                    
                    MarkDirty();
                    RestartPreviewSimulation();
                }

                int32 LODCount = SelectedEmitter ? static_cast<int32>(SelectedEmitter->GetLODLevels().size()) : 0;
                KeyValueRow("LOD Levels", std::to_string(LODCount));
                KeyValueRow("Modules", std::to_string(ModuleList.size()));

                if (SelectedModule)
                {
                    KeyValueRow("Module", FString(SelectedModuleName));
                }
            }

            ImGui::EndTable();
        }

        // 선택된 모듈의 실제 UPROPERTY 들을 편집한다.
        if (SelectedModule)
        {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::TextColored(PSE::DimTextV, "%s", SelectedModuleName);
            ImGui::Spacing();
            RenderModuleProperties(SelectedModule);
        }
    }
    EndPanel();
}

// ── 모듈 프로퍼티 편집 ──────────────────────────────────────────────────────
void FParticleSystemEditorWidget::RenderModuleProperties(UParticleModule* Module)
{
    if (!Module)
    {
        return;
    }

    bool bChanged = false;

    bool bModuleEnabled = Module->bEnabled != 0;
    if (ImGui::Checkbox("Enabled##ModuleEnabled", &bModuleEnabled))
    {
        Module->bEnabled = bModuleEnabled ? 1 : 0;
        bChanged         = true;
    }

    if (UParticleModuleRequired* Required = Cast<UParticleModuleRequired>(Module))
    {
        const FString CurrentSlot = Required->MaterialSlot.ToString();
        const bool    bSlotNone   = (CurrentSlot.empty() || CurrentSlot == "None");
        const FString Preview     = bSlotNone ? FString("None") : CurrentSlot;

        bool bMaterialPicked = false;

        if (ImGui::BeginCombo("Material", Preview.c_str()))
        {
            if (ImGui::Selectable("None", bSlotNone))
            {
                Required->MaterialSlot = "None";
                Required->ResolveMaterialFromSlot();
                bChanged         = true;
                bMaterialPicked  = true;
            }
            if (bSlotNone)
            {
                ImGui::SetItemDefaultFocus();
            }

            const TArray<FMaterialAssetListItem>& MatFiles =
                FMaterialManager::Get().GetAvailableMaterialFiles();
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
                    bChanged         = true;
                    bMaterialPicked  = true;
                }
                if (bSelected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        if (bMaterialPicked)
        {
            // 인스턴스가 Init 시점에 캐싱한 머티리얼/렌더상태를 다시 잡도록 시뮬레이션을 재시작.
            RestartPreviewSimulation();
        }

        bChanged |= ImGui::DragFloat("SpawnRate", &Required->SpawnRate, 0.1f, 0.0f, 10000.0f);
        bChanged |= ImGui::DragFloat("Emitter Duration", &Required->EmitterDuration, 0.05f, 0.0f, 10000.0f);
        bChanged |= ImGui::DragFloat("Emitter Delay", &Required->EmitterDelay, 0.05f, 0.0f, 10000.0f);
        bChanged |= ImGui::DragInt("Emitter Loops", &Required->EmitterLoops, 1.0f, 0, 10000);
        bChanged |= ImGui::DragInt("SubImages H", &Required->SubImages_Horizontal, 1.0f, 1, 64);
        bChanged |= ImGui::DragInt("SubImages V", &Required->SubImages_Vertical, 1.0f, 1, 64);
        bChanged |= ImGui::DragFloat3("Emitter Origin", Required->EmitterOrigin.Data, 0.1f);
    }
    else if (UParticleModuleSpawn* Spawn = Cast<UParticleModuleSpawn>(Module))
    {
        bChanged |= ImGui::DragFloat("SpawnRate", &Spawn->SpawnRate, 0.1f, 0.0f, 10000.0f);
        bChanged |= ImGui::DragFloat("SpawnRate Scale", &Spawn->SpawnRateScale, 0.01f, 0.0f, 100.0f);
        bChanged |= ImGui::DragFloat("Burst Scale", &Spawn->BurstScale, 0.01f, 0.0f, 100.0f);
    }
    else if (UParticleModuleLifetime* Lifetime = Cast<UParticleModuleLifetime>(Module))
    {
        bChanged |= ImGui::DragFloat("Lifetime Min", &Lifetime->LifetimeMin, 0.05f, 0.0f, 10000.0f);
        bChanged |= ImGui::DragFloat("Lifetime Max", &Lifetime->LifetimeMax, 0.05f, 0.0f, 10000.0f);
        if (Lifetime->LifetimeMax < Lifetime->LifetimeMin)
        {
            Lifetime->LifetimeMax = Lifetime->LifetimeMin;
        }
    }
    else if (UParticleModuleLocation* Location = Cast<UParticleModuleLocation>(Module))
    {
        bChanged |= ImGui::DragFloat3("Start Location", Location->StartLocation.Data, 0.1f);
    }
    else if (UParticleModuleVelocity* Velocity = Cast<UParticleModuleVelocity>(Module))
    {
        bChanged |= ImGui::DragFloat3("Min Velocity", Velocity->MinVelocity.Data, 0.5f);
        bChanged |= ImGui::DragFloat3("Max Velocity", Velocity->MaxVelocity.Data, 0.5f);
    }
    else if (UParticleModuleSize* Size = Cast<UParticleModuleSize>(Module))
    {
        bChanged |= ImGui::DragFloat3("Start Size", Size->StartSize.Data, 0.05f, 0.0f, 10000.0f);
    }
    else if (UParticleModuleColor* Color = Cast<UParticleModuleColor>(Module))
    {
        float ColorRGB[3] = {
            Color->StartColor.R / 255.0f,
            Color->StartColor.G / 255.0f,
            Color->StartColor.B / 255.0f,
        };
        if (ImGui::ColorEdit3("Start Color", ColorRGB))
        {
            Color->StartColor.R = static_cast<uint32>(Clamp01(ColorRGB[0], 0.0f, 1.0f) * 255.0f);
            Color->StartColor.G = static_cast<uint32>(Clamp01(ColorRGB[1], 0.0f, 1.0f) * 255.0f);
            Color->StartColor.B = static_cast<uint32>(Clamp01(ColorRGB[2], 0.0f, 1.0f) * 255.0f);
            bChanged            = true;
        }
        bChanged |= ImGui::DragFloat("Start Alpha", &Color->StartAlpha, 0.01f, 0.0f, 1.0f);
    }
    else
    {
        ImGui::TextColored(PSE::DimTextV, "No editable properties exposed for this module.");
    }

    if (bChanged)
    {
        MarkDirty();
    }
}

// ── 커브 에디터 (패널 6) ─────────────────────────────────────────────────────
void FParticleSystemEditorWidget::RenderCurveEditorPanel(float Width, float Height)
{
    // 선택된 모듈 (Emitters 패널 선택과 직접 연동).
    UParticleSystem* ParticleSystem = GetParticleSystem();

    UParticleEmitter* SelectedEmitter = nullptr;
    if (ParticleSystem && SelectedEmitterIndex >= 0 &&
        SelectedEmitterIndex < static_cast<int32>(ParticleSystem->GetEmitters().size()))
    {
        SelectedEmitter = ParticleSystem->GetEmitters()[SelectedEmitterIndex];
    }

    TArray<FEmitterModuleEntry> ModuleList;
    BuildEmitterModuleList(SelectedEmitter, ModuleList);

    const FEmitterModuleEntry* SelectedEntry = nullptr;
    if (SelectedModuleIndex >= 0 && SelectedModuleIndex < static_cast<int32>(ModuleList.size()))
    {
        SelectedEntry = &ModuleList[SelectedModuleIndex];
    }

    const char* Context = SelectedEntry ? SelectedEntry->Name : "no module selected";

    if (BeginPanel("##PSECurveEditor", "Curve Editor", Width, Height, Context))
    {
        // 뷰 조작 툴 (TODO).
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        ImGui::BeginDisabled();
        ImGui::SmallButton("Fit H"); ImGui::SameLine();
        ImGui::SmallButton("Fit V"); ImGui::SameLine();
        ImGui::SmallButton("Fit All"); ImGui::SameLine();
        ImGui::SmallButton("Pan"); ImGui::SameLine();
        ImGui::SmallButton("Zoom");
        ImGui::EndDisabled();
        ImGui::PopStyleVar();
        ImGui::Spacing();

        constexpr float TrackListWidth = 140.0f;

        // 좌측 트랙 목록 — Distribution 커브가 모듈에 연결되면 여기 채워진다.
        if (ImGui::BeginChild("##PSECurveTracks", ImVec2(TrackListWidth, 0.0f), true))
        {
            ImGui::TextColored(PSE::DimTextV, "%s",
                SelectedEntry ? "No curves" : "Select a\nmodule");
        }
        ImGui::EndChild();

        ImGui::SameLine();

        // 우측 그래프 캔버스.
        const ImVec2 CanvasMin  = ImGui::GetCursorScreenPos();
        const ImVec2 CanvasSize = ImGui::GetContentRegionAvail();
        const ImVec2 CanvasMax(CanvasMin.x + CanvasSize.x, CanvasMin.y + CanvasSize.y);
        ImGui::Dummy(CanvasSize);

        ImDrawList* DrawList = ImGui::GetWindowDrawList();
        DrawList->AddRectFilled(CanvasMin, CanvasMax, PSE::ViewportBg, 4.0f);

        // 격자 + 시간/값 축.
        for (int32 i = 1; i < 10; ++i)
        {
            const float T   = static_cast<float>(i) / 10.0f;
            const ImU32 Col = (i == 5) ? PSE::GridMajor : PSE::GridMinor;
            DrawList->AddLine(ImVec2(CanvasMin.x + CanvasSize.x * T, CanvasMin.y),
                ImVec2(CanvasMin.x + CanvasSize.x * T, CanvasMax.y), Col);
            DrawList->AddLine(ImVec2(CanvasMin.x, CanvasMin.y + CanvasSize.y * T),
                ImVec2(CanvasMax.x, CanvasMin.y + CanvasSize.y * T), Col);
        }
        DrawList->AddText(ImVec2(CanvasMin.x + 4.0f, CanvasMax.y - 16.0f), PSE::DimText, "0.0");
        DrawList->AddText(ImVec2(CanvasMax.x - 26.0f, CanvasMax.y - 16.0f), PSE::DimText, "1.0");

        // TODO: 모듈의 Distribution 커브가 연결되면, 표시 중인 트랙별로
        //       키프레임을 폴리라인으로 그린다. 현재는 키 데이터가 없어 비어 있다.
        if (!SelectedEntry)
        {
            CanvasHint(DrawList, CanvasMin, CanvasMax, "Select a module to edit its curves");
        }
        else
        {
            CanvasHint(DrawList, CanvasMin, CanvasMax, "No keyframe data");
        }
    }
    EndPanel();
}
