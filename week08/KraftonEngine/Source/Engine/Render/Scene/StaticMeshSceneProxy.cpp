#include "Render/Scene/StaticMeshSceneProxy.h"
#include "Component/StaticMeshComponent.h"
#include "Render/Resource/ShaderManager.h"
#include "Mesh/StaticMesh.h"
#include "Mesh/StaticMeshAsset.h"
#include "Materials/Material.h"
#include "Materials/MaterialSemantics.h"
#include "Texture/Texture2D.h"
#include "Engine/Runtime/Engine.h"
#include "Render/Renderer/Renderer.h"

#include <algorithm>
#include <initializer_list>
#include <memory>

namespace
{
struct FStaticMeshMaterialViewConstants
{
    FVector4 SectionColor = MaterialSemantics::GetDefaultSectionColor();
    FVector4 MaterialParam = FVector4(MaterialSemantics::DefaultSpecularPower, MaterialSemantics::DefaultSpecularStrength, 0.0f, 1.0f);
    uint32 HasBaseTexture = 0;
    uint32 HasNormalTexture = 0;
    float Padding[2] = { 0.0f, 0.0f };
};

bool SectionMaterialLess(const FMeshSectionDraw& A, const FMeshSectionDraw& B)
{
    const uintptr_t ACB0 = reinterpret_cast<uintptr_t>(A.MaterialCB[0]);
    const uintptr_t BCB0 = reinterpret_cast<uintptr_t>(B.MaterialCB[0]);
    if (ACB0 != BCB0)
    {
        return ACB0 < BCB0;
    }

    const uintptr_t ACB1 = reinterpret_cast<uintptr_t>(A.MaterialCB[1]);
    const uintptr_t BCB1 = reinterpret_cast<uintptr_t>(B.MaterialCB[1]);
    if (ACB1 != BCB1)
    {
        return ACB1 < BCB1;
    }

    const uintptr_t ABaseSRV = reinterpret_cast<uintptr_t>(A.DiffuseSRV);
    const uintptr_t BBaseSRV = reinterpret_cast<uintptr_t>(B.DiffuseSRV);
    if (ABaseSRV != BBaseSRV)
    {
        return ABaseSRV < BBaseSRV;
    }

    const uintptr_t ANormalSRV = reinterpret_cast<uintptr_t>(A.NormalSRV);
    const uintptr_t BNormalSRV = reinterpret_cast<uintptr_t>(B.NormalSRV);
    if (ANormalSRV != BNormalSRV)
    {
        return ANormalSRV < BNormalSRV;
    }

    return A.FirstIndex < B.FirstIndex;
}

bool TryGetTextureSRV(UMaterial* Material, std::initializer_list<const char*> SlotNames, ID3D11ShaderResourceView*& OutSRV)
{
    OutSRV = nullptr;
    if (!Material)
    {
        return false;
    }

    for (const char* SlotName : SlotNames)
    {
        UTexture2D* Texture = nullptr;
        if (Material->GetTextureParameter(SlotName, Texture) && Texture)
        {
            OutSRV = Texture->GetSRV();
            if (OutSRV)
            {
                return true;
            }
        }
    }

    return false;
}

float GetScalarOrDefault(const UMaterial* Material, const char* ParamName, float DefaultValue)
{
    if (!Material)
    {
        return DefaultValue;
    }

    float Value = DefaultValue;
    return Material->GetScalarParameter(ParamName, Value) ? Value : DefaultValue;
}

FVector4 GetVector4OrDefault(const UMaterial* Material, const char* ParamName, const FVector4& DefaultValue)
{
    if (!Material)
    {
        return DefaultValue;
    }

    FVector4 Value = DefaultValue;
    return Material->GetVector4Parameter(ParamName, Value) ? Value : DefaultValue;
}

std::unique_ptr<FMaterialConstantBuffer> BuildStaticMeshMaterialCB(const UMaterial* Material, ID3D11Device* Device, ID3D11DeviceContext* Context,
                                                                   ID3D11ShaderResourceView* DiffuseSRV, ID3D11ShaderResourceView* NormalSRV)
{
    if (!Device || !Context)
    {
        return nullptr;
    }

    auto Buffer = std::make_unique<FMaterialConstantBuffer>();
    Buffer->Init(Device, sizeof(FStaticMeshMaterialViewConstants), ECBSlot::PerShader0);

    FStaticMeshMaterialViewConstants Constants;
    Constants.SectionColor = GetVector4OrDefault(Material, MaterialSemantics::SectionColorParameter, MaterialSemantics::GetDefaultSectionColor());
    Constants.MaterialParam = FVector4(
        GetScalarOrDefault(Material, MaterialSemantics::SpecularPowerParameter, MaterialSemantics::DefaultSpecularPower),
        GetScalarOrDefault(Material, MaterialSemantics::SpecularStrengthParameter, MaterialSemantics::DefaultSpecularStrength),
        0.0f,
        1.0f);
    Constants.HasBaseTexture = DiffuseSRV ? 1u : 0u;
    Constants.HasNormalTexture = NormalSRV ? 1u : 0u;

    Buffer->SetData(&Constants, sizeof(Constants));
    Buffer->Upload(Context);
    return Buffer;
}

void SortSectionDrawsByMaterial(TArray<FMeshSectionDraw>& Draws)
{
    if (Draws.size() > 1)
    {
        std::sort(Draws.begin(), Draws.end(), SectionMaterialLess);
    }
}
} // namespace

FStaticMeshSceneProxy::FStaticMeshSceneProxy(UStaticMeshComponent* InComponent)
    : FPrimitiveSceneProxy(InComponent)
{
}

UStaticMeshComponent* FStaticMeshSceneProxy::GetStaticMeshComponent() const
{
    return static_cast<UStaticMeshComponent*>(Owner);
}

void FStaticMeshSceneProxy::UpdateMaterial()
{
    RebuildSectionDraws();
}

void FStaticMeshSceneProxy::UpdateMesh()
{
    MeshBuffer = Owner->GetMeshBuffer();
    Shader = FShaderManager::Get().GetShader(EShaderType::StaticMesh);
    Pass = ERenderPass::Opaque;

    RebuildSectionDraws();
}

void FStaticMeshSceneProxy::UpdateLOD(uint32 LODLevel)
{
    if (LODLevel >= LODCount)
        LODLevel = LODCount - 1;
    if (LODLevel == CurrentLOD)
        return;

    std::swap(MeshBuffer, LODData[CurrentLOD].MeshBuffer);
    std::swap(SectionDraws, LODData[CurrentLOD].SectionDraws);
    std::swap(ActiveOwnedMaterialCBs, LODData[CurrentLOD].OwnedMaterialCBs);

    CurrentLOD = LODLevel;
    std::swap(MeshBuffer, LODData[LODLevel].MeshBuffer);
    std::swap(SectionDraws, LODData[LODLevel].SectionDraws);
    std::swap(ActiveOwnedMaterialCBs, LODData[LODLevel].OwnedMaterialCBs);
}

void FStaticMeshSceneProxy::RebuildSectionDraws()
{
    UStaticMeshComponent* SMC = GetStaticMeshComponent();
    UStaticMesh* Mesh = SMC->GetStaticMesh();
    if (!Mesh || !Mesh->GetStaticMeshAsset())
    {
        for (uint32 lod = 0; lod < MAX_LOD; ++lod)
        {
            LODData[lod].MeshBuffer = nullptr;
            LODData[lod].SectionDraws.clear();
            LODData[lod].OwnedMaterialCBs.clear();
        }

        LODCount = 1;
        CurrentLOD = 0;
        MeshBuffer = nullptr;
        SectionDraws.clear();
        ActiveOwnedMaterialCBs.clear();
        return;
    }

    ID3D11Device* Device = GEngine ? GEngine->GetRenderer().GetFD3DDevice().GetDevice() : nullptr;
    ID3D11DeviceContext* Context = GEngine ? GEngine->GetRenderer().GetFD3DDevice().GetDeviceContext() : nullptr;

    const auto& Slots = Mesh->GetStaticMaterials();
    const auto& Overrides = SMC->GetOverrideMaterials();
    LODCount = Mesh->GetLODCount();

    for (uint32 lod = 0; lod < LODCount; ++lod)
    {
        const auto& Sections = Mesh->GetLODSections(lod);
        LODData[lod].MeshBuffer = Mesh->GetLODMeshBuffer(lod);
        LODData[lod].SectionDraws.clear();
        LODData[lod].OwnedMaterialCBs.clear();
        LODData[lod].SectionDraws.reserve(Sections.size());
        LODData[lod].OwnedMaterialCBs.reserve(Sections.size());

        for (const FStaticMeshSection& Section : Sections)
        {
            FMeshSectionDraw Draw;
            Draw.FirstIndex = Section.FirstIndex;
            Draw.IndexCount = Section.NumTriangles * 3;
            Draw.Blend = EBlendState::Opaque;
            Draw.DepthStencil = EDepthStencilState::Default;
            Draw.Rasterizer = ERasterizerState::SolidBackCull;
            Draw.MaterialCB[0] = nullptr;
            Draw.MaterialCB[1] = nullptr;

            UMaterial* Mat = nullptr;
            const int32 MaterialIndex = Section.MaterialIndex;
            if (MaterialIndex >= 0 && MaterialIndex < static_cast<int32>(Slots.size()))
            {
                if (MaterialIndex < static_cast<int32>(Overrides.size()) && Overrides[MaterialIndex])
                {
                    Mat = Overrides[MaterialIndex];
                }
                else if (Slots[MaterialIndex].MaterialInterface)
                {
                    Mat = Slots[MaterialIndex].MaterialInterface;
                }
            }

            if (Mat)
            {
                TryGetTextureSRV(Mat, { MaterialSemantics::DiffuseTextureSlot, "BaseColorTexture", "AlbedoTexture", "BaseTexture", "DiffuseMap" }, Draw.DiffuseSRV);
                TryGetTextureSRV(Mat, { MaterialSemantics::NormalTextureSlot, "NormalMap", "NormalMapTexture", "BumpTexture", "BumpMap" }, Draw.NormalSRV);
                Draw.Blend = Mat->GetBlendState();
                Draw.DepthStencil = Mat->GetDepthStencilState();
                Draw.Rasterizer = Mat->GetRasterizerState();
            }

            auto MaterialCB = BuildStaticMeshMaterialCB(Mat, Device, Context, Draw.DiffuseSRV, Draw.NormalSRV);
            if (MaterialCB)
            {
                Draw.MaterialCB[0] = MaterialCB->GetConstantBuffer();
                LODData[lod].OwnedMaterialCBs.push_back(std::move(MaterialCB));
            }

            LODData[lod].SectionDraws.push_back(Draw);
        }

        SortSectionDrawsByMaterial(LODData[lod].SectionDraws);
    }

    CurrentLOD = 0;
    std::swap(MeshBuffer, LODData[0].MeshBuffer);
    std::swap(SectionDraws, LODData[0].SectionDraws);
    std::swap(ActiveOwnedMaterialCBs, LODData[0].OwnedMaterialCBs);
}
