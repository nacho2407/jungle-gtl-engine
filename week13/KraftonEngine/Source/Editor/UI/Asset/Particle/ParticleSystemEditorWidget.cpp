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
#include <cstring>

#include "Component/Light/DirectionalLightComponent.h"
#include "Component/Primitive/ParticleSystemComponent.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
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
    // LOD0의 모듈을 표준 순서(Required → Spawn → Modules → TypeData)로 펼친다.
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

    const char* ScreenAlignmentName(EParticleScreenAlignment V)
    {
        switch (V)
        {
        case PSA_FacingCameraPosition:      return "FacingCameraPosition";
        case PSA_Square:                    return "Square";
        case PSA_Rectangle:                 return "Rectangle";
        case PSA_Velocity:                  return "Velocity";
        case PSA_AwayFromCenter:            return "AwayFromCenter";
        case PSA_TypeSpecific:              return "TypeSpecific";
        case PSA_FacingCameraDistanceBlend: return "FacingCameraDistanceBlend";
        default:                            return "?";
        }
    }

    const char* SortModeName(EParticleSortMode V)
    {
        switch (V)
        {
        case PSORTMODE_None:             return "None";
        case PSORTMODE_ViewProjDepth:    return "ViewProjDepth";
        case PSORTMODE_DistanceToView:   return "DistanceToView";
        case PSORTMODE_Age_OldestFirst:  return "Age_OldestFirst";
        case PSORTMODE_Age_NewestFirst:  return "Age_NewestFirst";
        default:                         return "?";
        }
    }

    // 같은 LOD에 같은 타입 모듈이 이미 있는지 (중복 추가 방지).
    template<class T>
    bool HasModuleOfType(UParticleLODLevel* LOD)
    {
        if (!LOD) return false;
        for (UParticleModule* M : LOD->Modules)
        {
            if (Cast<T>(M)) return true;
        }
        return false;
    }

    // ── 공용 위젯 헬퍼 ───────────────────────────────────────────────────────

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

    bool BeginPanel(const char* StrId, const char* Title, float Width, float Height, const char* Context = nullptr)
    {
        Width  = (std::max)(Width, 48.0f);
        Height = (std::max)(Height, 48.0f);

        ImGui::PushStyleColor(ImGuiCol_ChildBg, PSE::PanelBg);
        ImGui::PushStyleColor(ImGuiCol_Border, PSE::Border);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 9.0f));
        // 패널 외곽 레이아웃은 ItemSpacing(0,0)을 쓰지만, 패널 내부 위젯들은
        // 숨막히지 않도록 일반적인 간격을 다시 켠다.
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 6.0f));

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
        ImGui::PopStyleVar(4);
        ImGui::PopStyleColor(2);
    }

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

    void CanvasHint(ImDrawList* DrawList, const ImVec2& Min, const ImVec2& Max, const char* Text)
    {
        const ImVec2 Size = ImGui::CalcTextSize(Text);
        DrawList->AddText(ImVec2((Min.x + Max.x - Size.x) * 0.5f, (Min.y + Max.y - Size.y) * 0.5f), PSE::DimText, Text);
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
    FAssetEditorWidget::Close();

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
        HandleKeyboardShortcuts();
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

    // 좌측: 프리뷰(위) + Details(아래, 크게).
    ImGui::BeginGroup();
    RenderViewportPanel(LeftW, LeftTopH);
    Splitter("##SplitLeftRow", false, LayoutH, LeftW, LeftRowRatio);
    RenderPropertiesPanel(LeftW, LeftBotH);
    ImGui::EndGroup();

    ImGui::SameLine();
    Splitter("##SplitColumn", true, LayoutW, LayoutH, ColumnRatio);
    ImGui::SameLine();

    // 우측: 이미터 cascade(위) + 커브 에디터(아래).
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
            // 동일 템플릿을 참조하는 레벨 내 컴포넌트는 Emitter Instance 캐시에
            // 옛 머티리얼/모듈 상태를 들고 있다. 저장 직후 ResetSystem으로 다시 빌드시킨다.
            RefreshExternalComponents(ParticleSystem);
        }
    }
}

void FParticleSystemEditorWidget::RefreshExternalComponents(UParticleSystem* Template)
{
    if (!Template || !GEngine) return;

    for (FWorldContext& WC : GEngine->GetWorldList())
    {
        if (!WC.World) continue;
        // 프리뷰 월드는 이미 RestartPreviewSimulation으로 갱신했다.
        if (WC.ContextHandle == PreviewWorldHandle) continue;

        for (AActor* Actor : WC.World->GetActors())
        {
            if (!Actor) continue;
            for (UActorComponent* Comp : Actor->GetComponents())
            {
                if (auto* PSC = Cast<UParticleSystemComponent>(Comp))
                {
                    if (PSC->GetTemplate() == Template)
                    {
                        PSC->ResetSystem();
                    }
                }
            }
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
    SelectedEmitterIndex = EmitterIndex;
    SelectedModuleIndex  = ModuleIndex;
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
    EmitterNameBufFor    = -1;

    SyncEmitterUIState();

    MarkDirty();
    RestartPreviewSimulation();
}

void FParticleSystemEditorWidget::DuplicateEmitter(int32 SourceIndex)
{
    UParticleSystem* ParticleSystem = GetParticleSystem();
    if (!ParticleSystem) return;

    TArray<UParticleEmitter*>& Emitters = ParticleSystem->GetEmitters();
    if (SourceIndex < 0 || SourceIndex >= static_cast<int32>(Emitters.size())) return;

    UParticleEmitter* Src = Emitters[SourceIndex];
    if (!Src) return;

    UParticleEmitter* Dst = UObjectManager::Get().CreateObject<UParticleEmitter>(ParticleSystem);
    if (!Dst) return;

    Dst->InitializeDefaultSpriteEmitter();

    // 기본 이미터 필드.
    Dst->EmitterName            = FName(Src->EmitterName.ToString() + "_Copy");
    Dst->bUseMeshInstance       = Src->bUseMeshInstance;
    Dst->PivotOffset            = Src->PivotOffset;
    Dst->InitialAllocationCount = Src->InitialAllocationCount;
    Dst->SetEnabled(Src->IsEnabled());

    UParticleLODLevel* SrcLOD = Src->GetLODLevel(0);
    UParticleLODLevel* DstLOD = Dst->GetLODLevel(0);

    if (SrcLOD && DstLOD)
    {
        // Required 복사 (default emitter가 이미 생성해 둔 인스턴스에 값만 덮어쓴다).
        if (UParticleModuleRequired* SR = SrcLOD->RequiredModule)
        {
            if (UParticleModuleRequired* DR = DstLOD->RequiredModule)
            {
                DR->MaterialSlot         = SR->MaterialSlot;
                DR->EmitterOrigin        = SR->EmitterOrigin;
                DR->EmitterRotation      = SR->EmitterRotation;
                DR->bUseLocalSpace       = SR->bUseLocalSpace;
                DR->bKillOnDeactivate    = SR->bKillOnDeactivate;
                DR->bKillOnCompleted     = SR->bKillOnCompleted;
                DR->bDelayFirstLoopOnly  = SR->bDelayFirstLoopOnly;
                DR->EmitterDuration      = SR->EmitterDuration;
                DR->EmitterDurationLow   = SR->EmitterDurationLow;
                DR->EmitterDelay         = SR->EmitterDelay;
                DR->EmitterLoops         = SR->EmitterLoops;
                DR->ScreenAlignment      = SR->ScreenAlignment;
                DR->SortMode             = SR->SortMode;
                DR->SubImages_Horizontal = SR->SubImages_Horizontal;
                DR->SubImages_Vertical   = SR->SubImages_Vertical;
                DR->SpawnRate            = SR->SpawnRate;
                DR->BurstList            = SR->BurstList;
                DR->bUseMaxDrawCount     = SR->bUseMaxDrawCount;
                DR->MaxDrawCount         = SR->MaxDrawCount;
                DR->bEnabled             = SR->bEnabled;
                DR->ResolveMaterialFromSlot();
            }
        }
        // Spawn 복사.
        if (UParticleModuleSpawn* SS = SrcLOD->SpawnModule)
        {
            if (UParticleModuleSpawn* DS = DstLOD->SpawnModule)
            {
                DS->SpawnRate      = SS->SpawnRate;
                DS->SpawnRateScale = SS->SpawnRateScale;
                DS->BurstScale     = SS->BurstScale;
                DS->BurstList      = SS->BurstList;
                DS->bEnabled       = SS->bEnabled;
            }
        }
        // 추가 모듈 (Lifetime/Location/Velocity/Size/Color)을 같은 타입으로 신규 생성 + 얕은 복사.
        for (UParticleModule* M : SrcLOD->Modules)
        {
            if (!M) continue;
            UParticleModule* NewModule = nullptr;
            if (auto* X = Cast<UParticleModuleLifetime>(M))
            {
                auto* N = UObjectManager::Get().CreateObject<UParticleModuleLifetime>(DstLOD);
                N->LifetimeMin = X->LifetimeMin;
                N->LifetimeMax = X->LifetimeMax;
                NewModule = N;
            }
            else if (auto* X = Cast<UParticleModuleLocation>(M))
            {
                auto* N = UObjectManager::Get().CreateObject<UParticleModuleLocation>(DstLOD);
                N->StartLocation = X->StartLocation;
                NewModule = N;
            }
            else if (auto* X = Cast<UParticleModuleVelocity>(M))
            {
                auto* N = UObjectManager::Get().CreateObject<UParticleModuleVelocity>(DstLOD);
                N->MinVelocity = X->MinVelocity;
                N->MaxVelocity = X->MaxVelocity;
                NewModule = N;
            }
            else if (auto* X = Cast<UParticleModuleSize>(M))
            {
                auto* N = UObjectManager::Get().CreateObject<UParticleModuleSize>(DstLOD);
                N->StartSize = X->StartSize;
                NewModule = N;
            }
            else if (auto* X = Cast<UParticleModuleColor>(M))
            {
                auto* N = UObjectManager::Get().CreateObject<UParticleModuleColor>(DstLOD);
                N->StartColor  = X->StartColor;
                N->StartAlpha  = X->StartAlpha;
                N->bClampAlpha = X->bClampAlpha;
                NewModule = N;
            }
            if (NewModule)
            {
                NewModule->bEnabled = M->bEnabled;
                DstLOD->Modules.push_back(NewModule);
            }
        }
        DstLOD->UpdateModuleLists();
    }

    Emitters.push_back(Dst);

    const int32 NewIndex = static_cast<int32>(Emitters.size()) - 1;
    SelectEmitter(NewIndex, -1);

    MarkDirty();
    RestartPreviewSimulation();
}

void FParticleSystemEditorWidget::DeleteSelectedModule()
{
    UParticleSystem* ParticleSystem = GetParticleSystem();
    if (!ParticleSystem) return;
    if (SelectedEmitterIndex < 0 || SelectedEmitterIndex >= static_cast<int32>(ParticleSystem->GetEmitters().size())) return;
    if (SelectedModuleIndex < 0) return;

    UParticleEmitter*  Emitter = ParticleSystem->GetEmitters()[SelectedEmitterIndex];
    if (!Emitter) return;
    UParticleLODLevel* LOD0    = Emitter->GetLODLevel(0);
    if (!LOD0) return;

    TArray<FEmitterModuleEntry> ModuleList;
    BuildEmitterModuleList(Emitter, ModuleList);

    if (SelectedModuleIndex >= static_cast<int32>(ModuleList.size())) return;

    UParticleModule* Target = ModuleList[SelectedModuleIndex].Module;
    if (!Target) return;

    // Required/Spawn/TypeData는 슬롯 자체를 비우면 시뮬레이션이 깨진다. 삭제 금지.
    if (Target == LOD0->RequiredModule) return;
    if (Target == LOD0->SpawnModule)    return;
    if (Target == static_cast<UParticleModule*>(LOD0->TypeDataModule)) return;

    auto It = std::find(LOD0->Modules.begin(), LOD0->Modules.end(), Target);
    if (It == LOD0->Modules.end()) return;

    LOD0->Modules.erase(It);
    UObjectManager::Get().DestroyObject(Target);
    LOD0->UpdateModuleLists();

    SelectedModuleIndex = -1;

    MarkDirty();
    RestartPreviewSimulation();
}

void FParticleSystemEditorWidget::SyncEmitterUIState()
{
    UParticleSystem* ParticleSystem = GetParticleSystem();

    const int32 EmitterCount = ParticleSystem ? static_cast<int32>(ParticleSystem->GetEmitters().size()) : 0;

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

void FParticleSystemEditorWidget::HandleKeyboardShortcuts()
{
    ImGuiIO& IO = ImGui::GetIO();
    if (IO.WantTextInput) return;

    if (IO.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false))
    {
        if (IsDirty())
        {
            SaveAsset();
        }
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Delete, false))
    {
        if (SelectedModuleIndex >= 0)
        {
            DeleteSelectedModule();
        }
        else if (SelectedEmitterIndex >= 0)
        {
            DeleteSelectedEmitter();
        }
    }
    if (ImGui::IsKeyPressed(ImGuiKey_F, false))
    {
        if (ViewportClient.IsRenderable())
        {
            ViewportClient.ResetCameraToPreviewBounds();
        }
    }
}

// ── 메뉴 바 ─────────────────────────────────────────────────────────────────
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
        ImGui::Separator();
        if (ImGui::MenuItem("Close"))
        {
            Close();
        }
        ImGui::EndMenu();
    }

    ImGui::EndMenuBar();
}

// ── 툴바 (살아있는 버튼만) ─────────────────────────────────────────────────
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

    if (Tool("Save", "Save the particle system (Ctrl+S)", IsDirty()))
    {
        SaveAsset();
    }
    Group();

    if (Tool("Restart Sim", "Restart the preview simulation"))
    {
        RestartPreviewSimulation();
    }
    if (Tool(bSimulating ? "Pause" : "Play", "Toggle preview simulation"))
    {
        bSimulating = !bSimulating;
        if (PreviewPSC)
        {
            if (bSimulating) PreviewPSC->Activate();
            else             PreviewPSC->Deactivate();
        }
    }
    if (Tool("Frame", "Frame preview camera on emitters (F)", ViewportClient.IsRenderable()))
    {
        ViewportClient.ResetCameraToPreviewBounds();
    }

    ImGui::PopStyleVar(3);
}

// ── 상태 바 ─────────────────────────────────────────────────────────────────
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

        ImGui::SameLine();
        ImGui::TextColored(PSE::DimTextV, "  |  Sim %.2fs %s", PreviewTime, bSimulating ? "(playing)" : "(paused)");
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ── 프리뷰 뷰포트 ──────────────────────────────────────────────────────────
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

// ── 이미터 + 모듈 (cascade) ─────────────────────────────────────────────────
void FParticleSystemEditorWidget::RenderEmittersPanel(float Width, float Height)
{
    UParticleSystem* ParticleSystem = GetParticleSystem();
    const int32      EmitterCount   = ParticleSystem ? static_cast<int32>(ParticleSystem->GetEmitters().size()) : 0;

    char Context[32];
    std::snprintf(Context, sizeof(Context), "%d emitter%s", EmitterCount, EmitterCount == 1 ? "" : "s");

    if (BeginPanel("##PSEEmitters", "Emitters", Width, Height, Context))
    {
        constexpr float ColumnWidth = 178.0f;
        if (ImGui::BeginChild("##PSEEmitterColumns", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_HorizontalScrollbar))
        {
            int32 EmitterToDelete    = -1;
            int32 EmitterToDuplicate = -1;

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
                        ? Emitter->EmitterName.ToString()
                        : (FString("Emitter ") + std::to_string(EmitterIndex));

                    const bool bHeaderSel = bEmitterSelected && SelectedModuleIndex < 0;

                    // x 버튼 폭만큼 셀렉터블 너비를 줄인다.
                    constexpr float CloseBtnW = 20.0f;
                    const float     RowW      = ImGui::GetContentRegionAvail().x;
                    if (ImGui::Selectable(EmitterLabel.c_str(), bHeaderSel, 0, ImVec2(RowW - CloseBtnW - 4.0f, 0.0f)))
                    {
                        SelectEmitter(EmitterIndex, -1);
                    }
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
                        if (Emitter)
                        {
                            Emitter->SetEnabled(bEnabled);
                        }
                        MarkDirty();
                        RestartPreviewSimulation();
                    }
                    ImGui::Separator();

                    TArray<FEmitterModuleEntry> ModuleList;
                    BuildEmitterModuleList(Emitter, ModuleList);

                    int32 ModuleToDelete = -1;
                    for (int32 ModuleIndex = 0; ModuleIndex < static_cast<int32>(ModuleList.size()); ++ModuleIndex)
                    {
                        ImGui::PushID(ModuleIndex);
                        const FEmitterModuleEntry& Entry     = ModuleList[ModuleIndex];
                        const bool                 bSelected = bEmitterSelected && (SelectedModuleIndex == ModuleIndex);

                        if (ImGui::Selectable(Entry.Name, bSelected))
                        {
                            SelectEmitter(EmitterIndex, ModuleIndex);
                        }
                        if (ImGui::BeginPopupContextItem("##ModuleCtx"))
                        {
                            UParticleLODLevel* LOD0 = Emitter ? Emitter->GetLODLevel(0) : nullptr;
                            const bool bIsCore = LOD0 && (
                                Entry.Module == LOD0->RequiredModule ||
                                Entry.Module == LOD0->SpawnModule ||
                                Entry.Module == static_cast<UParticleModule*>(LOD0->TypeDataModule));

                            if (ImGui::MenuItem("Delete Module", "Del", false, !bIsCore))
                            {
                                SelectEmitter(EmitterIndex, ModuleIndex);
                                ModuleToDelete = ModuleIndex;
                            }
                            if (bIsCore && ImGui::IsItemHovered())
                            {
                                ImGui::SetTooltip("Required/Spawn/TypeData cannot be removed");
                            }
                            ImGui::Separator();
                            bool bModEnabled = Entry.Module ? (Entry.Module->bEnabled != 0) : true;
                            if (ImGui::MenuItem("Enabled", nullptr, &bModEnabled))
                            {
                                if (Entry.Module) Entry.Module->bEnabled = bModEnabled ? 1 : 0;
                                MarkDirty();
                                RestartPreviewSimulation();
                            }
                            ImGui::EndPopup();
                        }
                        ImGui::PopID();
                    }
                    if (ModuleToDelete >= 0)
                    {
                        SelectEmitter(EmitterIndex, ModuleToDelete);
                        DeleteSelectedModule();
                    }

                    ImGui::Separator();

                    // + Module 팝업 — LOD0에 이미 있는 타입은 비활성.
                    if (ImGui::SmallButton("+ Module"))
                    {
                        ImGui::OpenPopup("##AddModulePopup");
                    }
                    if (ImGui::BeginPopup("##AddModulePopup"))
                    {
                        UParticleLODLevel* LOD0 = Emitter ? Emitter->GetLODLevel(0) : nullptr;

                        auto AddItem = [&](const char* Label, auto Creator)
                        {
                            const bool bExists = Creator(/*query*/true, LOD0);
                            if (ImGui::MenuItem(Label, nullptr, false, LOD0 && !bExists))
                            {
                                Creator(/*query*/false, LOD0);
                                SelectEmitter(EmitterIndex, -1);
                                MarkDirty();
                                RestartPreviewSimulation();
                            }
                        };

                        AddItem("Lifetime", [](bool bQuery, UParticleLODLevel* L)
                        {
                            if (bQuery) return HasModuleOfType<UParticleModuleLifetime>(L);
                            auto* N = UObjectManager::Get().CreateObject<UParticleModuleLifetime>(L);
                            L->Modules.push_back(N);
                            L->UpdateModuleLists();
                            return true;
                        });
                        AddItem("Location", [](bool bQuery, UParticleLODLevel* L)
                        {
                            if (bQuery) return HasModuleOfType<UParticleModuleLocation>(L);
                            auto* N = UObjectManager::Get().CreateObject<UParticleModuleLocation>(L);
                            L->Modules.push_back(N);
                            L->UpdateModuleLists();
                            return true;
                        });
                        AddItem("Velocity", [](bool bQuery, UParticleLODLevel* L)
                        {
                            if (bQuery) return HasModuleOfType<UParticleModuleVelocity>(L);
                            auto* N = UObjectManager::Get().CreateObject<UParticleModuleVelocity>(L);
                            L->Modules.push_back(N);
                            L->UpdateModuleLists();
                            return true;
                        });
                        AddItem("Size", [](bool bQuery, UParticleLODLevel* L)
                        {
                            if (bQuery) return HasModuleOfType<UParticleModuleSize>(L);
                            auto* N = UObjectManager::Get().CreateObject<UParticleModuleSize>(L);
                            L->Modules.push_back(N);
                            L->UpdateModuleLists();
                            return true;
                        });
                        AddItem("Color", [](bool bQuery, UParticleLODLevel* L)
                        {
                            if (bQuery) return HasModuleOfType<UParticleModuleColor>(L);
                            auto* N = UObjectManager::Get().CreateObject<UParticleModuleColor>(L);
                            L->Modules.push_back(N);
                            L->UpdateModuleLists();
                            return true;
                        });

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
        }
        ImGui::EndChild();
    }
    EndPanel();
}

// ── 프로퍼티 (스크롤 거의 없는 그룹화) ─────────────────────────────────────
void FParticleSystemEditorWidget::RenderPropertiesPanel(float Width, float Height)
{
    UParticleSystem* ParticleSystem = GetParticleSystem();

    FString Context = "Particle System";

    UParticleEmitter* SelectedEmitter = nullptr;
    if (ParticleSystem && SelectedEmitterIndex >= 0 &&
        SelectedEmitterIndex < static_cast<int32>(ParticleSystem->GetEmitters().size()))
    {
        SelectedEmitter = ParticleSystem->GetEmitters()[SelectedEmitterIndex];
    }

    TArray<FEmitterModuleEntry> ModuleList;
    BuildEmitterModuleList(SelectedEmitter, ModuleList);

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
        // 위젯이 패널 폭을 다 먹어 라벨이 잘리는 일이 없도록 우측에 160px를 라벨 영역으로 남긴다.
        ImGui::PushItemWidth(-160.0f);

        if (SelectedEmitterIndex < 0)
        {
            // 파티클 시스템 자체 — read-only 요약만.
            ImGui::TextColored(PSE::DimTextV, "Source Path");
            ImGui::TextWrapped("%s", ParticleSystem && !ParticleSystem->GetSourcePath().empty()
                ? ParticleSystem->GetSourcePath().c_str() : "(unsaved)");
            ImGui::Spacing();
            ImGui::TextColored(PSE::DimTextV, "Emitter Count");
            ImGui::Text("%d", ParticleSystem ? static_cast<int32>(ParticleSystem->GetEmitters().size()) : 0);
            ImGui::Spacing();
            ImGui::TextColored(PSE::DimTextV, "Status");
            ImGui::Text("%s", IsDirty() ? "Modified" : "Saved");
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
                    const FString s = SelectedEmitter->EmitterName.ToString();
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
                        bChanged = true;
                    }
                }

                bool bEnabled = SelectedEmitter ? SelectedEmitter->IsEnabled() : true;
                if (ImGui::Checkbox("Enabled", &bEnabled))
                {
                    if (SelectedEmitter) SelectedEmitter->SetEnabled(bEnabled);
                    bChanged = true;
                    RestartPreviewSimulation();
                }

                if (SelectedEmitter)
                {
                    bChanged |= ImGui::Checkbox("Use Mesh Instance", &SelectedEmitter->bUseMeshInstance);
                    bChanged |= ImGui::DragInt("Initial Alloc Count", &SelectedEmitter->InitialAllocationCount,
                                               1.0f, 0, 100000);
                    bChanged |= ImGui::DragFloat3("Pivot Offset", SelectedEmitter->PivotOffset.Data, 0.01f);
                }

                if (bChanged) MarkDirty();
            }

            ImGui::SetNextItemOpen(true, ImGuiCond_Once);
            if (ImGui::CollapsingHeader("LOD"))
            {
                const int32 LODCount = SelectedEmitter
                    ? static_cast<int32>(SelectedEmitter->GetLODLevels().size()) : 0;
                ImGui::Text("Levels: %d", LODCount);
                ImGui::Text("Modules in LOD0: %d", static_cast<int32>(ModuleList.size()));
            }
        }
        else
        {
            // 모듈 편집.
            RenderModuleProperties(SelectedModule);
        }

        ImGui::PopItemWidth();
    }
    EndPanel();
}

// ── 모듈 프로퍼티 편집 (CollapsingHeader 그룹) ──────────────────────────────
void FParticleSystemEditorWidget::RenderModuleProperties(UParticleModule* Module)
{
    if (!Module)
    {
        return;
    }

    bool bChanged       = false;
    bool bMaterialDirty = false;

    // Required/Spawn은 이미터 동작에 필수라 disable 토글이 무의미하다. 그 외 모듈만 노출.
    const bool bIsCoreModule = Cast<UParticleModuleRequired>(Module) || Cast<UParticleModuleSpawn>(Module);
    if (!bIsCoreModule)
    {
        bool bModuleEnabled = Module->bEnabled != 0;
        if (ImGui::Checkbox("Module Enabled", &bModuleEnabled))
        {
            Module->bEnabled = bModuleEnabled ? 1 : 0;
            bChanged = true;
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
                    bChanged = bMaterialDirty = true;
                }
                if (bSelected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::Spacing();

        // ── Emitter ──
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        if (ImGui::CollapsingHeader("Emitter##Req"))
        {
            bChanged |= ImGui::DragFloat3("Origin",   Required->EmitterOrigin.Data, 0.1f);

            float RotPYR[3] = {
                Required->EmitterRotation.Pitch,
                Required->EmitterRotation.Yaw,
                Required->EmitterRotation.Roll
            };
            if (ImGui::DragFloat3("Rotation P/Y/R", RotPYR, 0.5f))
            {
                Required->EmitterRotation.Pitch = RotPYR[0];
                Required->EmitterRotation.Yaw   = RotPYR[1];
                Required->EmitterRotation.Roll  = RotPYR[2];
                bChanged = true;
            }
            bChanged |= ImGui::DragFloat("Duration",     &Required->EmitterDuration,    0.05f, 0.0f, 10000.0f);
            bChanged |= ImGui::DragFloat("Duration Low", &Required->EmitterDurationLow, 0.05f, 0.0f, 10000.0f);
            bChanged |= ImGui::DragFloat("Delay",        &Required->EmitterDelay,       0.05f, 0.0f, 10000.0f);
            bChanged |= ImGui::DragInt  ("Loops",        &Required->EmitterLoops,       1.0f,  0,    10000);
        }

        // ── Spawn ──
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        if (ImGui::CollapsingHeader("Spawn##Req"))
        {
            bChanged |= ImGui::DragFloat("Spawn Rate", &Required->SpawnRate, 0.1f, 0.0f, 10000.0f);
            ImGui::Spacing();
            ImGui::TextColored(PSE::DimTextV, "Bursts");
            RenderBurstList(Required->BurstList);
        }

        // ── Rendering ──
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        if (ImGui::CollapsingHeader("Rendering##Req"))
        {
            if (ImGui::BeginCombo("Screen Alignment", ScreenAlignmentName(Required->ScreenAlignment)))
            {
                for (int32 i = 0; i < PSA_MAX; ++i)
                {
                    const auto V = static_cast<EParticleScreenAlignment>(i);
                    const bool bSel = (Required->ScreenAlignment == V);
                    if (ImGui::Selectable(ScreenAlignmentName(V), bSel))
                    {
                        Required->ScreenAlignment = V;
                        bChanged = true;
                    }
                    if (bSel) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            if (ImGui::BeginCombo("Sort Mode", SortModeName(Required->SortMode)))
            {
                for (int32 i = 0; i < PSORTMODE_MAX; ++i)
                {
                    const auto V = static_cast<EParticleSortMode>(i);
                    const bool bSel = (Required->SortMode == V);
                    if (ImGui::Selectable(SortModeName(V), bSel))
                    {
                        Required->SortMode = V;
                        bChanged = true;
                    }
                    if (bSel) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            bChanged |= ImGui::Checkbox("Use Max Draw Count", &Required->bUseMaxDrawCount);
            if (!Required->bUseMaxDrawCount) ImGui::BeginDisabled();
            bChanged |= ImGui::DragInt("Max Draw Count", &Required->MaxDrawCount, 1.0f, 0, 100000);
            if (Required->MaxDrawCount < 0) { Required->MaxDrawCount = 0; bChanged = true; }
            if (!Required->bUseMaxDrawCount) ImGui::EndDisabled();
        }

        // ── Sub-UV ──
        if (ImGui::CollapsingHeader("Sub-UV##Req"))
        {
            bChanged |= ImGui::DragInt("Horizontal", &Required->SubImages_Horizontal, 1.0f, 1, 64);
            bChanged |= ImGui::DragInt("Vertical",   &Required->SubImages_Vertical,   1.0f, 1, 64);
        }

        // ── Flags ──
        if (ImGui::CollapsingHeader("Flags##Req"))
        {
            bool bUseLocal = Required->bUseLocalSpace;
            if (ImGui::Checkbox("Use Local Space", &bUseLocal))
            { Required->bUseLocalSpace = bUseLocal ? 1 : 0; bChanged = true; }

            bool bKillDeact = Required->bKillOnDeactivate;
            if (ImGui::Checkbox("Kill on Deactivate", &bKillDeact))
            { Required->bKillOnDeactivate = bKillDeact ? 1 : 0; bChanged = true; }

            bool bKillComp = Required->bKillOnCompleted;
            if (ImGui::Checkbox("Kill on Completed", &bKillComp))
            { Required->bKillOnCompleted = bKillComp ? 1 : 0; bChanged = true; }

            bChanged |= ImGui::Checkbox("Delay First Loop Only", &Required->bDelayFirstLoopOnly);
        }
    }
    else if (UParticleModuleSpawn* Spawn = Cast<UParticleModuleSpawn>(Module))
    {
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        if (ImGui::CollapsingHeader("Spawn"))
        {
            bChanged |= ImGui::DragFloat("Spawn Rate",       &Spawn->SpawnRate,      0.1f,  0.0f, 10000.0f);
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
                bChanged = true;
            }
        }
    }
    else if (UParticleModuleLocation* Location = Cast<UParticleModuleLocation>(Module))
    {
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        if (ImGui::CollapsingHeader("Location"))
        {
            bChanged |= ImGui::DragFloat3("Start Location", Location->StartLocation.Data, 0.1f);
        }
    }
    else if (UParticleModuleVelocity* Velocity = Cast<UParticleModuleVelocity>(Module))
    {
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        if (ImGui::CollapsingHeader("Velocity"))
        {
            bChanged |= ImGui::DragFloat3("Min", Velocity->MinVelocity.Data, 0.5f);
            bChanged |= ImGui::DragFloat3("Max", Velocity->MaxVelocity.Data, 0.5f);
        }
    }
    else if (UParticleModuleSize* Size = Cast<UParticleModuleSize>(Module))
    {
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        if (ImGui::CollapsingHeader("Size"))
        {
            bChanged |= ImGui::DragFloat3("Start Size", Size->StartSize.Data, 0.05f, 0.0f, 10000.0f);
        }
    }
    else if (UParticleModuleColor* Color = Cast<UParticleModuleColor>(Module))
    {
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        if (ImGui::CollapsingHeader("Color"))
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
                bChanged = true;
            }
            bChanged |= ImGui::DragFloat("Start Alpha", &Color->StartAlpha, 0.01f, 0.0f, 1.0f);
            bool bClamp = Color->bClampAlpha;
            if (ImGui::Checkbox("Clamp Alpha", &bClamp))
            { Color->bClampAlpha = bClamp ? 1 : 0; bChanged = true; }
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

    if (bChanged)
    {
        MarkDirty();
    }
    if (bMaterialDirty)
    {
        // 머티리얼 변경은 인스턴스 캐시(렌더 상태)에 영향이 커서 시뮬레이션을 다시 시작.
        RestartPreviewSimulation();
    }
}

// ── Burst List 편집 테이블 ──────────────────────────────────────────────────
void FParticleSystemEditorWidget::RenderBurstList(TArray<FParticleBurst>& Bursts)
{
    bool bChanged = false;
    int32 ToRemove = -1;

    constexpr ImGuiTableFlags TableFlags = ImGuiTableFlags_BordersInnerV |
                                           ImGuiTableFlags_RowBg        |
                                           ImGuiTableFlags_SizingStretchSame;

    if (ImGui::BeginTable("##Bursts", 4, TableFlags))
    {
        ImGui::TableSetupColumn("Time",      ImGuiTableColumnFlags_WidthStretch, 1.2f);
        ImGui::TableSetupColumn("Count",     ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("Count Low", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("",          ImGuiTableColumnFlags_WidthFixed,   24.0f);
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
            ImGui::SetNextItemWidth(-1.0f);
            bChanged |= ImGui::DragInt("##cl", &B.CountLow, 1.0f, -1, 100000);

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

// ── 커브 에디터 (placeholder — Distribution 연결 시 채워짐) ────────────────
void FParticleSystemEditorWidget::RenderCurveEditorPanel(float Width, float Height)
{
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
        const ImVec2 CanvasMin  = ImGui::GetCursorScreenPos();
        const ImVec2 CanvasSize = ImGui::GetContentRegionAvail();
        const ImVec2 CanvasMax(CanvasMin.x + CanvasSize.x, CanvasMin.y + CanvasSize.y);
        ImGui::Dummy(CanvasSize);

        ImDrawList* DrawList = ImGui::GetWindowDrawList();
        DrawList->AddRectFilled(CanvasMin, CanvasMax, PSE::ViewportBg, 4.0f);

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

        if (!SelectedEntry)
        {
            CanvasHint(DrawList, CanvasMin, CanvasMax, "Select a module to edit its curves");
        }
        else
        {
            // TODO: 모듈이 FRawDistribution* 필드를 노출하기 시작하면 키프레임 폴리라인을 그린다.
            CanvasHint(DrawList, CanvasMin, CanvasMax, "No keyframe data (distribution not bound)");
        }
    }
    EndPanel();
}
