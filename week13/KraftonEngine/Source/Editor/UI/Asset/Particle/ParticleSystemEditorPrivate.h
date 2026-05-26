#pragma once

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

namespace PSE
{
    static const ImVec4 WindowBg = ImVec4(0.086f, 0.090f, 0.102f, 1.0f);
    static const ImVec4 PanelBg  = ImVec4(0.122f, 0.128f, 0.145f, 1.0f);
    static const ImVec4 Border   = ImVec4(0.224f, 0.235f, 0.267f, 1.0f);
    static const ImVec4 FrameBg  = ImVec4(0.067f, 0.071f, 0.082f, 1.0f);

    static constexpr ImU32 HeaderText = IM_COL32(228, 231, 238, 255);
    static constexpr ImU32 DimText    = IM_COL32(122, 128, 140, 255);
    static constexpr ImU32 Accent     = IM_COL32(74, 144, 255, 255);
    static constexpr ImU32 AccentSoft = IM_COL32(74, 144, 255, 70);
    static constexpr ImU32 Border32   = IM_COL32(57, 60, 70, 255);
    static constexpr ImU32 ViewportBg = IM_COL32(17, 18, 22, 255);
    static constexpr ImU32 GridMinor  = IM_COL32(35, 37, 44, 255);
    static constexpr ImU32 GridMajor  = IM_COL32(52, 55, 64, 255);

    static const ImVec4 DimTextV  = ImVec4(0.478f, 0.502f, 0.549f, 1.0f);
    static const ImVec4 WarnTextV = ImVec4(1.000f, 0.620f, 0.240f, 1.0f);
}

struct FEmitterModuleEntry
{
    const char*      Name;
    UParticleModule* Module;
};

enum class EParticleCurveDomain : uint8
{
    EmitterTime,
    RelativeParticleTime,
    BeamStep,
    DistanceAlongBeam,
    Unknown,
};

enum class EParticleCurvePropertyKind : uint8
{
    Float,
    VectorXYZ,
    VectorRGB,
};

struct FParticleCurveProperty
{
    FString Name;
    FRawDistributionFloat* FloatRaw = nullptr;
    FRawDistributionVector* VectorRaw = nullptr;
    UParticleModule* OwnerModule = nullptr;
    EParticleCurvePropertyKind Kind = EParticleCurvePropertyKind::Float;
    EParticleCurveDomain Domain = EParticleCurveDomain::Unknown;
};

struct FParticleCurveTrack
{
    FString Name;
    FFloatCurve* Curve = nullptr;
    UParticleModule* OwnerModule = nullptr;
    UObject* OwnerDistribution = nullptr;
    int32 ChannelIndex = 0;
    ImU32 Color = IM_COL32(80, 220, 120, 255);
    EParticleCurveDomain Domain = EParticleCurveDomain::Unknown;
};

const char* ParticleCurveDomainName(EParticleCurveDomain Domain);
void CollectParticleCurveProperties(UParticleModule* Module, TArray<FParticleCurveProperty>& OutProperties);
void CollectParticleCurveTracks(UParticleModule* Module, TArray<FParticleCurveTrack>& OutTracks);
bool IsParticleCurvePropertyActive(const FParticleCurveProperty& Property);
bool ConvertParticleCurvePropertyToCurve(const FParticleCurveProperty& Property);
bool ParticleModuleHasCurveProperties(UParticleModule* Module);
bool ParticleModuleHasActiveCurveTracks(UParticleModule* Module);

void DrawRawDistributionVector(const char* Label, FRawDistributionVector& Raw, bool& bChanged, UObject* Outer, bool bAllowCurve = true);
void DrawRawDistributionFloat(const char* Label, FRawDistributionFloat& Raw, bool& bChanged, UObject* Outer, bool bAllowCurve = true);

FParticleBeam2EmitterInstance* GetPreviewBeamInstance(UParticleSystemComponent* PreviewPSC, int32 EmitterIndex);
FVector& GetPreviewBeamUserSetPoint(
    std::unordered_map<const UParticleModule*, FVector>& Points,
    const UParticleModule* Module,
    UParticleSystemComponent* PreviewPSC,
    int32 EmitterIndex,
    bool bSource
);
void ApplyPreviewBeamUserSetPoint(UParticleSystemComponent* PreviewPSC, int32 EmitterIndex, bool bSource, const FVector& Point);

extern std::unordered_map<const UParticleModule*, FVector> GPreviewBeamUserSetSourcePoints;
extern std::unordered_map<const UParticleModule*, FVector> GPreviewBeamUserSetTargetPoints;

float Clamp01(float V, float Lo, float Hi);
const char* MaterialDomainName(EMaterialDomain Domain);
const char* GetModuleDisplayName(const UParticleModule* Module);
ImU32 GetModuleCategoryColor(const UParticleModule* Module);
void BuildEmitterModuleListAt(UParticleEmitter* Emitter, int32 LODIndex, TArray<FEmitterModuleEntry>& OutList);
void BuildEmitterModuleList(UParticleEmitter* Emitter, TArray<FEmitterModuleEntry>& OutList);
int32 GetParticleSystemLODCount(UParticleSystem* ParticleSystem);
void SyncParticleSystemLODDistances(UParticleSystem* ParticleSystem);
bool IsModuleSharedWithHigher(UParticleEmitter* Emitter, int32 LODIndex, int32 ModuleIndex);
UParticleModule* CloneParticleModule(UParticleModule* Source, UObject* Outer);
const char* ScreenAlignmentName(EParticleScreenAlignment V);
const char* SortModeName(EParticleSortMode V);

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

bool HasBeamModifierOfType(UParticleLODLevel* LOD, BeamModifierType Type);

void PanelHeader(const char* Title, const char* Context = nullptr);
bool BeginPanel(const char* StrId, const char* Title, float Width, float Height, const char* Context = nullptr);
void EndPanel();
void Splitter(const char* StrId, bool bVertical, float FullExtent, float CrossExtent, float& Ratio);
void CanvasHint(ImDrawList* DrawList, const ImVec2& Min, const ImVec2& Max, const char* Text);
ID3D11ShaderResourceView* LoadToolIcon(const wchar_t* FileName);
FString MakeMaterialGuid();
FString SanitizeFileStem(FString Stem);
std::filesystem::path ToProjectPath(const FString& Path);
std::filesystem::path BuildUniqueMaterialPath(const std::filesystem::path& Directory, const FString& Stem);
bool IconToolButton(
    const char*               Id,
    ID3D11ShaderResourceView* Icon,
    const char*               FallbackLabel,
    const char*               Tooltip,
    bool                      bEnabled,
    float                     IconSize
);
bool WriteBmp24(const char* Path, uint32 W, uint32 H, const uint8* RGBA, uint32 RowPitch);
