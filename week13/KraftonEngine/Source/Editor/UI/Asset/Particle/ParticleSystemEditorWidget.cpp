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
#include "Particles/Color/ParticleModuleColorOverLife.h"
#include "Particles/Lifetime/ParticleModuleLifetime.h"
#include "Particles/Location/ParticleModuleLocation.h"
#include "Particles/Size/ParticleModuleSize.h"
#include "Particles/Spawn/ParticleModuleSpawn.h"
#include "Particles/TypeData/ParticleModuleTypeDataBase.h"
#include "Particles/Velocity/ParticleModuleVelocity.h"
#include "Materials/Material.h"
#include "Materials/Graph/MaterialGraphAsset.h"
#include "Materials/MaterialManager.h"

#include "Engine/Distributions/DistributionVector.h"
#include "Engine/Distributions/DistributionFloat.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <random>

#include "Component/Light/DirectionalLightComponent.h"
#include "Component/Primitive/ParticleSystemComponent.h"
#include "Editor/EditorEngine.h"
#include "Editor/Subsystem/AssetFactory.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "GameFramework/WorldContext.h"
#include "GameFramework/Light/DirectionalLightActor.h"
#include "Editor/UI/Util/EditorTextureManager.h"
#include "Platform/Paths.h"
#include "Runtime/Engine.h"
#include "Slate/SlateApplication.h"
#include "Object/Reflection/ObjectFactory.h"
#include "Serialization/MemoryArchive.h"
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

    void DrawRawDistributionVector(const char* Label, FRawDistributionVector& Raw, bool& bChanged, UObject* Outer)
    {
        if (ImGui::TreeNode(Label))
        {
            const char* TypeStr = "None";
            if (Cast<UDistributionVectorConstant>(Raw.Distribution))TypeStr = "Constant";
            else if (Cast<UDistributionVectorUniform>(Raw.Distribution)) TypeStr = "Uniform";

            if (ImGui::BeginCombo("Type", TypeStr))
            {
                if (ImGui::Selectable("Constant", TypeStr == "Constant"))
                {
                    Raw.Distribution = UObjectManager::Get().CreateObject<UDistributionVectorConstant>(Outer);
                    bChanged = true;
                }
                if (ImGui::Selectable("Uniform", TypeStr == "Uniform"))
                {
                    Raw.Distribution = UObjectManager::Get().CreateObject<UDistributionVectorUniform>(Outer);
                    bChanged = true;
                }
                ImGui::EndCombo();
            }

            if (UDistributionVectorConstant* Constant = Cast<UDistributionVectorConstant>(Raw.Distribution))
            {
                bChanged |= ImGui::DragFloat3("Value", Constant->Constant.Data, 0.5f);
            }
            else if (UDistributionVectorUniform* Uniform = Cast<UDistributionVectorUniform>(Raw.Distribution))
            {
                bChanged |= ImGui::DragFloat3("Min", Uniform->Min.Data, 0.5f);
                bChanged |= ImGui::DragFloat3("Max", Uniform->Max.Data, 0.5f);
            }
            ImGui::TreePop();
        }
    }

    void DrawRawDistributionFloat(const char* Label, FRawDistributionFloat& Raw, bool& bChanged, UObject* Outer)
    {
        if (ImGui::TreeNode(Label))
        {
            const char* TypeStr = "None";
            if (Cast<UDistributionFloatConstant>(Raw.Distribution)) TypeStr = "Constant";
            else if (Cast<UDistributionFloatUniform>(Raw.Distribution)) TypeStr = "Uniform";

            if (ImGui::BeginCombo("Type", TypeStr))
            {
                if (ImGui::Selectable("Constant", TypeStr == "Constant"))
                {
                    Raw.Distribution = UObjectManager::Get().CreateObject<UDistributionFloatConstant>(Outer);
                    bChanged = true;
                }
                if (ImGui::Selectable("Uniform", TypeStr == "Uniform"))
                {
                    Raw.Distribution = UObjectManager::Get().CreateObject<UDistributionFloatUniform>(Outer);
                    bChanged = true;
                }
                ImGui::EndCombo();
            }

            if (UDistributionFloatConstant* Constant = Cast<UDistributionFloatConstant>(Raw.Distribution))
            {
                bChanged |= ImGui::DragFloat("Value", &Constant->Constant, 0.5f);
            }
            else if (UDistributionFloatUniform* Uniform = Cast<UDistributionFloatUniform>(Raw.Distribution))
            {
                bChanged |= ImGui::DragFloat("Min", &Uniform->Min, 0.5f);
                bChanged |= ImGui::DragFloat("Max", &Uniform->Max, 0.5f);
            }
            ImGui::TreePop();
        }
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
        if (Cast<UParticleModuleColorOverLife>(Module)) return "Color Over Life";
        if (Cast<UParticleModuleColor>(Module))    return "Color";
        return "Module";
    }

    void BuildEmitterModuleListAt(UParticleEmitter* Emitter, int32 LODIndex, TArray<FEmitterModuleEntry>& OutList)
    {
        OutList.clear();
        if (!Emitter) return;

        UParticleLODLevel* LOD = Emitter->GetLODLevel(LODIndex);
        if (!LOD) return;

        if (LOD->RequiredModule)
        {
            OutList.push_back({ "Required", LOD->RequiredModule });
        }
        if (LOD->SpawnModule)
        {
            OutList.push_back({ "Spawn", LOD->SpawnModule });
        }
        for (UParticleModule* Module : LOD->Modules)
        {
            if (!Module) continue;
            OutList.push_back({ GetModuleDisplayName(Module), Module });
        }
        if (LOD->TypeDataModule)
        {
            OutList.push_back({ "TypeData", static_cast<UParticleModule*>(LOD->TypeDataModule) });
        }
    }

    // 호환용 — 기존 LOD0 전용 호출 사이트.
    void BuildEmitterModuleList(UParticleEmitter* Emitter, TArray<FEmitterModuleEntry>& OutList)
    {
        BuildEmitterModuleListAt(Emitter, 0, OutList);
    }

    int32 GetParticleSystemLODCount(UParticleSystem* ParticleSystem)
    {
        int32 LODCount = 0;
        if (ParticleSystem)
        {
            for (UParticleEmitter* Emitter : ParticleSystem->GetEmitters())
            {
                if (!Emitter) continue;
                LODCount = (std::max)(LODCount, static_cast<int32>(Emitter->GetLODLevels().size()));
            }
            if (LODCount <= 0)
            {
                LODCount = static_cast<int32>(ParticleSystem->LODDistances.size());
            }
        }
        return (std::max)(LODCount, 1);
    }

    void SyncParticleSystemLODDistances(UParticleSystem* ParticleSystem)
    {
        if (!ParticleSystem) return;

        const int32 LODCount = GetParticleSystemLODCount(ParticleSystem);
        TArray<float>& Dist = ParticleSystem->LODDistances;

        if (Dist.empty())
        {
            Dist.push_back(0.0f);
        }
        if (LODCount > 1
            && static_cast<int32>(Dist.size()) == LODCount - 1
            && Dist[0] > 0.0f)
        {
            Dist.insert(Dist.begin(), 0.0f);
        }
        while (static_cast<int32>(Dist.size()) < LODCount)
        {
            const float Prev = Dist.empty() ? 0.0f : Dist.back();
            Dist.push_back(Prev > 0.0f ? Prev * 2.0f : 1000.0f);
        }
        if (static_cast<int32>(Dist.size()) > LODCount)
        {
            Dist.resize(LODCount);
        }

        Dist[0] = 0.0f;
        for (float& Value : Dist)
        {
            Value = (std::max)(0.0f, Value);
        }
    }

    // SelectedLODIndex의 같은-위치 모듈이 상위(LODIndex-1) LOD의 같은-위치 모듈과
    // 동일 포인터인지 검사한다. 동일하면 "공유 중"이므로 sub-LOD에서 편집 금지.
    bool IsModuleSharedWithHigher(UParticleEmitter* Emitter, int32 LODIndex, int32 ModuleIndex)
    {
        if (!Emitter || LODIndex <= 0) return false;
        TArray<FEmitterModuleEntry> Cur, Hi;
        BuildEmitterModuleListAt(Emitter, LODIndex,     Cur);
        BuildEmitterModuleListAt(Emitter, LODIndex - 1, Hi);
        if (ModuleIndex < 0
            || ModuleIndex >= static_cast<int32>(Cur.size())
            || ModuleIndex >= static_cast<int32>(Hi.size()))
        {
            return false;
        }
        return Cur[ModuleIndex].Module == Hi[ModuleIndex].Module;
    }
    // 직렬화 라운드트립으로 모듈을 깊은 복사. ParticleSystem 의 Serialize 인프라가 이미
    // 모든 모듈 필드를 지원하므로 일일이 필드를 베끼는 dispatch 표를 두지 않아도 된다.
    UParticleModule* CloneParticleModule(UParticleModule* Source, UObject* Outer)
    {
        if (!Source) return nullptr;

        const FString ClassName = FString(Source->GetClass()->GetName());
        UObject* Created = FObjectFactory::Get().Create(ClassName, Outer);
        UParticleModule* Copy = Cast<UParticleModule>(Created);
        if (!Copy)
        {
            if (Created) UObjectManager::Get().DestroyObject(Created);
            return nullptr;
        }

        FMemoryArchive Saver(true /*save*/);
        Source->Serialize(Saver);
        FMemoryArchive Loader(Saver.GetBuffer(), false /*load*/);
        Copy->Serialize(Loader);
        return Copy;
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

    // Content/Editor/ToolIcons/<filename> 의 아이콘을 캐시된 SRV 로 가져온다.
    // FEditorTextureManager가 내부 캐시를 관리해서 중복 로드는 발생하지 않음.
    ID3D11ShaderResourceView* LoadToolIcon(const wchar_t* FileName)
    {
        const std::wstring Wide = FPaths::Combine(FPaths::AssetDir(), L"Editor/ToolIcons/") + FileName;
        return FEditorTextureManager::Get().GetOrLoadIcon(FPaths::ToUtf8(Wide));
    }

    FString MakeMaterialGuid()
    {
        std::random_device Rd;
        std::mt19937_64    Gen(Rd());
        const uint64       A = Gen();
        const uint64       B = static_cast<uint64>(std::chrono::steady_clock::now().time_since_epoch().count());
        char               Buffer[40];
        std::snprintf(
            Buffer,
            sizeof(Buffer),
            "%016llX%016llX",
            static_cast<unsigned long long>(A),
            static_cast<unsigned long long>(B)
        );
        return Buffer;
    }

    FString SanitizeFileStem(FString Stem)
    {
        if (Stem.empty())
        {
            return "Material";
        }

        for (char& Ch : Stem)
        {
            const bool bAlphaNum = (Ch >= '0' && Ch <= '9') || (Ch >= 'A' && Ch <= 'Z') || (Ch >= 'a' && Ch <= 'z');
            if (!bAlphaNum && Ch != '_' && Ch != '-')
            {
                Ch = '_';
            }
        }
        return Stem;
    }

    std::filesystem::path ToProjectPath(const FString& Path)
    {
        std::filesystem::path Result(FPaths::ToWide(Path));
        if (Result.is_relative())
        {
            Result = std::filesystem::path(FPaths::RootDir()) / Result;
        }
        return Result.lexically_normal();
    }

    std::filesystem::path BuildUniqueMaterialPath(const std::filesystem::path& Directory, const FString& Stem)
    {
        int32 Suffix = 0;
        for (;;)
        {
            FString CandidateStem = Stem;
            if (Suffix > 0)
            {
                CandidateStem += "_";
                CandidateStem += std::to_string(Suffix);
            }

            std::filesystem::path Candidate = Directory / (FPaths::ToWide(CandidateStem) + L".mat");
            if (!std::filesystem::exists(Candidate))
            {
                return Candidate;
            }
            ++Suffix;
        }
    }

    // 아이콘 버튼 + 텍스트 폴백 + 비활성 상태 시각화 + 툴팁을 한 번에 처리.
    // 클릭 시 true 반환. 비활성 상태에서는 표시만 흐려지고 클릭은 무시됨.
    bool IconToolButton(
        const char*               Id,
        ID3D11ShaderResourceView* Icon,
        const char*               FallbackLabel,
        const char*               Tooltip,
        bool                      bEnabled,
        float                     IconSize)
    {
        if (!bEnabled)
        {
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.4f);
        }
        bool bClicked = false;
        if (Icon)
        {
            bClicked = ImGui::ImageButton(Id, reinterpret_cast<ImTextureID>(Icon), ImVec2(IconSize, IconSize));
        }
        else
        {
            bClicked = ImGui::Button(FallbackLabel, ImVec2(IconSize + 8.0f, IconSize + 8.0f));
        }
        if (!bEnabled)
        {
            ImGui::PopStyleVar();
            bClicked = false;
        }
        if (Tooltip && ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("%s", Tooltip);
        }
        return bClicked;
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
    ImGuiWindowFlags WindowFlags = ImGuiWindowFlags_MenuBar |
                                   ImGuiWindowFlags_NoScrollbar |
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

    // 시스템에 sub-LOD가 이미 있다면, 새 이미터에도 같은 수의 LOD를 만들어준다 — 각 추가 LOD는
    // 직전 LOD의 모듈 포인터를 그대로 공유 (Cascade 규약).
    {
        SyncParticleSystemLODDistances(ParticleSystem);
        const int32 TargetLODCount = GetParticleSystemLODCount(ParticleSystem);
        while (static_cast<int32>(NewEmitter->GetLODLevels().size()) < TargetLODCount)
        {
            UParticleLODLevel* Last = NewEmitter->GetLODLevels().back();
            if (!Last) break;

            UParticleLODLevel* NewLOD = UObjectManager::Get().CreateObject<UParticleLODLevel>(NewEmitter);
            NewLOD->Level          = static_cast<int32>(NewEmitter->GetLODLevels().size());
            NewLOD->bEnabled       = true;
            NewLOD->RequiredModule = Last->RequiredModule;
            NewLOD->SpawnModule    = Last->SpawnModule;
            NewLOD->TypeDataModule = Last->TypeDataModule;
            NewLOD->Modules        = Last->Modules;
            NewEmitter->GetLODLevels().push_back(NewLOD);
            NewLOD->UpdateModuleLists();
        }
    }

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
                N->StartVelocity = X->StartVelocity;
                N->StartVelocityRadial = X->StartVelocityRadial;
                NewModule = N;
            }
            else if (auto* X = Cast<UParticleModuleSize>(M))
            {
                auto* N = UObjectManager::Get().CreateObject<UParticleModuleSize>(DstLOD);
                N->StartSize = X->StartSize;
                NewModule = N;
            }
            else if (auto* X = Cast<UParticleModuleColorOverLife>(M))
            {
                auto* N = UObjectManager::Get().CreateObject<UParticleModuleColorOverLife>(DstLOD);
                N->ColorOverLife = X->ColorOverLife;
                N->AlphaOverLife = X->AlphaOverLife;
                N->bClampAlpha = X->bClampAlpha;
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

    // Cascade 규약: 모듈 추가/삭제는 LOD 0(highest)에서만 가능. sub-LOD에서는 구조 변경 금지.
    if (SelectedLODIndex != 0) return;

    UParticleEmitter*  Emitter = ParticleSystem->GetEmitters()[SelectedEmitterIndex];
    if (!Emitter) return;
    UParticleLODLevel* LOD0    = Emitter->GetLODLevel(0);
    if (!LOD0) return;

    TArray<FEmitterModuleEntry> ModuleList;
    BuildEmitterModuleListAt(Emitter, 0, ModuleList);

    if (SelectedModuleIndex >= static_cast<int32>(ModuleList.size())) return;

    UParticleModule* Target = ModuleList[SelectedModuleIndex].Module;
    if (!Target) return;

    // Required/Spawn/TypeData는 슬롯 자체를 비우면 시뮬레이션이 깨진다. 삭제 금지.
    if (Target == LOD0->RequiredModule) return;
    if (Target == LOD0->SpawnModule)    return;
    if (Target == static_cast<UParticleModule*>(LOD0->TypeDataModule)) return;

    // LOD 0 에서 모듈 위치(인덱스)를 찾는다 — sub-LOD에서도 같은 위치를 제거한다.
    auto It = std::find(LOD0->Modules.begin(), LOD0->Modules.end(), Target);
    if (It == LOD0->Modules.end()) return;
    const int32 ArrayIndex = static_cast<int32>(std::distance(LOD0->Modules.begin(), It));

    // 모든 LOD에서 같은 위치의 모듈을 수거: unique 한 것만 destroy.
    const int32 LODCount = static_cast<int32>(Emitter->GetLODLevels().size());
    TArray<UParticleModule*> ToDestroy;

    for (int32 L = 0; L < LODCount; ++L)
    {
        UParticleLODLevel* LOD = Emitter->GetLODLevel(L);
        if (!LOD || ArrayIndex >= static_cast<int32>(LOD->Modules.size())) continue;
        UParticleModule* M = LOD->Modules[ArrayIndex];
        if (M && std::find(ToDestroy.begin(), ToDestroy.end(), M) == ToDestroy.end())
        {
            ToDestroy.push_back(M);
        }
        LOD->Modules.erase(LOD->Modules.begin() + ArrayIndex);
        LOD->UpdateModuleLists();
    }
    for (UParticleModule* M : ToDestroy)
    {
        UObjectManager::Get().DestroyObject(M);
    }

    SelectedModuleIndex = -1;

    MarkDirty();
    RestartPreviewSimulation();
}

// ── LOD 관리 ───────────────────────────────────────────────────────────────
void FParticleSystemEditorWidget::SelectLOD(int32 LODIndex)
{
    SelectedLODIndex   = (std::max)(0, LODIndex);
    SelectedModuleIndex = -1; // sub-LOD 전환 시 모듈 선택은 리셋 (구조가 다를 수 있음).
}

void FParticleSystemEditorWidget::AddLODAfterSelected()
{
    UParticleSystem* PS = GetParticleSystem();
    if (!PS) return;
    SyncParticleSystemLODDistances(PS);

    // 시스템 단위 LODDistances 에 항목 추가 — 인덱스가 LOD 레벨과 1:1로 대응한다.
    const int32 InsertLODIndex = SelectedLODIndex + 1;
    const float PrevDist = InsertLODIndex > 0 && InsertLODIndex - 1 < static_cast<int32>(PS->LODDistances.size())
        ? PS->LODDistances[InsertLODIndex - 1] : 0.0f;
    const float NextDist = InsertLODIndex < static_cast<int32>(PS->LODDistances.size())
        ? PS->LODDistances[InsertLODIndex] : (PrevDist > 0.0f ? PrevDist * 2.0f : 1000.0f);
    const float DefaultDist = NextDist > PrevDist ? (PrevDist + NextDist) * 0.5f : (PrevDist > 0.0f ? PrevDist * 2.0f : 1000.0f);
    if (InsertLODIndex >= static_cast<int32>(PS->LODDistances.size()))
    {
        PS->LODDistances.push_back(DefaultDist);
    }
    else
    {
        PS->LODDistances.insert(PS->LODDistances.begin() + InsertLODIndex, DefaultDist);
    }

    // 각 이미터에 새 LODLevel 을 동일 InsertLODIndex 위치에 끼워넣는다.
    // 기본 정책: 새 LOD 의 모든 모듈 슬롯은 직전 LOD(SelectedLODIndex)의 같은 위치 포인터를
    // 그대로 공유 — Cascade와 동일한 "shared by default" 동작.
    for (UParticleEmitter* Emitter : PS->GetEmitters())
    {
        if (!Emitter) continue;
        UParticleLODLevel* Src = Emitter->GetLODLevel(SelectedLODIndex);
        if (!Src) continue;

        UParticleLODLevel* New = UObjectManager::Get().CreateObject<UParticleLODLevel>(Emitter);
        New->Level          = InsertLODIndex;
        New->bEnabled       = true;
        New->RequiredModule = Src->RequiredModule;
        New->SpawnModule    = Src->SpawnModule;
        New->TypeDataModule = Src->TypeDataModule;
        New->Modules        = Src->Modules; // pointer copy = sharing

        auto& LODs = Emitter->GetLODLevels();
        if (InsertLODIndex >= static_cast<int32>(LODs.size()))
            LODs.push_back(New);
        else
            LODs.insert(LODs.begin() + InsertLODIndex, New);

        // Level 인덱스 재정렬.
        for (int32 i = 0; i < static_cast<int32>(LODs.size()); ++i)
        {
            if (LODs[i]) LODs[i]->Level = i;
        }
        New->UpdateModuleLists();
    }

    SelectLOD(InsertLODIndex);
    MarkDirty();
    RestartPreviewSimulation();
}

void FParticleSystemEditorWidget::RemoveLODAt(int32 LODIndex)
{
    if (LODIndex <= 0) return; // LOD 0 은 삭제 불가.
    UParticleSystem* PS = GetParticleSystem();
    if (!PS) return;
    SyncParticleSystemLODDistances(PS);

    // 우선 어떤 모듈을 "이 LOD가 유일하게 보유" 하는지 모든 emitter 에 걸쳐 미리 수집.
    // sharing 관계를 잃지 않으려면, 다른 어떤 LOD 슬롯에서도 참조되지 않는 포인터만 destroy.
    auto IsReferencedElsewhere = [&](UParticleEmitter* E, UParticleModule* M, int32 ExcludeLOD) -> bool
    {
        if (!M || !E) return false;
        const int32 LCount = static_cast<int32>(E->GetLODLevels().size());
        for (int32 i = 0; i < LCount; ++i)
        {
            if (i == ExcludeLOD) continue;
            UParticleLODLevel* L = E->GetLODLevel(i);
            if (!L) continue;
            if (L->RequiredModule == M) return true;
            if (L->SpawnModule == M)    return true;
            if (static_cast<UParticleModule*>(L->TypeDataModule) == M) return true;
            for (UParticleModule* X : L->Modules) if (X == M) return true;
        }
        return false;
    };

    for (UParticleEmitter* Emitter : PS->GetEmitters())
    {
        if (!Emitter) continue;
        auto& LODs = Emitter->GetLODLevels();
        if (LODIndex >= static_cast<int32>(LODs.size())) continue;

        UParticleLODLevel* Removed = LODs[LODIndex];
        if (Removed)
        {
            TArray<UParticleModule*> ToDestroy;
            auto Push = [&](UParticleModule* M)
            {
                if (!M) return;
                if (IsReferencedElsewhere(Emitter, M, LODIndex)) return;
                if (std::find(ToDestroy.begin(), ToDestroy.end(), M) != ToDestroy.end()) return;
                ToDestroy.push_back(M);
            };
            Push(Removed->RequiredModule);
            Push(Removed->SpawnModule);
            Push(static_cast<UParticleModule*>(Removed->TypeDataModule));
            for (UParticleModule* M : Removed->Modules) Push(M);

            for (UParticleModule* M : ToDestroy) UObjectManager::Get().DestroyObject(M);
            UObjectManager::Get().DestroyObject(Removed);
        }
        LODs.erase(LODs.begin() + LODIndex);

        for (int32 i = 0; i < static_cast<int32>(LODs.size()); ++i)
        {
            if (LODs[i]) LODs[i]->Level = i;
        }
    }

    if (LODIndex < static_cast<int32>(PS->LODDistances.size()))
    {
        PS->LODDistances.erase(PS->LODDistances.begin() + LODIndex);
    }

    SelectLOD((std::max)(0, LODIndex - 1));
    MarkDirty();
    RestartPreviewSimulation();
}

// ── 모듈 sharing 관리 ──────────────────────────────────────────────────────
void FParticleSystemEditorWidget::DuplicateModuleFromHigherLOD(UParticleEmitter* Emitter, int32 LODIndex, int32 ModuleIndex)
{
    if (!Emitter || LODIndex <= 0 || ModuleIndex < 0) return;
    UParticleLODLevel* Cur = Emitter->GetLODLevel(LODIndex);
    UParticleLODLevel* Hi  = Emitter->GetLODLevel(LODIndex - 1);
    if (!Cur || !Hi) return;

    TArray<FEmitterModuleEntry> CurList, HiList;
    BuildEmitterModuleListAt(Emitter, LODIndex,     CurList);
    BuildEmitterModuleListAt(Emitter, LODIndex - 1, HiList);
    if (ModuleIndex >= static_cast<int32>(HiList.size())) return;

    UParticleModule* Source = HiList[ModuleIndex].Module;
    UParticleModule* Clone  = CloneParticleModule(Source, Cur);
    if (!Clone) return;

    UParticleModule* Old = ModuleIndex < static_cast<int32>(CurList.size()) ? CurList[ModuleIndex].Module : nullptr;

    // 슬롯 결정 — Required/Spawn/TypeData/Modules[i]
    if (Cur->RequiredModule == Old)
    {
        if (auto* R = Cast<UParticleModuleRequired>(Clone)) Cur->RequiredModule = R;
    }
    else if (Cur->SpawnModule == Old)
    {
        if (auto* S = Cast<UParticleModuleSpawn>(Clone)) Cur->SpawnModule = S;
    }
    else if (static_cast<UParticleModule*>(Cur->TypeDataModule) == Old)
    {
        if (auto* T = Cast<UParticleModuleTypeDataBase>(Clone)) Cur->TypeDataModule = T;
    }
    else
    {
        auto It = std::find(Cur->Modules.begin(), Cur->Modules.end(), Old);
        if (It != Cur->Modules.end()) *It = Clone;
        else                          Cur->Modules.push_back(Clone);
    }
    Cur->UpdateModuleLists();

    MarkDirty();
    RestartPreviewSimulation();
}

void FParticleSystemEditorWidget::ShareModuleFromHigherLOD(UParticleEmitter* Emitter, int32 LODIndex, int32 ModuleIndex)
{
    if (!Emitter || LODIndex <= 0 || ModuleIndex < 0) return;
    UParticleLODLevel* Cur = Emitter->GetLODLevel(LODIndex);
    UParticleLODLevel* Hi  = Emitter->GetLODLevel(LODIndex - 1);
    if (!Cur || !Hi) return;

    TArray<FEmitterModuleEntry> CurList, HiList;
    BuildEmitterModuleListAt(Emitter, LODIndex,     CurList);
    BuildEmitterModuleListAt(Emitter, LODIndex - 1, HiList);
    if (ModuleIndex >= static_cast<int32>(CurList.size())) return;
    if (ModuleIndex >= static_cast<int32>(HiList.size())) return;

    UParticleModule* OldUnique = CurList[ModuleIndex].Module;
    UParticleModule* ShareSrc  = HiList[ModuleIndex].Module;
    if (OldUnique == ShareSrc) return; // 이미 공유 중.

    // 포인터 교체.
    if (Cur->RequiredModule == OldUnique)
    {
        if (auto* R = Cast<UParticleModuleRequired>(ShareSrc)) Cur->RequiredModule = R;
    }
    else if (Cur->SpawnModule == OldUnique)
    {
        if (auto* S = Cast<UParticleModuleSpawn>(ShareSrc)) Cur->SpawnModule = S;
    }
    else if (static_cast<UParticleModule*>(Cur->TypeDataModule) == OldUnique)
    {
        if (auto* T = Cast<UParticleModuleTypeDataBase>(ShareSrc)) Cur->TypeDataModule = T;
    }
    else
    {
        auto It = std::find(Cur->Modules.begin(), Cur->Modules.end(), OldUnique);
        if (It != Cur->Modules.end()) *It = ShareSrc;
    }
    Cur->UpdateModuleLists();

    // OldUnique 가 다른 LOD에서도 참조되지 않으면 destroy.
    bool bReferenced = false;
    const int32 LCount = static_cast<int32>(Emitter->GetLODLevels().size());
    for (int32 i = 0; i < LCount && !bReferenced; ++i)
    {
        UParticleLODLevel* L = Emitter->GetLODLevel(i);
        if (!L) continue;
        if (L->RequiredModule == OldUnique || L->SpawnModule == OldUnique ||
            static_cast<UParticleModule*>(L->TypeDataModule) == OldUnique)
        {
            bReferenced = true; break;
        }
        for (UParticleModule* M : L->Modules) if (M == OldUnique) { bReferenced = true; break; }
    }
    if (!bReferenced && OldUnique) UObjectManager::Get().DestroyObject(OldUnique);

    MarkDirty();
    RestartPreviewSimulation();
}

void FParticleSystemEditorWidget::DuplicateModuleFromHighestLOD(UParticleEmitter* Emitter, int32 LODIndex, int32 ModuleIndex)
{
    if (!Emitter || LODIndex <= 0 || ModuleIndex < 0) return;
    UParticleLODLevel* Cur = Emitter->GetLODLevel(LODIndex);
    UParticleLODLevel* Top = Emitter->GetLODLevel(0);
    if (!Cur || !Top) return;

    TArray<FEmitterModuleEntry> CurList, TopList;
    BuildEmitterModuleListAt(Emitter, LODIndex, CurList);
    BuildEmitterModuleListAt(Emitter, 0,        TopList);
    if (ModuleIndex >= static_cast<int32>(TopList.size())) return;

    UParticleModule* Source = TopList[ModuleIndex].Module;
    UParticleModule* Clone  = CloneParticleModule(Source, Cur);
    if (!Clone) return;

    UParticleModule* Old = ModuleIndex < static_cast<int32>(CurList.size()) ? CurList[ModuleIndex].Module : nullptr;

    if (Cur->RequiredModule == Old)
    {
        if (auto* R = Cast<UParticleModuleRequired>(Clone)) Cur->RequiredModule = R;
    }
    else if (Cur->SpawnModule == Old)
    {
        if (auto* S = Cast<UParticleModuleSpawn>(Clone)) Cur->SpawnModule = S;
    }
    else if (static_cast<UParticleModule*>(Cur->TypeDataModule) == Old)
    {
        if (auto* T = Cast<UParticleModuleTypeDataBase>(Clone)) Cur->TypeDataModule = T;
    }
    else
    {
        auto It = std::find(Cur->Modules.begin(), Cur->Modules.end(), Old);
        if (It != Cur->Modules.end()) *It = Clone;
        else                          Cur->Modules.push_back(Clone);
    }
    Cur->UpdateModuleLists();

    MarkDirty();
    RestartPreviewSimulation();
}

void FParticleSystemEditorWidget::SyncEmitterUIState()
{
    UParticleSystem* ParticleSystem = GetParticleSystem();
    SyncParticleSystemLODDistances(ParticleSystem);

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

void FParticleSystemEditorWidget::OpenMaterialForRequired(UParticleModuleRequired* Required)
{
    if (!Required || !EditorEngine)
    {
        return;
    }

    Required->ResolveMaterialFromSlot();
    UMaterial* Material = Required->Material;
    if (!Material && !Required->MaterialSlot.IsNull() && Required->MaterialSlot != "None")
    {
        Material = FMaterialManager::Get().GetOrCreateMaterial(Required->MaterialSlot.ToString());
    }

    if (Material)
    {
        EditorEngine->OpenAssetEditorForObject(Material);
    }
}

void FParticleSystemEditorWidget::DuplicateMaterialForRequired(UParticleModuleRequired* Required)
{
    if (!Required)
    {
        return;
    }

    FString       CreatedPath;
    const FString SourceSlot    = Required->MaterialSlot.ToString();
    const bool    bHasSource    = !Required->MaterialSlot.IsNull() && !SourceSlot.empty() && SourceSlot != "None";
    const FString EmitterSuffix = SelectedEmitterIndex >= 0 ? FString("Emitter") + std::to_string(SelectedEmitterIndex)
    : FString("Emitter");

    if (bHasSource)
    {
        const std::filesystem::path SourcePath = ToProjectPath(SourceSlot);
        if (std::filesystem::exists(SourcePath))
        {
            const FString               SourceStem = FPaths::ToUtf8(SourcePath.stem().wstring());
            const FString               TargetStem = SanitizeFileStem(SourceStem + "_" + EmitterSuffix);
            const std::filesystem::path TargetPath = BuildUniqueMaterialPath(SourcePath.parent_path(), TargetStem);
            const FString               ProjectRelativePath = FPaths::ToUtf8(
                TargetPath.lexically_relative(FPaths::RootDir()).generic_wstring()
            );
            const FString NewGuid = MakeMaterialGuid();

            std::ifstream InFile(SourcePath);
            FString       Content((std::istreambuf_iterator<char>(InFile)), std::istreambuf_iterator<char>());
            json::JSON    Root = json::JSON::Load(Content);
            if (Root.IsNull() || !Root.hasKey(MatKeys::Graph))
            {
                Root = MaterialGraphAsset::MakeDefaultMaterialJson(ProjectRelativePath, NewGuid);
            }
            else
            {
                Root[MatKeys::Version]                       = 2;
                Root[MatKeys::MaterialGuid]                  = NewGuid;
                Root[MatKeys::PathFileName]                  = ProjectRelativePath;
                Root[MatKeys::GeneratedShaderPath]           = "";
                Root[MatKeys::ShaderPath]                    = "";
                Root[MatKeys::Compiled]                      = json::Object();
                Root[MatKeys::Compiled][MatKeys::GraphHash]  = "";
                Root[MatKeys::Compiled][MatKeys::Parameters] = json::Object();
                Root[MatKeys::Compiled][MatKeys::Textures]   = json::Object();
            }

            std::ofstream OutFile(TargetPath);
            if (OutFile.is_open())
            {
                OutFile << Root.dump();
                CreatedPath = ProjectRelativePath;
            }
        }
    }

    if (CreatedPath.empty())
    {
        const std::wstring MaterialDir = FPaths::Combine(FPaths::AssetDir(), L"Material");
        FPaths::CreateDir(MaterialDir);
        const FString AssetName = SanitizeFileStem(FString("Material_") + EmitterSuffix);
        if (!FAssetFactory::CreateMaterial(FPaths::ToUtf8(MaterialDir), AssetName, CreatedPath))
        {
            return;
        }
        CreatedPath = FPaths::MakeProjectRelative(CreatedPath);
    }

    Required->MaterialSlot = CreatedPath;
    Required->ResolveMaterialFromSlot();
    FMaterialManager::Get().ScanMaterialAssets();

    MarkDirty();
    RestartPreviewSimulation();

    if (EditorEngine)
    {
        EditorEngine->RefreshContentBrowser();
        if (UMaterial* Material = FMaterialManager::Get().GetOrCreateMaterial(CreatedPath))
        {
            EditorEngine->OpenAssetEditorForObject(Material);
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
        if (ImGui::MenuItem("Save", "Ctrl+S", false, IsDirty())) SaveAsset();
        ImGui::MenuItem("Save As...", nullptr, false, false);
        ImGui::Separator();
        if (ImGui::MenuItem("Close")) bPendingClose = true;
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Edit"))
    {
        ImGui::MenuItem("Undo", "Ctrl+Z", false, false);
        ImGui::MenuItem("Redo", "Ctrl+Y", false, false);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Asset"))
    {
        ImGui::MenuItem("Find in Content Browser", nullptr, false, false);
        ImGui::MenuItem("Reimport", nullptr, false, false);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Window"))
    {
        ImGui::MenuItem("Preview",      nullptr, true, false);
        ImGui::MenuItem("Emitters",     nullptr, true, false);
        ImGui::MenuItem("Details",      nullptr, true, false);
        ImGui::MenuItem("Curve Editor", nullptr, true, false);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Help"))
    {
        ImGui::MenuItem("Documentation", nullptr, false, false);
        ImGui::EndMenu();
    }

    ImGui::EndMenuBar();
}

// ── 툴바 (Content/Editor/ToolIcons/ 아이콘 사용, 가로 스크롤 지원) ─────────
void FParticleSystemEditorWidget::RenderToolbar()
{
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 4.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(3.0f, 3.0f));
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 1.0f, 1.0f, 0.12f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(1.0f, 1.0f, 1.0f, 0.22f));

    constexpr float IconSize  = 28.0f;
    // 높이 = WindowPadding y*2 + (icon + FramePadding y*2) + 가로 스크롤바.
    //      = 2*2 + (28 + 2*4) + 14 = 54px.
    constexpr float ToolbarH  = 54.0f;

    // child 내부의 기본 WindowPadding(8,8)이 그대로면 세로 스크롤바가 항상 생긴다.
    // 가로 스크롤바만 필요할 때만 보이도록 padding을 최소화.
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(2.0f, 2.0f));
    if (ImGui::BeginChild("##PSEToolbar", ImVec2(0.0f, ToolbarH), false,
                          ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoBackground))
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
        if (IconToolButton("##Save",
                           LoadToolIcon(L"SaveCurrent.png"),
                           "Save", "Save the particle system (Ctrl+S)", IsDirty(), IconSize))
        {
            SaveAsset();
        }
        ImGui::SameLine();
        IconToolButton("##FindCB",
                       LoadToolIcon(L"ContentBrowser.png"),
                       "Find", "Find this asset in the Content Browser (not implemented)", false, IconSize);
        ImGui::SameLine();
        Group();

        // 그룹 2: 시뮬레이션.
        if (IconToolButton("##RestartSim",
                           LoadToolIcon(L"icon_Cascade_RestartSim_40x.png"),
                           "RSim", "Restart the preview simulation", true, IconSize))
        {
            RestartPreviewSimulation();
        }
        ImGui::SameLine();
        if (IconToolButton("##RestartLvl",
                           LoadToolIcon(L"icon_Cascade_RestartInLevel_40x.png"),
                           "RLvl",
                           "Restart all level instances of this particle system\n"
                           "(re-runs ResetSystem on every UParticleSystemComponent referencing this asset)",
                           GetParticleSystem() != nullptr, IconSize))
        {
            RefreshExternalComponents(GetParticleSystem());
            RestartPreviewSimulation();
        }
        ImGui::SameLine();
        Group();

        // 그룹 3: 편집 이력.
        IconToolButton("##Undo",
                       LoadToolIcon(L"icon_Generic_Undo_40x.png"),
                       "Undo", "Undo (not implemented — no transaction system yet)", false, IconSize);
        ImGui::SameLine();
        IconToolButton("##Redo",
                       LoadToolIcon(L"icon_Generic_Redo_40x.png"),
                       "Redo", "Redo (not implemented — no transaction system yet)", false, IconSize);
        ImGui::SameLine();
        Group();

        // 그룹 4: 뷰포트 옵션.
        IconToolButton("##Thumb",
                       LoadToolIcon(L"icon_Cascade_Thumbnail_40x.png"),
                       "Thmb", "Capture thumbnail from preview (not implemented)", false, IconSize);
        ImGui::SameLine();
        IconToolButton("##Bounds",
                       LoadToolIcon(L"icon_Cascade_Bounds_40x.png"),
                       "Bnds", "Toggle bounds display (not implemented)", false, IconSize);
        ImGui::SameLine();
        IconToolButton("##Axis",
                       LoadToolIcon(L"icon_Cascade_Axis_40x.png"),
                       "Axis", "Toggle origin axis display (not implemented)", false, IconSize);
        ImGui::SameLine();
        IconToolButton("##BG",
                       LoadToolIcon(L"icon_Cascade_Color_40x.png"),
                       "BG", "Set preview background color (not implemented)", false, IconSize);
        ImGui::SameLine();
        Group();

        // 그룹 5: LOD. RegenLOD 만 미구현, 나머지는 모두 작동.
        const int32 PSLODCount = GetParticleSystemLODCount(GetParticleSystem());

        IconToolButton("##RegenLOD1",
                       LoadToolIcon(L"icon_Cascade_RegenLOD1_40x.png"),
                       "RL1", "Regenerate lowest LOD from highest (not implemented)", false, IconSize);
        ImGui::SameLine();
        IconToolButton("##RegenLOD2",
                       LoadToolIcon(L"icon_Cascade_RegenLOD2_40x.png"),
                       "RL2", "Regenerate highest LOD from lowest (not implemented)", false, IconSize);
        ImGui::SameLine();
        if (IconToolButton("##LowestLOD",
                           LoadToolIcon(L"icon_Cascade_LowestLOD_40x.png"),
                           "LowL", "Jump to lowest LOD",
                           SelectedLODIndex < PSLODCount - 1, IconSize))
        {
            SelectLOD(PSLODCount - 1);
        }
        ImGui::SameLine();
        if (IconToolButton("##LowerLOD",
                           LoadToolIcon(L"icon_Cascade_LowerLOD_40x.png"),
                           "Lwr", "Jump to next lower LOD (higher index)",
                           SelectedLODIndex < PSLODCount - 1, IconSize))
        {
            SelectLOD(SelectedLODIndex + 1);
        }
        ImGui::SameLine();
        if (IconToolButton("##HigherLOD",
                           LoadToolIcon(L"icon_Cascade_HigherLOD_40x.png"),
                           "Hgr", "Jump to next higher LOD (lower index)",
                           SelectedLODIndex > 0, IconSize))
        {
            SelectLOD(SelectedLODIndex - 1);
        }
        ImGui::SameLine();
        if (IconToolButton("##HighestLOD",
                           LoadToolIcon(L"icon_Cascade_HighestLOD_40x.png"),
                           "HghL", "Jump to highest LOD (LOD 0)",
                           SelectedLODIndex > 0, IconSize))
        {
            SelectLOD(0);
        }
        ImGui::SameLine();
        if (IconToolButton("##AddLOD1",
                           LoadToolIcon(L"icon_Cascade_AddLOD1_40x.png"),
                           "+L1", "Add LOD before the current (= insert at current index)",
                           GetParticleSystem() != nullptr, IconSize))
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
        if (IconToolButton("##AddLOD2",
                           LoadToolIcon(L"icon_Cascade_AddLOD2_40x.png"),
                           "+L2", "Add LOD after the current",
                           GetParticleSystem() != nullptr, IconSize))
        {
            AddLODAfterSelected();
        }
        ImGui::SameLine();
        if (IconToolButton("##DeleteLOD",
                           LoadToolIcon(L"icon_Cascade_DeleteLOD_40x.png"),
                           "-LOD", "Delete current LOD (LOD 0 cannot be removed)",
                           SelectedLODIndex > 0, IconSize))
        {
            RemoveLODAt(SelectedLODIndex);
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleVar(); // WindowPadding

    ImGui::PopStyleColor(3);
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
            BuildEmitterModuleListAt(StatusEmitter, SelectedLODIndex, ModuleList);

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
        // 레퍼런스 Cascade의 View/Time 탭. 현재는 시각용 placeholder.
        if (ImGui::BeginTabBar("##PSEViewportTabs", ImGuiTabBarFlags_None))
        {
            if (ImGui::BeginTabItem("View")) { ImGui::EndTabItem(); }
            if (ImGui::BeginTabItem("Time")) { ImGui::EndTabItem(); }
            ImGui::EndTabBar();
        }

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

    // 시스템 단위 LOD 카운트 — LODDistances와 LODLevels가 같은 인덱스를 공유한다.
    const int32 LODCount = GetParticleSystemLODCount(ParticleSystem);

    // SelectedLODIndex 범위 검증.
    if (SelectedLODIndex < 0)         SelectedLODIndex = 0;
    if (SelectedLODIndex >= LODCount) SelectedLODIndex = LODCount - 1;

    char Context[64];
    std::snprintf(Context, sizeof(Context), "LOD %d / %d  ·  %d emitter%s",
        SelectedLODIndex, LODCount - 1, EmitterCount, EmitterCount == 1 ? "" : "s");

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

                    // Mesh emitter 표시 — 레퍼런스 Cascade의 "Mesh Data" 헤더와 동일한 위치.
                    if (Emitter && Emitter->bUseMeshInstance)
                    {
                        ImDrawList*  DL  = ImGui::GetWindowDrawList();
                        const ImVec2 Pos = ImGui::GetCursorScreenPos();
                        const float  W   = ImGui::GetContentRegionAvail().x;
                        const float  H   = ImGui::GetTextLineHeight() + 4.0f;
                        DL->AddRectFilled(Pos, ImVec2(Pos.x + W, Pos.y + H), IM_COL32(60, 70, 90, 255), 3.0f);
                        DL->AddText(ImVec2(Pos.x + 6.0f, Pos.y + 2.0f), PSE::HeaderText, "Mesh Data");
                        ImGui::Dummy(ImVec2(W, H + 2.0f));
                    }

                    ImGui::Separator();

                    TArray<FEmitterModuleEntry> ModuleList;
                    BuildEmitterModuleListAt(Emitter, SelectedLODIndex, ModuleList);

                    int32 ModuleToDelete           = -1;
                    int32 ModuleToDuplicateHigher  = -1;
                    int32 ModuleToShareHigher      = -1;
                    int32 ModuleToDuplicateHighest = -1;
                    int32 ModuleToRefresh          = -1;
                    for (int32 ModuleIndex = 0; ModuleIndex < static_cast<int32>(ModuleList.size()); ++ModuleIndex)
                    {
                        ImGui::PushID(ModuleIndex);
                        const FEmitterModuleEntry& Entry     = ModuleList[ModuleIndex];
                        const bool                 bSelected = bEmitterSelected && (SelectedModuleIndex == ModuleIndex);

                        UParticleLODLevel* SelLOD = Emitter ? Emitter->GetLODLevel(SelectedLODIndex) : nullptr;
                        const bool bIsCoreSlot = SelLOD && (
                            Entry.Module == SelLOD->RequiredModule ||
                            Entry.Module == SelLOD->SpawnModule ||
                            Entry.Module == static_cast<UParticleModule*>(SelLOD->TypeDataModule));

                        const bool bIsShared = IsModuleSharedWithHigher(Emitter, SelectedLODIndex, ModuleIndex);

                        // 좌측 enable 토글 아이콘 — sub-LOD에서 공유 중이면 회색.
                        ImDrawList*  DL  = ImGui::GetWindowDrawList();
                        const ImVec2 IconPos = ImGui::GetCursorScreenPos();
                        constexpr float IconSize = 14.0f;
                        const bool bModEnabled = Entry.Module && (Entry.Module->bEnabled != 0);
                        const ImU32 IconCol = (bIsCoreSlot || bIsShared)
                            ? IM_COL32(110, 115, 125, 200)
                            : (bModEnabled ? PSE::Accent : IM_COL32(80, 84, 92, 255));
                        DL->AddRectFilled(IconPos,
                            ImVec2(IconPos.x + IconSize, IconPos.y + IconSize),
                            IconCol, 2.0f);
                        if (bModEnabled)
                        {
                            DL->AddLine(
                                ImVec2(IconPos.x + 3.0f, IconPos.y + IconSize * 0.55f),
                                ImVec2(IconPos.x + IconSize * 0.42f, IconPos.y + IconSize - 3.0f),
                                IM_COL32(255, 255, 255, 230), 1.6f);
                            DL->AddLine(
                                ImVec2(IconPos.x + IconSize * 0.42f, IconPos.y + IconSize - 3.0f),
                                ImVec2(IconPos.x + IconSize - 2.0f, IconPos.y + 3.0f),
                                IM_COL32(255, 255, 255, 230), 1.6f);
                        }
                        ImGui::InvisibleButton("##enableicon", ImVec2(IconSize, IconSize));
                        if (!bIsCoreSlot && !bIsShared && ImGui::IsItemClicked() && Entry.Module)
                        {
                            Entry.Module->bEnabled = bModEnabled ? 0 : 1;
                            MarkDirty();
                            RestartPreviewSimulation();
                        }
                        if (ImGui::IsItemHovered())
                        {
                            ImGui::SetTooltip(bIsCoreSlot
                                ? "Required/Spawn/TypeData are always enabled"
                                : (bIsShared ? "Shared from higher LOD — duplicate first to edit"
                                             : (bModEnabled ? "Disable module" : "Enable module")));
                        }
                        ImGui::SameLine(0.0f, 4.0f);

                        // 공유 중인 모듈은 dim text로 표시 — 편집 불가 시각 신호.
                        if (bIsShared) ImGui::PushStyleColor(ImGuiCol_Text, PSE::DimTextV);
                        if (ImGui::Selectable(Entry.Name, bSelected))
                        {
                            SelectEmitter(EmitterIndex, ModuleIndex);
                        }
                        if (bIsShared) ImGui::PopStyleColor();

                        // 우측 curve placeholder 아이콘.
                        const ImVec2 RowMin = ImGui::GetItemRectMin();
                        const ImVec2 RowMax = ImGui::GetItemRectMax();
                        const ImVec2 CurveIconMin(RowMax.x - IconSize - 2.0f, RowMin.y + 1.0f);
                        const ImVec2 CurveIconMax(RowMax.x - 2.0f, RowMin.y + 1.0f + IconSize);
                        DL->AddRect(CurveIconMin, CurveIconMax, IM_COL32(90, 95, 105, 200), 2.0f);
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
                                IM_COL32(110, 115, 125, 180), 1.0f);
                        }

                        // ── 모듈 컨텍스트 메뉴 (LOD 인식) ──
                        if (ImGui::BeginPopupContextItem("##ModuleCtx"))
                        {
                            const bool bIsLOD0      = (SelectedLODIndex == 0);
                            const bool bIsRequired  = SelLOD && Entry.Module == SelLOD->RequiredModule;
                            // 모듈 삭제 — LOD 0의 비-core 모듈만 가능 (구조 변경은 LOD 0에서만).
                            if (ImGui::MenuItem("모듈 삭제", "Del", false, bIsLOD0 && !bIsCoreSlot))
                            {
                                SelectEmitter(EmitterIndex, ModuleIndex);
                                ModuleToDelete = ModuleIndex;
                            }
                            if (!bIsLOD0 && ImGui::IsItemHovered())
                                ImGui::SetTooltip("Structural changes only in LOD 0");

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
                                if (Entry.Module) Entry.Module->bEnabled = bModEn ? 1 : 0;
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

                    // + Module 팝업 — LOD0 에서만 노출 (구조 변경은 LOD 0 전용).
                    if (SelectedLODIndex == 0 && ImGui::SmallButton("+ Module"))
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
                                // LOD 0에 새 모듈이 추가됐다면 같은 포인터를 모든 sub-LOD에도 push
                                // (기본 정책: 새 모듈은 sub-LOD에서 자동으로 shared).
                                if (Emitter && LOD0 && !LOD0->Modules.empty())
                                {
                                    UParticleModule* New = LOD0->Modules.back();
                                    const int32 LCount = static_cast<int32>(Emitter->GetLODLevels().size());
                                    for (int32 L = 1; L < LCount; ++L)
                                    {
                                        if (UParticleLODLevel* Sub = Emitter->GetLODLevel(L))
                                        {
                                            Sub->Modules.push_back(New);
                                            Sub->UpdateModuleLists();
                                        }
                                    }
                                }
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
                        AddItem("Color Over Life", [](bool bQuery, UParticleLODLevel* L)
                        {
                            if (bQuery) return HasModuleOfType<UParticleModuleColorOverLife>(L);
                            auto* N = UObjectManager::Get().CreateObject<UParticleModuleColorOverLife>(L);
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
                ImGui::TextWrapped("%s", ParticleSystem && !ParticleSystem->GetSourcePath().empty()
                    ? ParticleSystem->GetSourcePath().c_str() : "(unsaved)");

                ImGui::TextColored(PSE::DimTextV, "Emitters: %d",
                    ParticleSystem ? static_cast<int32>(ParticleSystem->GetEmitters().size()) : 0);
                ImGui::TextColored(PSE::DimTextV, "Status: %s", IsDirty() ? "Modified" : "Saved");

                // 레퍼런스 Cascade에 노출되는 시스템 단위 필드들 — 엔진에 아직 미구현이라 placeholder.
                ImGui::BeginDisabled();
                int   SysUpdateMode    = 0;
                float UpdateTimeFPS    = 60.0f;
                float WarmupTime       = 0.0f;
                float WarmupTickRate   = 0.0f;
                bool  bOrientZ         = false;
                float SecondsInactive  = 0.0f;
                ImGui::Combo("System Update Mode", &SysUpdateMode, "EPSUM_RealTime\0EPSUM_FixedTime\0\0");
                ImGui::DragFloat("Update Time FPS",          &UpdateTimeFPS, 1.0f, 1.0f, 240.0f);
                ImGui::DragFloat("Warmup Time",              &WarmupTime,    0.05f, 0.0f, 1000.0f);
                ImGui::DragFloat("Warmup Tick Rate",         &WarmupTickRate,0.05f, 0.0f, 1000.0f);
                ImGui::Checkbox ("Orient ZAxis Toward Camera", &bOrientZ);
                ImGui::DragFloat("Seconds Before Inactive",  &SecondsInactive,0.1f, 0.0f, 1000.0f);
                ImGui::EndDisabled();
            }

            // ── Thumbnail ──
            if (ImGui::CollapsingHeader("Thumbnail"))
            {
                ImGui::BeginDisabled();
                float ThumbWarmup = 1.0f;
                bool  bRealtime   = false;
                ImGui::DragFloat("Thumbnail Warmup", &ThumbWarmup, 0.1f, 0.0f, 60.0f);
                ImGui::Checkbox ("Use Realtime Thumbnail", &bRealtime);
                ImGui::EndDisabled();
            }

            // ── LOD ──
            ImGui::SetNextItemOpen(true, ImGuiCond_Once);
            if (ImGui::CollapsingHeader("LOD"))
            {
                ImGui::BeginDisabled();
                float CheckTime = 0.25f;
                int   Method    = 0;
                ImGui::DragFloat("LOD Distance Check Time", &CheckTime, 0.05f, 0.0f, 10.0f);
                ImGui::Combo("LOD Method", &Method,
                    "PARTICLESYSTEMLODMETHOD_Automatic\0"
                    "PARTICLESYSTEMLODMETHOD_DirectSet\0"
                    "PARTICLESYSTEMLODMETHOD_ActivateAutomatic\0\0");
                ImGui::EndDisabled();

                // LODDistances는 실제 UPROPERTY → 편집 가능.
                if (ParticleSystem)
                {
                    SyncParticleSystemLODDistances(ParticleSystem);
                    TArray<float>& Dist = ParticleSystem->LODDistances;
                    ImGui::Text("LOD Distances (%d)", static_cast<int32>(Dist.size()));
                    bool bDistChanged = false;
                    int32 RemoveAt = -1;
                    for (int32 i = 0; i < static_cast<int32>(Dist.size()); ++i)
                    {
                        ImGui::PushID(i);
                        char Lbl[24]; std::snprintf(Lbl, sizeof(Lbl), "LOD %d", i);
                        if (i == 0) ImGui::BeginDisabled();
                        float Value = Dist[i];
                        if (ImGui::DragFloat(Lbl, &Value, 1.0f, 0.0f, 100000.0f))
                        {
                            Dist[i] = (std::max)(0.0f, Value);
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
            // 모듈 편집. sub-LOD에서 공유 중인 모듈은 편집 금지 — 안내 + BeginDisabled.
            const bool bIsShared = IsModuleSharedWithHigher(SelectedEmitter, SelectedLODIndex, SelectedModuleIndex);
            if (bIsShared)
            {
                ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.40f, 1.0f),
                    "이 모듈은 LOD %d 와 공유 중 — 편집하려면 우클릭 > '상위에서 복제'를 선택하세요.",
                    SelectedLODIndex - 1);
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
            { ColorOverLife->bClampAlpha = bClamp ? 1 : 0; bChanged = true; }
        }
    }
    else if (UParticleModuleColor* Color = Cast<UParticleModuleColor>(Module))
    {
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        if (ImGui::CollapsingHeader("Color"))
        {
			DrawRawDistributionVector("Start Color", Color->StartColor, bChanged, Color);
			DrawRawDistributionFloat("Start Alpha", Color->StartAlpha, bChanged, Color);
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
    BuildEmitterModuleListAt(SelectedEmitter, SelectedLODIndex, ModuleList);

    const FEmitterModuleEntry* SelectedEntry = nullptr;
    if (SelectedModuleIndex >= 0 && SelectedModuleIndex < static_cast<int32>(ModuleList.size()))
    {
        SelectedEntry = &ModuleList[SelectedModuleIndex];
    }

    const char* Context = SelectedEntry ? SelectedEntry->Name : "no module selected";

    if (BeginPanel("##PSECurveEditor", "Curve Editor", Width, Height, Context))
    {
        // ── 상단 툴바 — Cascade Curve Editor 아이콘 (모두 미구현이라 disabled, 가로 스크롤 지원) ──
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2.0f, 2.0f));
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 1.0f, 1.0f, 0.12f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(1.0f, 1.0f, 1.0f, 0.22f));

        constexpr float CurveIconSize = 22.0f;
        // 높이 = WindowPadding y*2 + (icon + FramePadding y*2) + 가로 스크롤바.
        //      = 2*2 + (22 + 2*4) + 14 = 48px.
        constexpr float CurveToolbarH = 48.0f;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(2.0f, 2.0f));
        if (ImGui::BeginChild("##PSECurveToolbar", ImVec2(0.0f, CurveToolbarH), false,
                              ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoBackground))
        {
            struct FCurveBtn { const wchar_t* Icon; const char* Fallback; const char* Tip; };
            const FCurveBtn ViewBtns[] = {
                { L"icon_CurveEditor_Horizontal_40x.png",  "H",   "Fit horizontal" },
                { L"icon_CurveEditor_Vertical_40x.png",    "V",   "Fit vertical" },
                { L"icon_CurveEditor_ShowAll_40x.png",     "All", "Fit all" },
                { L"icon_CurveEditor_ZoomToFit_40x.png",   "Sel", "Fit selected" },
                { L"icon_CurveEditor_Pan_40x.png",         "Pan", "Pan mode" },
                { L"icon_CurveEditor_Zoom_40x.png",        "Zm",  "Zoom mode" },
            };
            for (const auto& B : ViewBtns)
            {
                IconToolButton(B.Tip, LoadToolIcon(B.Icon), B.Fallback, B.Tip, false, CurveIconSize);
                ImGui::SameLine();
            }

            // 구분자.
            const ImVec2 SepPos = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddLine(
                ImVec2(SepPos.x + 3.0f, SepPos.y + 3.0f),
                ImVec2(SepPos.x + 3.0f, SepPos.y + 25.0f),
                PSE::Border32);
            ImGui::Dummy(ImVec2(7.0f, 0.0f));
            ImGui::SameLine();

            const FCurveBtn TanBtns[] = {
                { L"icon_CurveEditor_Auto_40x.png",        "A",  "Auto tangent" },
                { L"icon_CurveEditor_AutoClamped_40x.png", "AC", "Auto/Clamped tangent" },
                { L"icon_CurveEditor_User_40x.png",        "U",  "User tangent" },
                { L"icon_CurveEditor_Break_40x.png",       "Br", "Break tangent" },
                { L"icon_CurveEditor_Linear_40x.png",      "Ln", "Linear" },
                { L"icon_CurveEditor_Constant_40x.png",    "Cn", "Constant" },
                { L"icon_CurveEditor_Flatten_40x.png",     "Fl", "Flatten" },
                { L"icon_CurveEditor_Straighten_40x.png",  "St", "Straighten" },
            };
            for (const auto& B : TanBtns)
            {
                IconToolButton(B.Tip, LoadToolIcon(B.Icon), B.Fallback, B.Tip, false, CurveIconSize);
                ImGui::SameLine();
            }

            // 구분자 + Create/Delete.
            const ImVec2 SepPos2 = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddLine(
                ImVec2(SepPos2.x + 3.0f, SepPos2.y + 3.0f),
                ImVec2(SepPos2.x + 3.0f, SepPos2.y + 25.0f),
                PSE::Border32);
            ImGui::Dummy(ImVec2(7.0f, 0.0f));
            ImGui::SameLine();

            IconToolButton("Create", LoadToolIcon(L"icon_CurveEditor_Create_40x.png"),
                           "+", "Create curve (not implemented)", false, CurveIconSize);
            ImGui::SameLine();
            IconToolButton("Delete", LoadToolIcon(L"icon_CurveEditor_DeleteTab_40x.png"),
                           "x", "Delete curve (not implemented)", false, CurveIconSize);
        }
        ImGui::EndChild();
        ImGui::PopStyleVar(); // WindowPadding

        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar(2);
        ImGui::Spacing();

        // ── 좌측 트랙 목록 + 우측 그래프 캔버스 ──
        constexpr float TrackListW = 140.0f;
        if (ImGui::BeginChild("##PSECurveTracks", ImVec2(TrackListW, 0.0f), true))
        {
            if (!SelectedEntry)
            {
                ImGui::TextColored(PSE::DimTextV, "Select a\nmodule");
            }
            else
            {
                // 현재는 모듈에 FRawDistribution 필드가 없어 실제 트랙이 없음.
                // 레퍼런스 UI 파리티를 위해 자주 쓰이는 트랙명 placeholder를 노출.
                const char* TrackNames[] = {
                    "ColorOverLife", "AlphaOverLife", "LifeMultiplier",
                    "SizeMultiplier", "VelocityMultiplier"
                };
                for (int32 i = 0; i < IM_ARRAYSIZE(TrackNames); ++i)
                {
                    ImGui::PushID(i);
                    ImDrawList*  DL  = ImGui::GetWindowDrawList();
                    const ImVec2 Pos = ImGui::GetCursorScreenPos();
                    constexpr float Sw = 8.0f;
                    // RGB 채널 표시 박스 — Cascade의 작은 색 사각형 3개를 모사.
                    DL->AddRectFilled(ImVec2(Pos.x, Pos.y + 3.0f),
                                      ImVec2(Pos.x + Sw, Pos.y + 3.0f + Sw),
                                      IM_COL32(214, 90, 90, 200));
                    DL->AddRectFilled(ImVec2(Pos.x + Sw + 2.0f, Pos.y + 3.0f),
                                      ImVec2(Pos.x + 2 * Sw + 2.0f, Pos.y + 3.0f + Sw),
                                      IM_COL32(96, 196, 96, 200));
                    DL->AddRectFilled(ImVec2(Pos.x + 2 * (Sw + 2.0f), Pos.y + 3.0f),
                                      ImVec2(Pos.x + 3 * Sw + 4.0f, Pos.y + 3.0f + Sw),
                                      IM_COL32(96, 140, 226, 200));
                    ImGui::Dummy(ImVec2(3 * Sw + 8.0f, Sw + 2.0f));
                    ImGui::SameLine();
                    ImGui::TextColored(PSE::DimTextV, "%s", TrackNames[i]);
                    ImGui::PopID();
                }
            }
        }
        ImGui::EndChild();

        ImGui::SameLine();

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
