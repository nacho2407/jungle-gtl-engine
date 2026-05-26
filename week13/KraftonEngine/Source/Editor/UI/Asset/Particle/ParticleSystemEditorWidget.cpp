#include "ParticleSystemEditorWidget.h"

#include "imgui.h"
#include "Object/Object.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleEmitterInstances.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModule.h"
#include "Particles/ParticleModuleRequired.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemManager.h"
#include "Particles/Color/ParticleModuleColor.h"
#include "Particles/Color/ParticleModuleColorOverLife.h"
#include "Particles/Material/ParticleModuleMeshMaterial.h"
#include "Particles/Lifetime/ParticleModuleLifetime.h"
#include "Particles/Location/ParticleModuleLocation.h"
#include "Particles/Rotation/ParticleModuleMeshRotation.h"
#include "Particles/RotationRate/ParticleModuleMeshRotationRate.h"
#include "Particles/Size/ParticleModuleSize.h"
#include "Particles/Spawn/ParticleModuleSpawn.h"
#include "Particles/Spawn/ParticleModuleSpawnPerUnit.h"
#include "Particles/Trail/ParticleModuleTrailSource.h"
#include "Particles/TypeData/ParticleModuleTypeDataBase.h"
#include "Particles/TypeData/ParticleModuleTypeDataMesh.h"
#include "Particles/TypeData/ParticleModuleTypeDataRibbon.h"
#include "Particles/TypeData/ParticleModuleTypeDataBeam2.h"
#include "Particles/Beam/ParticleModuleBeamSource.h"
#include "Particles/Beam/ParticleModuleBeamTarget.h"
#include "Particles/Beam/ParticleModuleBeamNoise.h"
#include "Particles/Beam/ParticleModuleBeamModifier.h"
#include "Particles/Velocity/ParticleModuleVelocity.h"
#include "Particles/Event/ParticleModuleEventGenerator.h"
#include "Particles/Collision/ParticleModuleCollision.h"
#include "Materials/Material.h"
#include "Materials/Graph/MaterialGraphAsset.h"
#include "Materials/MaterialManager.h"
#include "Mesh/MeshManager.h"
#include "Mesh/Static/StaticMesh.h"

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
#include <unordered_map>

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
#include "Serialization/WindowsArchive.h"
#include "Asset/AssetPackage.h"
#include "Viewport/Viewport.h"

#if defined(_WIN32)
#include <shellapi.h>
#endif

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
            if (Cast<UDistributionVectorConstant>(Raw.Distribution)) TypeStr = "Constant";
            else if (Cast<UDistributionVectorUniform>(Raw.Distribution)) TypeStr = "Uniform";

            if (ImGui::BeginCombo("Type", TypeStr))
            {
                if (ImGui::Selectable("Constant", TypeStr == "Constant"))
                {
                    Raw.Distribution = UObjectManager::Get().CreateObject<UDistributionVectorConstant>(Outer);
                    bChanged         = true;
                }
                if (ImGui::Selectable("Uniform", TypeStr == "Uniform"))
                {
                    Raw.Distribution = UObjectManager::Get().CreateObject<UDistributionVectorUniform>(Outer);
                    bChanged         = true;
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
                    bChanged         = true;
                }
                if (ImGui::Selectable("Uniform", TypeStr == "Uniform"))
                {
                    Raw.Distribution = UObjectManager::Get().CreateObject<UDistributionFloatUniform>(Outer);
                    bChanged         = true;
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

    FParticleBeam2EmitterInstance* GetPreviewBeamInstance(UParticleSystemComponent* PreviewPSC, int32 EmitterIndex)
    {
        if (!PreviewPSC || EmitterIndex < 0)
        {
            return nullptr;
        }

        const TArray<FParticleEmitterInstance*>& Instances = PreviewPSC->GetEmitterInstances();
        if (EmitterIndex >= static_cast<int32>(Instances.size()))
        {
            return nullptr;
        }

        return dynamic_cast<FParticleBeam2EmitterInstance*>(Instances[EmitterIndex]);
    }

    FVector& GetPreviewBeamUserSetPoint(
        std::unordered_map<const UParticleModule*, FVector>& Points,
        const UParticleModule* Module,
        UParticleSystemComponent* PreviewPSC,
        int32 EmitterIndex,
        bool bSource)
    {
        auto It = Points.find(Module);
        if (It != Points.end())
        {
            return It->second;
        }

        FVector InitialPoint = bSource ? FVector::ZeroVector : FVector(100.0f, 0.0f, 0.0f);
        if (FParticleBeam2EmitterInstance* BeamInst = GetPreviewBeamInstance(PreviewPSC, EmitterIndex))
        {
            FVector RuntimePoint;
            const bool bHasRuntimePoint = bSource
                ? BeamInst->GetBeamSourcePoint(0, RuntimePoint)
                : BeamInst->GetBeamTargetPoint(0, RuntimePoint);
            if (bHasRuntimePoint)
            {
                InitialPoint = RuntimePoint;
            }
        }

        return Points.emplace(Module, InitialPoint).first->second;
    }

    void ApplyPreviewBeamUserSetPoint(
        UParticleSystemComponent* PreviewPSC,
        int32 EmitterIndex,
        bool bSource,
        const FVector& Point)
    {
        if (FParticleBeam2EmitterInstance* BeamInst = GetPreviewBeamInstance(PreviewPSC, EmitterIndex))
        {
            if (bSource)
            {
                BeamInst->SetBeamSourcePoint(Point, 0);
            }
            else
            {
                BeamInst->SetBeamTargetPoint(Point, 0);
            }
        }
    }

    std::unordered_map<const UParticleModule*, FVector> GPreviewBeamUserSetSourcePoints;
    std::unordered_map<const UParticleModule*, FVector> GPreviewBeamUserSetTargetPoints;

    float Clamp01(float V, float Lo, float Hi)
    {
        return V < Lo ? Lo : (V > Hi ? Hi : V);
    }

    // ── 모듈 목록 헬퍼 ───────────────────────────────────────────────────────
    // LOD0의 모듈을 표준 순서(Required → Spawn → Modules → TypeData)로 펼친다.
    const char* MaterialDomainName(EMaterialDomain Domain)
    {
        switch (Domain)
        {
        case EMaterialDomain::Surface:
            return "Surface";
        case EMaterialDomain::ParticleSprite:
            return "ParticleSprite";
        case EMaterialDomain::ParticleMesh:
            return "ParticleMesh";
        case EMaterialDomain::Decal:
            return "Decal";
        case EMaterialDomain::PostProcess:
            return "PostProcess";
        default:
            return "Unknown";
        }
    }

    struct FEmitterModuleEntry
    {
        const char*      Name;
        UParticleModule* Module;
    };

    const char* GetModuleDisplayName(const UParticleModule* Module)
    {
        if (!Module) return "Module";
        if (Cast<UParticleModuleRequired>(Module)) return "Required";
        if (Cast<UParticleModuleSpawn>(Module)) return "Spawn";
        if (Cast<UParticleModuleLifetime>(Module)) return "Lifetime";
        if (Cast<UParticleModuleLocation>(Module)) return "Location";
        if (Cast<UParticleModuleVelocity>(Module)) return "Velocity";
        if (Cast<UParticleModuleSize>(Module)) return "Size";
        if (Cast<UParticleModuleColorOverLife>(Module)) return "Color Over Life";
        if (Cast<UParticleModuleEventGenerator>(Module)) return "Event Generator";
        if (Cast<UParticleModuleCollision>(Module)) return "Collision";
        if (Cast<UParticleModuleColor>(Module)) return "Color";
        if (Cast<UParticleModuleMeshMaterial>(Module)) return "Mesh Material";
        if (Cast<UParticleModuleMeshRotation>(Module)) return "Mesh Rotation";
        if (Cast<UParticleModuleMeshRotationRate>(Module)) return "Mesh Rotation Rate";
        if (Cast<UParticleModuleSpawnPerUnit>(Module)) return "Spawn Per Unit";
        if (Cast<UParticleModuleTrailSource>(Module)) return "Trail Source";
        if (Cast<UParticleModuleBeamSource>(Module)) return "Beam Source";
        if (Cast<UParticleModuleBeamTarget>(Module)) return "Beam Target";
        if (Cast<UParticleModuleBeamNoise>(Module)) return "Beam Noise";
        if (auto* BeamModifier = Cast<UParticleModuleBeamModifier>(Module))
        {
            return BeamModifier->ModifierType == PEB2MT_Source ? "Beam Source Modifier" : "Beam Target Modifier";
        }
        // TypeData subclasses — 구체 타입을 표시.
        if (Cast<UParticleModuleTypeDataMesh>(Module))   return "Mesh Data";
        if (Cast<UParticleModuleTypeDataRibbon>(Module)) return "Ribbon Data";
        if (Cast<UParticleModuleTypeDataBeam2>(Module))  return "Beam Data";
        if (Cast<UParticleModuleTypeDataBase>(Module))   return "TypeData";
        return "Module";
    }

    // 모듈 카테고리별 row 배경 색 — Cascade 레퍼런스의 색 코딩을 차용. alpha 낮게 깔아서
    // 텍스트 가독성 유지. 사용자가 빠르게 카테고리를 식별할 수 있게 한다.
    ImU32 GetModuleCategoryColor(const UParticleModule* Module)
    {
        if (!Module) return IM_COL32(80, 85, 95, 90);
        if (Cast<UParticleModuleRequired>(Module))         return IM_COL32(186, 154, 50, 110);  // yellow-olive
        if (Cast<UParticleModuleSpawn>(Module))            return IM_COL32(178,  73, 73, 110);  // red
        if (Cast<UParticleModuleLifetime>(Module))         return IM_COL32( 58, 142, 130, 100); // teal
        if (Cast<UParticleModuleLocation>(Module))         return IM_COL32(132,  75, 156, 100); // purple
        if (Cast<UParticleModuleVelocity>(Module))         return IM_COL32( 70, 140,  90, 100); // green
        if (Cast<UParticleModuleSize>(Module))             return IM_COL32(196, 130,  60, 100); // orange
        if (Cast<UParticleModuleColorOverLife>(Module))    return IM_COL32( 70, 130, 180, 110); // blue
        if (Cast<UParticleModuleColor>(Module))            return IM_COL32( 80, 130, 170, 100); // blue
        if (Cast<UParticleModuleCollision>(Module))        return IM_COL32(170,  90,  50, 100); // brown
        if (Cast<UParticleModuleEventGenerator>(Module))   return IM_COL32(160,  95, 130, 100); // pink
        if (Cast<UParticleModuleMeshMaterial>(Module))     return IM_COL32( 60,  80, 130, 110);
        if (Cast<UParticleModuleMeshRotation>(Module))     return IM_COL32( 60, 100, 150, 110);
        if (Cast<UParticleModuleMeshRotationRate>(Module)) return IM_COL32( 60, 100, 150, 110);
        if (Cast<UParticleModuleSpawnPerUnit>(Module))     return IM_COL32(110,  90, 140, 110);
        if (Cast<UParticleModuleTrailSource>(Module))      return IM_COL32(110,  90, 140, 110);
        if (Cast<UParticleModuleBeamSource>(Module))       return IM_COL32(130,  75,  90, 110);
        if (Cast<UParticleModuleBeamTarget>(Module))       return IM_COL32(130,  75,  90, 110);
        if (Cast<UParticleModuleBeamNoise>(Module))        return IM_COL32(130,  75,  90, 110);
        if (Cast<UParticleModuleBeamModifier>(Module))     return IM_COL32(130,  75,  90, 110);
        if (Cast<UParticleModuleTypeDataMesh>(Module))     return IM_COL32( 60,  80, 130, 110); // dark blue
        if (Cast<UParticleModuleTypeDataRibbon>(Module))   return IM_COL32(110,  90, 140, 110); // mauve
        if (Cast<UParticleModuleTypeDataBeam2>(Module))    return IM_COL32(130,  75,  90, 110); // dark red
        if (Cast<UParticleModuleTypeDataBase>(Module))     return IM_COL32( 80,  95, 120, 100); // grey-blue
        return IM_COL32(80, 85, 95, 90);
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
            UParticleModule* AsModule = static_cast<UParticleModule*>(LOD->TypeDataModule);
            OutList.push_back({ GetModuleDisplayName(AsModule), AsModule });
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

        const int32    LODCount = GetParticleSystemLODCount(ParticleSystem);
        TArray<float>& Dist     = ParticleSystem->LODDistances;

        if (Dist.empty())
        {
            Dist.push_back(0.0f);
        }
        if (LODCount > 1 && static_cast<int32>(Dist.size()) == LODCount - 1 && Dist[0] > 0.0f)
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
        BuildEmitterModuleListAt(Emitter, LODIndex, Cur);
        BuildEmitterModuleListAt(Emitter, LODIndex - 1, Hi);
        if (ModuleIndex < 0 || ModuleIndex >= static_cast<int32>(Cur.size()) || ModuleIndex >= static_cast<int32>(Hi.
            size()))
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

        const FString    ClassName = FString(Source->GetClass()->GetName());
        UObject*         Created   = FObjectFactory::Get().Create(ClassName, Outer);
        UParticleModule* Copy      = Cast<UParticleModule>(Created);
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
        case PSA_FacingCameraPosition:
            return "FacingCameraPosition";
        case PSA_Square:
            return "Square";
        case PSA_Rectangle:
            return "Rectangle";
        case PSA_Velocity:
            return "Velocity";
        case PSA_AwayFromCenter:
            return "AwayFromCenter";
        case PSA_TypeSpecific:
            return "TypeSpecific";
        case PSA_FacingCameraDistanceBlend:
            return "FacingCameraDistanceBlend";
        default:
            return "?";
        }
    }

    const char* SortModeName(EParticleSortMode V)
    {
        switch (V)
        {
        case PSORTMODE_None:
            return "None";
        case PSORTMODE_ViewProjDepth:
            return "ViewProjDepth";
        case PSORTMODE_DistanceToView:
            return "DistanceToView";
        case PSORTMODE_Age_OldestFirst:
            return "Age_OldestFirst";
        case PSORTMODE_Age_NewestFirst:
            return "Age_NewestFirst";
        default:
            return "?";
        }
    }

    // 같은 LOD에 같은 타입 모듈이 이미 있는지 (중복 추가 방지).
    template <class T>
    bool HasModuleOfType(UParticleLODLevel* LOD)
    {
        if (!LOD) return false;
        for (UParticleModule* M : LOD->Modules)
        {
            if (Cast<T>(M)) return true;
        }
        return false;
    }

    bool HasBeamModifierOfType(UParticleLODLevel* LOD, BeamModifierType Type)
    {
        if (!LOD) return false;
        for (UParticleModule* M : LOD->Modules)
        {
            if (auto* Mod = Cast<UParticleModuleBeamModifier>(M))
            {
                if (Mod->ModifierType == Type)
                {
                    return true;
                }
            }
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
        float                     IconSize
        )
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

    // 24-bit BMP를 디스크에 쓴다. RGBA8 입력을 BGR로 변환. 외부 의존성 없음.
    bool WriteBmp24(const char* Path, uint32 W, uint32 H, const uint8* RGBA, uint32 RowPitch)
    {
        if (!Path || !RGBA || W == 0 || H == 0) return false;

        const uint32 RowBytes = ((W * 3 + 3) / 4) * 4;
        const uint32 ImgSize  = RowBytes * H;
        const uint32 FileSize = 14 + 40 + ImgSize;

        std::FILE* F = nullptr;
    #if defined(_MSC_VER)
        if (fopen_s(&F, Path, "wb") != 0 || !F) return false;
    #else
        F = std::fopen(Path, "wb"); if (!F) return false;
    #endif

        uint8 Hdr[14] = { 'B',
                          'M',
                          (uint8)(FileSize),
                          (uint8)(FileSize >> 8),
                          (uint8)(FileSize >> 16),
                          (uint8)(FileSize >> 24),
                          0,
                          0,
                          0,
                          0,
                          54,
                          0,
                          0,
                          0
        };
        uint8 Dib[40] = { 40,
                          0,
                          0,
                          0,
                          (uint8)(W),
                          (uint8)(W >> 8),
                          (uint8)(W >> 16),
                          (uint8)(W >> 24),
                          (uint8)(H),
                          (uint8)(H >> 8),
                          (uint8)(H >> 16),
                          (uint8)(H >> 24),
                          1,
                          0,
                          24,
                          0,
                          0,
                          0,
                          0,
                          0,
                          (uint8)(ImgSize),
                          (uint8)(ImgSize >> 8),
                          (uint8)(ImgSize >> 16),
                          (uint8)(ImgSize >> 24),
                          0xC4,
                          0x0E,
                          0,
                          0,
                          0xC4,
                          0x0E,
                          0,
                          0,
                          0,
                          0,
                          0,
                          0,
                          0,
                          0,
                          0,
                          0
        };
        std::fwrite(Hdr, 1, sizeof(Hdr), F);
        std::fwrite(Dib, 1, sizeof(Dib), F);

        TArray<uint8> Row(RowBytes, 0);
        for (int32 y = static_cast<int32>(H) - 1; y >= 0; --y)
        {
            const uint8* Src = RGBA + static_cast<size_t>(y) * RowPitch;
            for (uint32 x = 0; x < W; ++x)
            {
                Row[x * 3 + 0] = Src[x * 4 + 2]; // B
                Row[x * 3 + 1] = Src[x * 4 + 1]; // G
                Row[x * 3 + 2] = Src[x * 4 + 0]; // R
            }
            std::fwrite(Row.data(), 1, RowBytes, F);
        }
        std::fclose(F);
        return true;
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

    PushUndoSnapshot();

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
            NewLOD->Level             = static_cast<int32>(NewEmitter->GetLODLevels().size());
            NewLOD->bEnabled          = true;
            NewLOD->RequiredModule    = Last->RequiredModule;
            NewLOD->SpawnModule       = Last->SpawnModule;
            NewLOD->TypeDataModule    = Last->TypeDataModule;
            NewLOD->Modules           = Last->Modules;
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

    PushUndoSnapshot();

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

    PushUndoSnapshot();

    UParticleEmitter* Dst = UObjectManager::Get().CreateObject<UParticleEmitter>(ParticleSystem);
    if (!Dst) return;

    Dst->InitializeDefaultSpriteEmitter();

    Dst->EmitterName            = FName(Src->EmitterName.ToString() + "_Copy");
    Dst->bUseMeshInstance       = Src->bUseMeshInstance;
    Dst->PivotOffset            = Src->PivotOffset;
    Dst->InitialAllocationCount = Src->InitialAllocationCount;
    Dst->SetEnabled(Src->IsEnabled());

    auto DestroyLODModules = [](UParticleLODLevel* LOD)
    {
        if (!LOD) return;
        if (LOD->RequiredModule) UObjectManager::Get().DestroyObject(LOD->RequiredModule);
        if (LOD->SpawnModule) UObjectManager::Get().DestroyObject(LOD->SpawnModule);
        if (LOD->TypeDataModule) UObjectManager::Get().DestroyObject(LOD->TypeDataModule);
        for (UParticleModule* M : LOD->Modules)
        {
            if (M) UObjectManager::Get().DestroyObject(M);
        }
        LOD->Modules.clear();
    };

    for (UParticleLODLevel* LOD : Dst->GetLODLevels())
    {
        DestroyLODModules(LOD);
        UObjectManager::Get().DestroyObject(LOD);
    }
    Dst->GetLODLevels().clear();

    for (UParticleLODLevel* SrcLOD : Src->GetLODLevels())
    {
        if (!SrcLOD) continue;

        UParticleLODLevel* NewLOD = UObjectManager::Get().CreateObject<UParticleLODLevel>(Dst);
        if (!NewLOD) continue;

        NewLOD->Level    = SrcLOD->Level;
        NewLOD->bEnabled = SrcLOD->bEnabled;

        if (auto* R = CloneParticleModule(SrcLOD->RequiredModule, NewLOD)) NewLOD->RequiredModule = Cast<UParticleModuleRequired>(R);
        if (auto* S = CloneParticleModule(SrcLOD->SpawnModule, NewLOD)) NewLOD->SpawnModule = Cast<UParticleModuleSpawn>(S);
        if (auto* T = CloneParticleModule(static_cast<UParticleModule*>(SrcLOD->TypeDataModule), NewLOD)) NewLOD->TypeDataModule = Cast<UParticleModuleTypeDataBase>(T);
        for (UParticleModule* M : SrcLOD->Modules)
        {
            if (auto* Cloned = CloneParticleModule(M, NewLOD))
            {
                NewLOD->Modules.push_back(Cloned);
            }
        }

        NewLOD->UpdateModuleLists();
        Dst->GetLODLevels().push_back(NewLOD);
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
    if (SelectedEmitterIndex < 0 || SelectedEmitterIndex >= static_cast<int32>(ParticleSystem->GetEmitters().
        size())) return;
    if (SelectedModuleIndex < 0) return;

    // Cascade 규약: 모듈 추가/삭제는 LOD 0(highest)에서만 가능. sub-LOD에서는 구조 변경 금지.
    if (SelectedLODIndex != 0) return;

    UParticleEmitter* Emitter = ParticleSystem->GetEmitters()[SelectedEmitterIndex];
    if (!Emitter) return;
    UParticleLODLevel* LOD0 = Emitter->GetLODLevel(0);
    if (!LOD0) return;

    PushUndoSnapshot();

    TArray<FEmitterModuleEntry> ModuleList;
    BuildEmitterModuleListAt(Emitter, 0, ModuleList);

    if (SelectedModuleIndex >= static_cast<int32>(ModuleList.size())) return;

    UParticleModule* Target = ModuleList[SelectedModuleIndex].Module;
    if (!Target) return;

    // Required/Spawn/TypeData는 슬롯 자체를 비우면 시뮬레이션이 깨진다. 삭제 금지.
    if (Target == LOD0->RequiredModule) return;
    if (Target == LOD0->SpawnModule) return;
    if (Target == static_cast<UParticleModule*>(LOD0->TypeDataModule)) return;

    // LOD 0 에서 모듈 위치(인덱스)를 찾는다 — sub-LOD에서도 같은 위치를 제거한다.
    auto It = std::find(LOD0->Modules.begin(), LOD0->Modules.end(), Target);
    if (It == LOD0->Modules.end()) return;
    const int32 ArrayIndex = static_cast<int32>(std::distance(LOD0->Modules.begin(), It));

    // 모든 LOD에서 같은 위치의 모듈을 수거: unique 한 것만 destroy.
    const int32              LODCount = static_cast<int32>(Emitter->GetLODLevels().size());
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
    if (IO.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, false))
    {
        if (IO.KeyShift) Redo();
        else Undo();
    }
    if (IO.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y, false))
    {
        Redo();
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
    const bool bMeshEmitter = [&]()
    {
        UParticleSystem* PS = GetParticleSystem();
        if (!PS) return false;
        if (SelectedEmitterIndex < 0 || SelectedEmitterIndex >= static_cast<int32>(PS->GetEmitters().size())) return false;
        UParticleEmitter* Emitter = PS->GetEmitters()[SelectedEmitterIndex];
        if (!Emitter) return false;
        if (UParticleLODLevel* LOD0 = Emitter->GetLODLevel(0))
        {
            return Cast<UParticleModuleTypeDataMesh>(LOD0->TypeDataModule) != nullptr;
        }
        return false;
    }();
    const FString ExpectedDomain = bMeshEmitter ? FString("ParticleMesh") : FString("ParticleSprite");

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
            Root[MatKeys::Domain] = ExpectedDomain;

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

        const std::filesystem::path CreatedFsPath = ToProjectPath(CreatedPath);
        if (std::filesystem::exists(CreatedFsPath))
        {
            std::ifstream InFile(CreatedFsPath);
            FString Content((std::istreambuf_iterator<char>(InFile)), std::istreambuf_iterator<char>());
            json::JSON Root = json::JSON::Load(Content);
            if (!Root.IsNull())
            {
                Root[MatKeys::Domain] = ExpectedDomain;
                std::ofstream OutFile(CreatedFsPath);
                if (OutFile.is_open())
                {
                    OutFile << Root.dump();
                }
            }
        }
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

void FParticleSystemEditorWidget::AddLODAfterSelected()
{
    UParticleSystem* PS = GetParticleSystem();
    if (!PS) return;

    PushUndoSnapshot();
    SyncParticleSystemLODDistances(PS);

    // 시스템 단위 LODDistances 에 항목 추가 — 인덱스가 LOD 레벨과 1:1로 대응한다.
    const int32 InsertLODIndex = SelectedLODIndex + 1;
    const float PrevDist       = InsertLODIndex > 0 && InsertLODIndex - 1 < static_cast<int32>(PS->LODDistances.size())
    ? PS->LODDistances[InsertLODIndex - 1] : 0.0f;
    const float NextDist = InsertLODIndex < static_cast<int32>(PS->LODDistances.size())
    ? PS->LODDistances[InsertLODIndex] : (PrevDist > 0.0f ? PrevDist * 2.0f : 1000.0f);
    const float DefaultDist = NextDist > PrevDist ? (PrevDist + NextDist) * 0.5f
    : (PrevDist > 0.0f ? PrevDist * 2.0f : 1000.0f);
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
        New->Level             = InsertLODIndex;
        New->bEnabled          = true;
        New->RequiredModule    = Src->RequiredModule;
        New->SpawnModule       = Src->SpawnModule;
        New->TypeDataModule    = Src->TypeDataModule;
        New->Modules           = Src->Modules; // pointer copy = sharing

        auto& LODs = Emitter->GetLODLevels();
        if (InsertLODIndex >= static_cast<int32>(LODs.size())) LODs.push_back(New);
        else LODs.insert(LODs.begin() + InsertLODIndex, New);

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

    PushUndoSnapshot();
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
            if (L->SpawnModule == M) return true;
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
            auto                     Push = [&](UParticleModule* M)
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

// ── LOD 관리 ───────────────────────────────────────────────────────────────
void FParticleSystemEditorWidget::SelectLOD(int32 LODIndex)
{
    SelectedLODIndex    = (std::max)(0, LODIndex);
    SelectedModuleIndex = -1; // sub-LOD 전환 시 모듈 선택은 리셋 (구조가 다를 수 있음).
}

// ── Regenerate LOD ─────────────────────────────────────────────────────────
// Src LOD 의 모듈을 Dst LOD 로 deep-clone 한 뒤 spawn rate 계열 값을 SpawnRateScale 로
// 곱한다. Cascade 의 "Regenerate Lowest from Highest" (Scale 0.5), "Regen Highest from
// Lowest" (Scale 1/Scale) 두 패턴에 모두 활용 가능.
void FParticleSystemEditorWidget::RegenerateLOD(int32 SrcLODIndex, int32 DstLODIndex, float SpawnRateScale)
{
    UParticleSystem* PS = GetParticleSystem();
    if (!PS) return;
    if (SrcLODIndex == DstLODIndex) return;

    PushUndoSnapshot();

    for (UParticleEmitter* Emitter : PS->GetEmitters())
    {
        if (!Emitter) continue;
        UParticleLODLevel* Src = Emitter->GetLODLevel(SrcLODIndex);
        UParticleLODLevel* Dst = Emitter->GetLODLevel(DstLODIndex);
        if (!Src || !Dst) continue;

        // Dst 의 unique 모듈 (다른 LOD 에서 참조되지 않는 것) 부터 destroy.
        auto IsRefElsewhere = [&](UParticleModule* M)
        {
            if (!M) return false;
            const int32 LC = static_cast<int32>(Emitter->GetLODLevels().size());
            for (int32 i = 0; i < LC; ++i)
            {
                if (i == DstLODIndex) continue;
                UParticleLODLevel* L = Emitter->GetLODLevel(i);
                if (!L) continue;
                if (L->RequiredModule == M) return true;
                if (L->SpawnModule == M) return true;
                if (static_cast<UParticleModule*>(L->TypeDataModule) == M) return true;
                for (UParticleModule* X : L->Modules) if (X == M) return true;
            }
            return false;
        };
        auto DestroyIfOwned = [&](UParticleModule* M)
        {
            if (M && !IsRefElsewhere(M)) UObjectManager::Get().DestroyObject(M);
        };

        DestroyIfOwned(Dst->RequiredModule);
        DestroyIfOwned(Dst->SpawnModule);
        DestroyIfOwned(static_cast<UParticleModule*>(Dst->TypeDataModule));
        for (UParticleModule* M : Dst->Modules) DestroyIfOwned(M);
        Dst->Modules.clear();
        Dst->RequiredModule = nullptr;
        Dst->SpawnModule    = nullptr;
        Dst->TypeDataModule = nullptr;

        // Src 의 모든 슬롯을 deep-clone 해서 Dst 에 채운다.
        if (auto* R = CloneParticleModule(Src->RequiredModule, Dst)) Dst->RequiredModule = Cast<
            UParticleModuleRequired>(R);
        if (auto* S = CloneParticleModule(Src->SpawnModule, Dst)) Dst->SpawnModule = Cast<UParticleModuleSpawn>(S);
        if (auto* T = CloneParticleModule(static_cast<UParticleModule*>(Src->TypeDataModule), Dst)) Dst->TypeDataModule
        = Cast<UParticleModuleTypeDataBase>(T);
        for (UParticleModule* M : Src->Modules)
        {
            if (auto* N = CloneParticleModule(M, Dst)) Dst->Modules.push_back(N);
        }

        // Spawn rate 계열 값을 스케일.
        if (Dst->RequiredModule)
        {
            Dst->RequiredModule->SpawnRate *= SpawnRateScale;
        }
        if (Dst->SpawnModule)
        {
            Dst->SpawnModule->SpawnRate      *= SpawnRateScale;
            Dst->SpawnModule->SpawnRateScale *= SpawnRateScale;
            Dst->SpawnModule->BurstScale     *= SpawnRateScale;
        }
        Dst->UpdateModuleLists();
    }

    MarkDirty();
    RestartPreviewSimulation();
}

// ── 모듈 sharing 관리 ──────────────────────────────────────────────────────
void FParticleSystemEditorWidget::DuplicateModuleFromHigherLOD(
    UParticleEmitter* Emitter,
    int32             LODIndex,
    int32             ModuleIndex
    )
{
    if (!Emitter || LODIndex <= 0 || ModuleIndex < 0) return;
    UParticleLODLevel* Cur = Emitter->GetLODLevel(LODIndex);
    UParticleLODLevel* Hi  = Emitter->GetLODLevel(LODIndex - 1);
    if (!Cur || !Hi) return;

    TArray<FEmitterModuleEntry> CurList, HiList;
    BuildEmitterModuleListAt(Emitter, LODIndex, CurList);
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
        else Cur->Modules.push_back(Clone);
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
    BuildEmitterModuleListAt(Emitter, LODIndex, CurList);
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
    bool        bReferenced = false;
    const int32 LCount      = static_cast<int32>(Emitter->GetLODLevels().size());
    for (int32 i = 0; i < LCount && !bReferenced; ++i)
    {
        UParticleLODLevel* L = Emitter->GetLODLevel(i);
        if (!L) continue;
        if (L->RequiredModule == OldUnique || L->SpawnModule == OldUnique || static_cast<UParticleModule*>(L->
            TypeDataModule) == OldUnique)
        {
            bReferenced = true;
            break;
        }
        for (UParticleModule* M : L->Modules) if (M == OldUnique)
        {
            bReferenced = true;
            break;
        }
    }
    if (!bReferenced && OldUnique) UObjectManager::Get().DestroyObject(OldUnique);

    MarkDirty();
    RestartPreviewSimulation();
}

void FParticleSystemEditorWidget::DuplicateModuleFromHighestLOD(
    UParticleEmitter* Emitter,
    int32             LODIndex,
    int32             ModuleIndex
    )
{
    if (!Emitter || LODIndex <= 0 || ModuleIndex < 0) return;
    UParticleLODLevel* Cur = Emitter->GetLODLevel(LODIndex);
    UParticleLODLevel* Top = Emitter->GetLODLevel(0);
    if (!Cur || !Top) return;

    TArray<FEmitterModuleEntry> CurList, TopList;
    BuildEmitterModuleListAt(Emitter, LODIndex, CurList);
    BuildEmitterModuleListAt(Emitter, 0, TopList);
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
        else Cur->Modules.push_back(Clone);
    }
    Cur->UpdateModuleLists();

    MarkDirty();
    RestartPreviewSimulation();
}

// ── Drag and Drop — 모듈 / 이미터 이동 ─────────────────────────────────────
void FParticleSystemEditorWidget::MoveModule(
    int32 SrcEmitterIndex,
    int32 SrcArrayIndex,
    int32 DstEmitterIndex,
    int32 DstArrayIndex
    )
{
    UParticleSystem* PS = GetParticleSystem();
    if (!PS) return;

    auto& Emitters = PS->GetEmitters();
    if (SrcEmitterIndex < 0 || SrcEmitterIndex >= static_cast<int32>(Emitters.size())) return;
    if (DstEmitterIndex < 0 || DstEmitterIndex >= static_cast<int32>(Emitters.size())) return;

    UParticleEmitter* Src = Emitters[SrcEmitterIndex];
    UParticleEmitter* Dst = Emitters[DstEmitterIndex];
    if (!Src || !Dst) return;
    if (SrcEmitterIndex == DstEmitterIndex && SrcArrayIndex == DstArrayIndex) return;

    // 다른 이미터로 옮길 때 같은 타입 모듈이 이미 있으면 거부 (중복 방지).
    if (SrcEmitterIndex != DstEmitterIndex)
    {
        UParticleLODLevel* SrcLOD0 = Src->GetLODLevel(0);
        UParticleLODLevel* DstLOD0 = Dst->GetLODLevel(0);
        if (SrcLOD0 && DstLOD0 && SrcArrayIndex < static_cast<int32>(SrcLOD0->Modules.size()))
        {
            UParticleModule* MovingProbe = SrcLOD0->Modules[SrcArrayIndex];
            for (UParticleModule* Existing : DstLOD0->Modules)
            {
                if (Existing && MovingProbe && std::strcmp(
                    Existing->GetClass()->GetName(),
                    MovingProbe->GetClass()->GetName()
                ) == 0)
                {
                    return; // 같은 타입 중복 — 무시.
                }
            }
        }
    }

    PushUndoSnapshot();

    // 모든 LOD에 동일 위치로 적용 (구조 변경은 LOD 0 결정이지만 sub-LOD 도 평행 구조 유지).
    const int32 LCount = (std::max)(
        static_cast<int32>(Src->GetLODLevels().size()),
        static_cast<int32>(Dst->GetLODLevels().size())
    );
    for (int32 L = 0; L < LCount; ++L)
    {
        UParticleLODLevel* SrcLOD = Src->GetLODLevel(L);
        UParticleLODLevel* DstLOD = Dst->GetLODLevel(L);
        if (!SrcLOD || !DstLOD) continue;
        if (SrcArrayIndex < 0 || SrcArrayIndex >= static_cast<int32>(SrcLOD->Modules.size())) continue;

        UParticleModule* M = SrcLOD->Modules[SrcArrayIndex];
        SrcLOD->Modules.erase(SrcLOD->Modules.begin() + SrcArrayIndex);

        int32 InsertAt = DstArrayIndex;
        // 같은 이미터에서 뒤로 옮길 때는 erase 로 인덱스가 한 칸 당겨졌으니 조정.
        if (SrcEmitterIndex == DstEmitterIndex && DstArrayIndex > SrcArrayIndex)
        {
            InsertAt = DstArrayIndex - 1;
        }
        if (InsertAt < 0) InsertAt = 0;
        if (InsertAt > static_cast<int32>(DstLOD->Modules.size())) InsertAt = static_cast<int32>(DstLOD->Modules.
            size());

        DstLOD->Modules.insert(DstLOD->Modules.begin() + InsertAt, M);

        SrcLOD->UpdateModuleLists();
        if (DstLOD != SrcLOD) DstLOD->UpdateModuleLists();
    }

    // 이동 후 선택 모듈은 새 위치로 따라간다 — Required/Spawn 갯수만큼 오프셋 (보통 2).
    UParticleLODLevel* DstLOD0 = Dst->GetLODLevel(0);
    if (DstLOD0)
    {
        int32 DisplayOffset = 0;
        if (DstLOD0->RequiredModule) ++DisplayOffset;
        if (DstLOD0->SpawnModule) ++DisplayOffset;
        int32 NewDisplay = DisplayOffset + DstArrayIndex;
        if (SrcEmitterIndex == DstEmitterIndex && DstArrayIndex > SrcArrayIndex) --NewDisplay;
        SelectEmitter(DstEmitterIndex, NewDisplay);
    }

    MarkDirty();
    RestartPreviewSimulation();
}

void FParticleSystemEditorWidget::MoveEmitter(int32 SrcEmitterIndex, int32 DstEmitterIndex)
{
    UParticleSystem* PS = GetParticleSystem();
    if (!PS) return;

    auto&       Emitters = PS->GetEmitters();
    const int32 N        = static_cast<int32>(Emitters.size());
    if (SrcEmitterIndex < 0 || SrcEmitterIndex >= N) return;
    if (DstEmitterIndex < 0 || DstEmitterIndex >= N) return;
    if (SrcEmitterIndex == DstEmitterIndex) return;

    PushUndoSnapshot();

    UParticleEmitter* Moving = Emitters[SrcEmitterIndex];
    Emitters.erase(Emitters.begin() + SrcEmitterIndex);

    int32 InsertAt = DstEmitterIndex;
    if (SrcEmitterIndex < DstEmitterIndex) InsertAt = DstEmitterIndex - 1;
    if (InsertAt < 0) InsertAt = 0;
    if (InsertAt > static_cast<int32>(Emitters.size())) InsertAt = static_cast<int32>(Emitters.size());

    Emitters.insert(Emitters.begin() + InsertAt, Moving);

    // 선택 인덱스 재조정.
    if (SelectedEmitterIndex == SrcEmitterIndex)
    {
        SelectedEmitterIndex = InsertAt;
    }
    else if (SrcEmitterIndex < SelectedEmitterIndex && SelectedEmitterIndex <= InsertAt)
    {
        --SelectedEmitterIndex;
    }
    else if (SrcEmitterIndex > SelectedEmitterIndex && SelectedEmitterIndex >= InsertAt)
    {
        ++SelectedEmitterIndex;
    }
    EmitterNameBufFor = -1;

    MarkDirty();
    RestartPreviewSimulation();
}

void FParticleSystemEditorWidget::SetEmitterTypeData(int32 EmitterIndex, const char* TypeDataClassName)
{
    UParticleSystem* PS = GetParticleSystem();
    if (!PS) return;
    if (EmitterIndex < 0 || EmitterIndex >= static_cast<int32>(PS->GetEmitters().size())) return;
    UParticleEmitter* Emitter = PS->GetEmitters()[EmitterIndex];
    if (!Emitter) return;

    PushUndoSnapshot();

    UParticleLODLevel* LOD0 = Emitter->GetLODLevel(0);
    UParticleModuleTypeDataBase* OldType = LOD0 ? LOD0->TypeDataModule : nullptr;

    UParticleModuleTypeDataBase* NewType = nullptr;
    if (TypeDataClassName && std::strcmp(TypeDataClassName, "None") != 0)
    {
        UObject* Created = FObjectFactory::Get().Create(TypeDataClassName, LOD0);
        NewType = Cast<UParticleModuleTypeDataBase>(Created);
        if (!NewType && Created)
        {
            UObjectManager::Get().DestroyObject(Created);
        }
    }

    if (NewType)
    {
        NewType->bEnabled           = true;
        NewType->bSpawnModule       = true;
        NewType->bUpdateModule      = true;
        NewType->bFinalUpdateModule = false;

        if (UParticleModuleTypeDataBeam2* BeamType = Cast<UParticleModuleTypeDataBeam2>(NewType))
        {
            if (!BeamType->Distance.Distribution)
            {
                auto* D = UObjectManager::Get().CreateObject<UDistributionFloatConstant>(BeamType);
                D->Constant = 500.0f;
                BeamType->Distance.Distribution = D;
            }
            if (!BeamType->TaperFactor.Distribution)
            {
                auto* D = UObjectManager::Get().CreateObject<UDistributionFloatConstant>(BeamType);
                D->Constant = 1.0f;
                BeamType->TaperFactor.Distribution = D;
            }
            if (!BeamType->TaperScale.Distribution)
            {
                auto* D = UObjectManager::Get().CreateObject<UDistributionFloatConstant>(BeamType);
                D->Constant = 1.0f;
                BeamType->TaperScale.Distribution = D;
            }
        }
    }

    // 모든 LOD 의 TypeData 슬롯을 새 인스턴스 포인터로 동기화 (sharing).
    for (UParticleLODLevel* LL : Emitter->GetLODLevels())
    {
        if (LL)
        {
            LL->TypeDataModule = NewType;
            LL->UpdateModuleLists();
        }
    }

    // 옛 TypeData 가 다른 어디서도 참조되지 않으면 destroy.
    if (OldType)
    {
        bool bStillReferenced = false;
        for (UParticleEmitter* OtherE : PS->GetEmitters())
        {
            if (!OtherE) continue;
            for (UParticleLODLevel* LL : OtherE->GetLODLevels())
            {
                if (LL && static_cast<UParticleModuleTypeDataBase*>(LL->TypeDataModule) == OldType)
                {
                    bStillReferenced = true;
                    break;
                }
            }
            if (bStillReferenced) break;
        }
        if (!bStillReferenced)
        {
            UObjectManager::Get().DestroyObject(OldType);
        }
    }

    // Mesh TypeData 면 emitter 도 mesh 모드로 표시 — runtime 측 BuildEmitterInstances 가
    // bUseMeshInstance 분기를 한다.
    Emitter->bUseMeshInstance = (Cast<UParticleModuleTypeDataMesh>(NewType) != nullptr);

    MarkDirty();
    RestartPreviewSimulation();
}

// ── Undo / Redo ────────────────────────────────────────────────────────────
void FParticleSystemEditorWidget::PushUndoSnapshot()
{
    UParticleSystem* PS = GetParticleSystem();
    if (!PS) return;

    FMemoryArchive Ar(true);
    PS->Serialize(Ar);

    // 직전 스냅샷과 동일하면 중복 push 스킵 — 구조 변경(explicit) 과 자동 캡처(범용)
    // 가 같은 pre-edit 상태를 두 번 쌓는 경우를 막는다.
    if (!UndoStack.empty() && UndoStack.back() == Ar.GetBuffer())
    {
        return;
    }

    UndoStack.push_back(Ar.GetBuffer());
    while (static_cast<int32>(UndoStack.size()) > MaxUndoStackSize)
    {
        UndoStack.erase(UndoStack.begin());
    }
    RedoStack.clear();

    // 자동 캡처가 이번 explicit push 직후에 같은 상태를 또 쌓지 못하도록 초기화.
    bPreEditCached = false;
    bPushPending   = false;
}

void FParticleSystemEditorWidget::Undo()
{
    if (UndoStack.empty()) return;
    UParticleSystem* PS = GetParticleSystem();
    if (!PS) return;

    FMemoryArchive Cur(true);
    PS->Serialize(Cur);
    RedoStack.push_back(Cur.GetBuffer());

    const TArray<uint8> Snap = UndoStack.back();
    UndoStack.pop_back();
    RestoreFromSnapshot(Snap);
}

void FParticleSystemEditorWidget::Redo()
{
    if (RedoStack.empty()) return;
    UParticleSystem* PS = GetParticleSystem();
    if (!PS) return;

    FMemoryArchive Cur(true);
    PS->Serialize(Cur);
    UndoStack.push_back(Cur.GetBuffer());

    const TArray<uint8> Snap = RedoStack.back();
    RedoStack.pop_back();
    RestoreFromSnapshot(Snap);
}

void FParticleSystemEditorWidget::RestoreFromSnapshot(const TArray<uint8>& Buffer)
{
    UParticleSystem* PS = GetParticleSystem();
    if (!PS) return;

    // 옛 이미터들에 PSC 가 instance 포인터 캐싱하고 있다 — destroy 전에 instance 정리.
    if (PreviewPSC)
    {
        PreviewPSC->SetTemplate(nullptr);
    }

    for (UParticleEmitter* E : PS->GetEmitters())
    {
        if (E) UObjectManager::Get().DestroyObject(E);
    }
    PS->GetEmitters().clear();

    FMemoryArchive Loader(Buffer, false);
    PS->Serialize(Loader);

    SelectedEmitterIndex = -1;
    SelectedModuleIndex  = -1;
    EmitterNameBufFor    = -1;
    SyncEmitterUIState();

    if (PreviewPSC)
    {
        PreviewPSC->SetTemplate(PS);
    }

    MarkDirty();
    bSimulating = true;
    PreviewTime = 0.0f;
    if (ViewportClient.IsRenderable())
    {
        ViewportClient.ResetCameraToPreviewBounds();
    }
}

// ── 썸네일 — 프리뷰 RT 를 <asset>.thumb.bmp 로 캡처 ────────────────────────
void FParticleSystemEditorWidget::SaveThumbnail()
{
    UParticleSystem* PS = GetParticleSystem();
    if (!PS) return;
    FViewport* VP = ViewportClient.GetViewport();
    if (!VP || !GEngine) return;
    ID3D11Texture2D* RT = VP->GetRTTexture();
    if (!RT) return;

    D3D11_TEXTURE2D_DESC Desc {};
    RT->GetDesc(&Desc);

    D3D11_TEXTURE2D_DESC StagingDesc = Desc;
    StagingDesc.Usage                = D3D11_USAGE_STAGING;
    StagingDesc.BindFlags            = 0;
    StagingDesc.CPUAccessFlags       = D3D11_CPU_ACCESS_READ;
    StagingDesc.MiscFlags            = 0;

    ID3D11Device*        Dev = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
    ID3D11DeviceContext* Ctx = GEngine->GetRenderer().GetFD3DDevice().GetDeviceContext();
    if (!Dev || !Ctx) return;

    ID3D11Texture2D* Staging = nullptr;
    if (FAILED(Dev->CreateTexture2D(&StagingDesc, nullptr, &Staging)) || !Staging) return;

    Ctx->CopyResource(Staging, RT);

    D3D11_MAPPED_SUBRESOURCE Mapped {};
    if (SUCCEEDED(Ctx->Map(Staging, 0, D3D11_MAP_READ, 0, &Mapped)))
    {
        FString Path = PS->GetSourcePath();
        if (Path.empty()) Path = "ParticleThumbnail";
        const size_t Dot = Path.find_last_of('.');
        if (Dot != FString::npos) Path.resize(Dot);
        Path += ".thumb.bmp";

        WriteBmp24(Path.c_str(), Desc.Width, Desc.Height, static_cast<const uint8*>(Mapped.pData), Mapped.RowPitch);
        Ctx->Unmap(Staging, 0);
    }
    Staging->Release();
}

// ── Save As — 같은 폴더에 새 이름으로 .uasset 복제 ─────────────────────────
void FParticleSystemEditorWidget::SaveAssetAs(const FString& NewAssetName)
{
    UParticleSystem* PS = GetParticleSystem();
    if (!PS || NewAssetName.empty()) return;

    const FString OldPath = PS->GetSourcePath();
    if (OldPath.empty()) return;

    // 같은 디렉토리에 새 이름으로 저장.
    std::filesystem::path OldFsPath(FPaths::ToWide(FPaths::MakeProjectRelative(OldPath)));
    std::filesystem::path Dir       = OldFsPath.parent_path();
    std::filesystem::path NewFsPath = Dir / (FPaths::ToWide(NewAssetName) + L".uasset");
    if (std::filesystem::exists(NewFsPath))
    {
        return; // 충돌 시 무시 (사용자가 다른 이름 입력하도록).
    }

    const FString NewPathUtf8 = FPaths::ToUtf8(NewFsPath.wstring());
    PS->SetSourcePath(NewPathUtf8);
    const bool bSaved = FParticleSystemManager::Get().Save(PS);
    if (!bSaved)
    {
        PS->SetSourcePath(OldPath);
        return;
    }
    ClearDirty();
    WindowTitle = "Particle System Editor - ";
    WindowTitle += NewPathUtf8;
}

// ── Reimport — 디스크에서 다시 읽어 메모리 상태 폐기 ──────────────────────
void FParticleSystemEditorWidget::ReimportAsset()
{
    UParticleSystem* PS = GetParticleSystem();
    if (!PS) return;
    const FString Path = PS->GetSourcePath();
    if (Path.empty()) return;

    // PSC 가 보유한 instance 부터 정리.
    if (PreviewPSC) PreviewPSC->SetTemplate(nullptr);

    // 옛 이미터 destroy.
    for (UParticleEmitter* E : PS->GetEmitters())
    {
        if (E) UObjectManager::Get().DestroyObject(E);
    }
    PS->GetEmitters().clear();

    // 디스크에서 다시 로드.
    FWindowsBinReader Ar(FPaths::MakeProjectRelative(Path));
    if (Ar.IsValid())
    {
        FAssetPackageHeader Header;
        Ar << Header;
        FAssetImportMetadata Meta;
        Ar << Meta;
        PS->Serialize(Ar);
    }

    SelectedEmitterIndex = -1;
    SelectedModuleIndex  = -1;
    EmitterNameBufFor    = -1;
    SyncEmitterUIState();

    if (PreviewPSC) PreviewPSC->SetTemplate(PS);
    ClearDirty();
    UndoStack.clear();
    RedoStack.clear();
    bSimulating = true;
    PreviewTime = 0.0f;
    if (ViewportClient.IsRenderable())
    {
        ViewportClient.ResetCameraToPreviewBounds();
    }
}

// ── Find in Content Browser — 콘텐츠 브라우저 측 API 가 없어 안내만 노출 ──
void FParticleSystemEditorWidget::FindInContentBrowser()
{
    UParticleSystem* PS = GetParticleSystem();
    if (!PS) return;
    // OpenPopup 은 BeginPopup 과 같은 nesting level 에서 호출되어야 한다 — 메뉴/툴바
    // 깊숙한 곳에서 바로 OpenPopup 하면 안 잡힘. Render 루프에서 deferred 처리.
    bFindCBPopupRequested = true;
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

// ── 툴바 (Content/Editor/ToolIcons/ 아이콘 사용, 가로 스크롤 지원) ─────────
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

            ImGui::TextColored(PSE::DimTextV, "  |  Selection: Emitter %d > %s", SelectedEmitterIndex, ModuleName);
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

                        // 우측 curve placeholder 아이콘 — row 세로 중앙 정렬.
                        const ImVec2 RowMin     = ImGui::GetItemRectMin();
                        const ImVec2 RowMax     = ImGui::GetItemRectMax();
                        const float  RowCenterY = (RowMin.y + RowMax.y) * 0.5f;
                        const ImVec2 CurveIconMin(ColRightPx - IconSize - 4.0f, RowCenterY - IconSize * 0.5f);
                        const ImVec2 CurveIconMax(ColRightPx - 4.0f, RowCenterY + IconSize * 0.5f);
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
                                IM_COL32(110, 115, 125, 180),
                                1.0f
                            );
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

// ── 프로퍼티 (스크롤 거의 없는 그룹화) ─────────────────────────────────────
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

// ── 커브 에디터 (placeholder — Distribution 연결 시 채워짐) ────────────────
void FParticleSystemEditorWidget::RenderCurveEditorPanel(float Width, float Height)
{
    UParticleSystem* ParticleSystem = GetParticleSystem();

    UParticleEmitter* SelectedEmitter = nullptr;
    if (ParticleSystem && SelectedEmitterIndex >= 0 && SelectedEmitterIndex < static_cast<int32>(ParticleSystem->
        GetEmitters().size()))
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
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 1.0f, 1.0f, 0.12f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0f, 1.0f, 1.0f, 0.22f));

        constexpr float CurveIconSize = 22.0f;
        // 높이 = WindowPadding y*2 + (icon + FramePadding y*2) + 가로 스크롤바.
        //      = 2*2 + (22 + 2*4) + 14 = 48px.
        constexpr float CurveToolbarH = 48.0f;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(2.0f, 2.0f));
        if (ImGui::BeginChild(
            "##PSECurveToolbar",
            ImVec2(0.0f, CurveToolbarH),
            false,
            ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoBackground
        ))
        {
            struct FCurveBtn
            {
                const wchar_t* Icon;
                const char*    Fallback;
                const char*    Tip;
            };
            // 뷰 fit 버튼 — 현재는 비활성 (distribution 미연결).
            const FCurveBtn FitBtns[] = { { L"icon_CurveEditor_Horizontal_40x.png", "H", "Fit horizontal" },
                                          { L"icon_CurveEditor_Vertical_40x.png", "V", "Fit vertical" },
                                          { L"icon_CurveEditor_ShowAll_40x.png", "All", "Fit all" },
                                          { L"icon_CurveEditor_ZoomToFit_40x.png", "Sel", "Fit selected" },
            };
            for (const auto& B : FitBtns)
            {
                IconToolButton(B.Tip, LoadToolIcon(B.Icon), B.Fallback, B.Tip, false, CurveIconSize);
                ImGui::SameLine();
            }

            // Pan / Zoom 상호작용 모드 — 활성 모드는 강조.
            const bool bPanActive  = (CurveMode == ECurveInteractionMode::Pan);
            const bool bZoomActive = (CurveMode == ECurveInteractionMode::Zoom);
            if (bPanActive) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.29f, 0.56f, 1.0f, 0.40f));
            if (IconToolButton(
                "PanMode",
                LoadToolIcon(L"icon_CurveEditor_Pan_40x.png"),
                "Pan",
                "Pan mode",
                true,
                CurveIconSize
            ))
            {
                CurveMode = ECurveInteractionMode::Pan;
            }
            if (bPanActive) ImGui::PopStyleColor();
            ImGui::SameLine();
            if (bZoomActive) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.29f, 0.56f, 1.0f, 0.40f));
            if (IconToolButton(
                "ZoomMode",
                LoadToolIcon(L"icon_CurveEditor_Zoom_40x.png"),
                "Zm",
                "Zoom mode",
                true,
                CurveIconSize
            ))
            {
                CurveMode = ECurveInteractionMode::Zoom;
            }
            if (bZoomActive) ImGui::PopStyleColor();
            ImGui::SameLine();

            // 구분자.
            const ImVec2 SepPos = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddLine(
                ImVec2(SepPos.x + 3.0f, SepPos.y + 3.0f),
                ImVec2(SepPos.x + 3.0f, SepPos.y + 25.0f),
                PSE::Border32
            );
            ImGui::Dummy(ImVec2(7.0f, 0.0f));
            ImGui::SameLine();

            const FCurveBtn TanBtns[] = { { L"icon_CurveEditor_Auto_40x.png", "A", "Auto tangent" },
                                          { L"icon_CurveEditor_AutoClamped_40x.png", "AC", "Auto/Clamped tangent" },
                                          { L"icon_CurveEditor_User_40x.png", "U", "User tangent" },
                                          { L"icon_CurveEditor_Break_40x.png", "Br", "Break tangent" },
                                          { L"icon_CurveEditor_Linear_40x.png", "Ln", "Linear" },
                                          { L"icon_CurveEditor_Constant_40x.png", "Cn", "Constant" },
                                          { L"icon_CurveEditor_Flatten_40x.png", "Fl", "Flatten" },
                                          { L"icon_CurveEditor_Straighten_40x.png", "St", "Straighten" },
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
                PSE::Border32
            );
            ImGui::Dummy(ImVec2(7.0f, 0.0f));
            ImGui::SameLine();

            IconToolButton(
                "Create",
                LoadToolIcon(L"icon_CurveEditor_Create_40x.png"),
                "+",
                "Create curve (not implemented)",
                false,
                CurveIconSize
            );
            ImGui::SameLine();
            IconToolButton(
                "Delete",
                LoadToolIcon(L"icon_CurveEditor_DeleteTab_40x.png"),
                "x",
                "Delete curve (not implemented)",
                false,
                CurveIconSize
            );
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
                const char* TrackNames[] = { "ColorOverLife",
                                             "AlphaOverLife",
                                             "LifeMultiplier",
                                             "SizeMultiplier",
                                             "VelocityMultiplier"
                };
                for (int32 i = 0; i < IM_ARRAYSIZE(TrackNames); ++i)
                {
                    ImGui::PushID(i);
                    ImDrawList*     DL  = ImGui::GetWindowDrawList();
                    const ImVec2    Pos = ImGui::GetCursorScreenPos();
                    constexpr float Sw  = 8.0f;
                    // RGB 채널 표시 박스 — Cascade의 작은 색 사각형 3개를 모사.
                    DL->AddRectFilled(
                        ImVec2(Pos.x, Pos.y + 3.0f),
                        ImVec2(Pos.x + Sw, Pos.y + 3.0f + Sw),
                        IM_COL32(214, 90, 90, 200)
                    );
                    DL->AddRectFilled(
                        ImVec2(Pos.x + Sw + 2.0f, Pos.y + 3.0f),
                        ImVec2(Pos.x + 2 * Sw + 2.0f, Pos.y + 3.0f + Sw),
                        IM_COL32(96, 196, 96, 200)
                    );
                    DL->AddRectFilled(
                        ImVec2(Pos.x + 2 * (Sw + 2.0f), Pos.y + 3.0f),
                        ImVec2(Pos.x + 3 * Sw + 4.0f, Pos.y + 3.0f + Sw),
                        IM_COL32(96, 140, 226, 200)
                    );
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
            DrawList->AddLine(
                ImVec2(CanvasMin.x + CanvasSize.x * T, CanvasMin.y),
                ImVec2(CanvasMin.x + CanvasSize.x * T, CanvasMax.y),
                Col
            );
            DrawList->AddLine(
                ImVec2(CanvasMin.x, CanvasMin.y + CanvasSize.y * T),
                ImVec2(CanvasMax.x, CanvasMin.y + CanvasSize.y * T),
                Col
            );
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

// ── 모듈 프로퍼티 편집 (CollapsingHeader 그룹) ──────────────────────────────
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
            DrawRawDistributionVector("Start Color", Color->StartColor, bChanged, Color);
            DrawRawDistributionFloat("Start Alpha", Color->StartAlpha, bChanged, Color);
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
        if (ImGui::CollapsingHeader("Spawn Per Unit"))
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
                bChanged = true;
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
                bChanged = true;
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

// ── Burst List 편집 테이블 ──────────────────────────────────────────────────
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
