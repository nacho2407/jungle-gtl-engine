#include "ParticleSystemEditorWidget.h"
#include "ParticleSystemEditorPrivate.h"

// =============================================================================
// Particle system editor private helpers
// =============================================================================

// -----------------------------------------------------------------------------
// Generic panel and icon UI helpers
// -----------------------------------------------------------------------------

    float Clamp01(float V, float Lo, float Hi)
    {
        return V < Lo ? Lo : (V > Hi ? Hi : V);
    }

    void PanelHeader(const char* Title, const char* Context)
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

    bool BeginPanel(const char* StrId, const char* Title, float Width, float Height, const char* Context)
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

    ID3D11ShaderResourceView* LoadToolIcon(const wchar_t* FileName)
    {
        const std::wstring Wide = FPaths::Combine(FPaths::AssetDir(), L"Editor/ToolIcons/") + FileName;
        return FEditorTextureManager::Get().GetOrLoadIcon(FPaths::ToUtf8(Wide));
    }

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

// -----------------------------------------------------------------------------
// Module list and LOD helper functions
// -----------------------------------------------------------------------------

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

// -----------------------------------------------------------------------------
// Distribution property controls
// -----------------------------------------------------------------------------

namespace
{
    void InitializeCurve(FFloatCurve& Curve, float InitialValue)
    {
        Curve.Reset();
        Curve.DefaultValue = InitialValue;
        Curve.AddKey(0.0f, InitialValue);
        Curve.AddKey(1.0f, InitialValue);
        Curve.SortKeys();
        Curve.AutoSetTangents();
    }

    const char* GetFloatDistributionTypeName(UDistributionFloat* Distribution)
    {
        if (Cast<UDistributionFloatConstant>(Distribution)) return "Constant";
        if (Cast<UDistributionFloatUniform>(Distribution)) return "Uniform";
        if (Cast<UDistributionFloatCurve>(Distribution)) return "Curve";
        return "None";
    }

    const char* GetVectorDistributionTypeName(UDistributionVector* Distribution)
    {
        if (Cast<UDistributionVectorConstant>(Distribution)) return "Constant";
        if (Cast<UDistributionVectorUniform>(Distribution)) return "Uniform";
        if (Cast<UDistributionVectorCurve>(Distribution)) return "Curve";
        return "None";
    }
}

void DrawRawDistributionVector(const char* Label, FRawDistributionVector& Raw, bool& bChanged, UObject* Outer, bool bAllowCurve)
{
    if (ImGui::TreeNode(Label))
    {
        const char* TypeStr = GetVectorDistributionTypeName(Raw.Distribution);

        if (ImGui::BeginCombo("Type", TypeStr))
        {
            if (ImGui::Selectable("Constant", TypeStr == "Constant"))
            {
                const FVector CurrentValue = Raw.Distribution ? Raw.Distribution->GetValue(0.0f) : FVector::ZeroVector;
                UDistributionVectorConstant* NewDistribution = UObjectManager::Get().CreateObject<UDistributionVectorConstant>(Outer);
                NewDistribution->Constant = CurrentValue;
                Raw.Distribution = NewDistribution;
                bChanged = true;
            }
            if (ImGui::Selectable("Uniform", TypeStr == "Uniform"))
            {
                const FVector CurrentValue = Raw.Distribution ? Raw.Distribution->GetValue(0.0f) : FVector::ZeroVector;
                UDistributionVectorUniform* NewDistribution = UObjectManager::Get().CreateObject<UDistributionVectorUniform>(Outer);
                NewDistribution->Min = CurrentValue;
                NewDistribution->Max = CurrentValue;
                Raw.Distribution = NewDistribution;
                bChanged = true;
            }
            if (bAllowCurve)
            {
                if (ImGui::Selectable("Curve", TypeStr == "Curve"))
                {
                    const FVector CurrentValue = Raw.Distribution ? Raw.Distribution->GetValue(0.0f) : FVector::ZeroVector;
                    UDistributionVectorCurve* NewDistribution = UObjectManager::Get().CreateObject<UDistributionVectorCurve>(Outer);
                    InitializeCurve(NewDistribution->X, CurrentValue.X);
                    InitializeCurve(NewDistribution->Y, CurrentValue.Y);
                    InitializeCurve(NewDistribution->Z, CurrentValue.Z);
                    Raw.Distribution = NewDistribution;
                    bChanged = true;
                }
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
            bChanged |= ImGui::Checkbox("Lock Axes", &Uniform->bLockAxes);
        }
        else if (UDistributionVectorCurve* Curve = Cast<UDistributionVectorCurve>(Raw.Distribution))
        {
            if (!bAllowCurve)
            {
                ImGui::TextColored(PSE::WarnTextV, "Curve is not supported for this field.");
                ImGui::TextColored(PSE::DimTextV, "Use Constant or Uniform for initial-only values.");
            }
            else
            {
                ImGui::TextColored(PSE::DimTextV, "Edit in the Curve Editor panel.");
            }
            ImGui::TextColored(
                PSE::DimTextV,
                "Keys: X %d / Y %d / Z %d",
                static_cast<int32>(Curve->X.Keys.size()),
                static_cast<int32>(Curve->Y.Keys.size()),
                static_cast<int32>(Curve->Z.Keys.size()));
        }
        ImGui::TreePop();
    }
}

void DrawRawDistributionFloat(const char* Label, FRawDistributionFloat& Raw, bool& bChanged, UObject* Outer, bool bAllowCurve)
{
    if (ImGui::TreeNode(Label))
    {
        const char* TypeStr = GetFloatDistributionTypeName(Raw.Distribution);

        if (ImGui::BeginCombo("Type", TypeStr))
        {
            if (ImGui::Selectable("Constant", TypeStr == "Constant"))
            {
                const float CurrentValue = Raw.Distribution ? Raw.Distribution->GetValue(0.0f) : 0.0f;
                UDistributionFloatConstant* NewDistribution = UObjectManager::Get().CreateObject<UDistributionFloatConstant>(Outer);
                NewDistribution->Constant = CurrentValue;
                Raw.Distribution = NewDistribution;
                bChanged = true;
            }
            if (ImGui::Selectable("Uniform", TypeStr == "Uniform"))
            {
                const float CurrentValue = Raw.Distribution ? Raw.Distribution->GetValue(0.0f) : 0.0f;
                UDistributionFloatUniform* NewDistribution = UObjectManager::Get().CreateObject<UDistributionFloatUniform>(Outer);
                NewDistribution->Min = CurrentValue;
                NewDistribution->Max = CurrentValue;
                Raw.Distribution = NewDistribution;
                bChanged = true;
            }
            if (bAllowCurve)
            {
                if (ImGui::Selectable("Curve", TypeStr == "Curve"))
                {
                    const float CurrentValue = Raw.Distribution ? Raw.Distribution->GetValue(0.0f) : 0.0f;
                    UDistributionFloatCurve* NewDistribution = UObjectManager::Get().CreateObject<UDistributionFloatCurve>(Outer);
                    InitializeCurve(NewDistribution->Curve, CurrentValue);
                    Raw.Distribution = NewDistribution;
                    bChanged = true;
                }
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
        else if (UDistributionFloatCurve* Curve = Cast<UDistributionFloatCurve>(Raw.Distribution))
        {
            if (!bAllowCurve)
            {
                ImGui::TextColored(PSE::WarnTextV, "Curve is not supported for this field.");
                ImGui::TextColored(PSE::DimTextV, "Use Constant or Uniform for initial-only values.");
            }
            else
            {
                ImGui::TextColored(PSE::DimTextV, "Edit in the Curve Editor panel.");
            }
            ImGui::TextColored(PSE::DimTextV, "Keys: %d", static_cast<int32>(Curve->Curve.Keys.size()));
        }
        ImGui::TreePop();
    }
}

// -----------------------------------------------------------------------------
// Curve-capable distribution discovery and binding
// -----------------------------------------------------------------------------

namespace
{
    static const ImU32 CurveColorX = IM_COL32(214, 90, 90, 255);
    static const ImU32 CurveColorY = IM_COL32(96, 196, 96, 255);
    static const ImU32 CurveColorZ = IM_COL32(96, 140, 226, 255);
    static const ImU32 CurveColorFloat = IM_COL32(255, 190, 80, 255);

    void InitializeParticleCurve(FFloatCurve& Curve, float InitialValue)
    {
        Curve.Reset();
        Curve.DefaultValue = InitialValue;
        Curve.AddKey(0.0f, InitialValue);
        Curve.AddKey(1.0f, InitialValue);
        Curve.SortKeys();
        Curve.AutoSetTangents();
    }

    void AddFloatProperty(
        const char* Label,
        FRawDistributionFloat& Raw,
        UParticleModule* OwnerModule,
        EParticleCurveDomain Domain,
        TArray<FParticleCurveProperty>& OutProperties)
    {
        FParticleCurveProperty Property;
        Property.Name = Label;
        Property.FloatRaw = &Raw;
        Property.OwnerModule = OwnerModule;
        Property.Kind = EParticleCurvePropertyKind::Float;
        Property.Domain = Domain;
        OutProperties.push_back(Property);
    }

    void AddVectorProperty(
        const char* Label,
        FRawDistributionVector& Raw,
        UParticleModule* OwnerModule,
        EParticleCurveDomain Domain,
        EParticleCurvePropertyKind Kind,
        TArray<FParticleCurveProperty>& OutProperties)
    {
        FParticleCurveProperty Property;
        Property.Name = Label;
        Property.VectorRaw = &Raw;
        Property.OwnerModule = OwnerModule;
        Property.Kind = Kind;
        Property.Domain = Domain;
        OutProperties.push_back(Property);
    }

    void AddFloatCurveTrack(
        const FParticleCurveProperty& Property,
        TArray<FParticleCurveTrack>& OutTracks)
    {
        if (!Property.FloatRaw)
        {
            return;
        }

        UDistributionFloatCurve* CurveDistribution = Cast<UDistributionFloatCurve>(Property.FloatRaw->Distribution);
        if (!CurveDistribution)
        {
            return;
        }

        FParticleCurveTrack Track;
        Track.Name = Property.Name;
        Track.Curve = &CurveDistribution->Curve;
        Track.OwnerModule = Property.OwnerModule;
        Track.OwnerDistribution = CurveDistribution;
        Track.ChannelIndex = 0;
        Track.Color = CurveColorFloat;
        Track.Domain = Property.Domain;
        OutTracks.push_back(Track);
    }

    void AddVectorCurveTrack(
        const FParticleCurveProperty& Property,
        const char* Suffix,
        FFloatCurve& Curve,
        UDistributionVectorCurve* CurveDistribution,
        int32 ChannelIndex,
        ImU32 Color,
        TArray<FParticleCurveTrack>& OutTracks)
    {
        FParticleCurveTrack Track;
        Track.Name = Property.Name + FString(".") + Suffix;
        Track.Curve = &Curve;
        Track.OwnerModule = Property.OwnerModule;
        Track.OwnerDistribution = CurveDistribution;
        Track.ChannelIndex = ChannelIndex;
        Track.Color = Color;
        Track.Domain = Property.Domain;
        OutTracks.push_back(Track);
    }

    void AddVectorCurveTracks(
        const FParticleCurveProperty& Property,
        TArray<FParticleCurveTrack>& OutTracks)
    {
        if (!Property.VectorRaw)
        {
            return;
        }

        UDistributionVectorCurve* CurveDistribution = Cast<UDistributionVectorCurve>(Property.VectorRaw->Distribution);
        if (!CurveDistribution)
        {
            return;
        }

        const bool bColor = Property.Kind == EParticleCurvePropertyKind::VectorRGB;
        AddVectorCurveTrack(Property, bColor ? "R" : "X", CurveDistribution->X, CurveDistribution, 0, CurveColorX, OutTracks);
        AddVectorCurveTrack(Property, bColor ? "G" : "Y", CurveDistribution->Y, CurveDistribution, 1, CurveColorY, OutTracks);
        AddVectorCurveTrack(Property, bColor ? "B" : "Z", CurveDistribution->Z, CurveDistribution, 2, CurveColorZ, OutTracks);
    }
}

const char* ParticleCurveDomainName(EParticleCurveDomain Domain)
{
    switch (Domain)
    {
    case EParticleCurveDomain::EmitterTime:
        return "Emitter Time";
    case EParticleCurveDomain::RelativeParticleTime:
        return "Particle Relative Time";
    case EParticleCurveDomain::BeamStep:
        return "Beam Step";
    case EParticleCurveDomain::DistanceAlongBeam:
        return "Distance Along Beam";
    default:
        return "Unknown";
    }
}

void CollectParticleCurveProperties(UParticleModule* Module, TArray<FParticleCurveProperty>& OutProperties)
{
    OutProperties.clear();
    if (!Module)
    {
        return;
    }

    // Keep this list aligned with the distributions that the Details panel actually exposes.
    // If a field is hidden in Details, it should not appear only in the curve panel.
    if (UParticleModuleLocation* Location = Cast<UParticleModuleLocation>(Module))
    {
        AddVectorProperty("Start Location", Location->StartLocation, Module, EParticleCurveDomain::EmitterTime, EParticleCurvePropertyKind::VectorXYZ, OutProperties);
    }
    else if (UParticleModuleVelocity* Velocity = Cast<UParticleModuleVelocity>(Module))
    {
        AddVectorProperty("Start Velocity", Velocity->StartVelocity, Module, EParticleCurveDomain::EmitterTime, EParticleCurvePropertyKind::VectorXYZ, OutProperties);
        AddFloatProperty("Start Velocity Radial", Velocity->StartVelocityRadial, Module, EParticleCurveDomain::EmitterTime, OutProperties);
    }
    else if (UParticleModuleSize* Size = Cast<UParticleModuleSize>(Module))
    {
        AddVectorProperty("Start Size", Size->StartSize, Module, EParticleCurveDomain::EmitterTime, EParticleCurvePropertyKind::VectorXYZ, OutProperties);
    }
    else if (UParticleModuleColorOverLife* ColorOverLife = Cast<UParticleModuleColorOverLife>(Module))
    {
        AddVectorProperty("Color Over Life", ColorOverLife->ColorOverLife, Module, EParticleCurveDomain::RelativeParticleTime, EParticleCurvePropertyKind::VectorRGB, OutProperties);
        AddFloatProperty("Alpha Over Life", ColorOverLife->AlphaOverLife, Module, EParticleCurveDomain::RelativeParticleTime, OutProperties);
    }
    else if (Cast<UParticleModuleColor>(Module))
    {
        // Initial Color writes BaseColor once during Spawn. It is intentionally not listed here.
        // Time-varying particle color belongs to UParticleModuleColorOverLife.
    }
    else if (UParticleModuleMeshRotation* MeshRotation = Cast<UParticleModuleMeshRotation>(Module))
    {
        AddVectorProperty("Start Rotation", MeshRotation->StartRotation, Module, EParticleCurveDomain::EmitterTime, EParticleCurvePropertyKind::VectorXYZ, OutProperties);
    }
    else if (UParticleModuleMeshRotationRate* MeshRotationRate = Cast<UParticleModuleMeshRotationRate>(Module))
    {
        AddVectorProperty("Start Rotation Rate", MeshRotationRate->StartRotationRate, Module, EParticleCurveDomain::EmitterTime, EParticleCurvePropertyKind::VectorXYZ, OutProperties);
    }
    else if (UParticleModuleTrailSource* TrailSource = Cast<UParticleModuleTrailSource>(Module))
    {
        AddFloatProperty("Source Strength", TrailSource->SourceStrength, Module, EParticleCurveDomain::EmitterTime, OutProperties);
    }
    else if (UParticleModuleBeamSource* BeamSource = Cast<UParticleModuleBeamSource>(Module))
    {
        AddVectorProperty("Source", BeamSource->Source, Module, EParticleCurveDomain::EmitterTime, EParticleCurvePropertyKind::VectorXYZ, OutProperties);
        AddVectorProperty("Source Tangent", BeamSource->SourceTangent, Module, EParticleCurveDomain::RelativeParticleTime, EParticleCurvePropertyKind::VectorXYZ, OutProperties);
        AddFloatProperty("Source Strength", BeamSource->SourceStrength, Module, EParticleCurveDomain::RelativeParticleTime, OutProperties);
    }
    else if (UParticleModuleBeamTarget* BeamTarget = Cast<UParticleModuleBeamTarget>(Module))
    {
        AddVectorProperty("Target", BeamTarget->Target, Module, EParticleCurveDomain::EmitterTime, EParticleCurvePropertyKind::VectorXYZ, OutProperties);
        AddVectorProperty("Target Tangent", BeamTarget->TargetTangent, Module, EParticleCurveDomain::RelativeParticleTime, EParticleCurvePropertyKind::VectorXYZ, OutProperties);
        AddFloatProperty("Target Strength", BeamTarget->TargetStrength, Module, EParticleCurveDomain::RelativeParticleTime, OutProperties);
    }
    else if (UParticleModuleBeamNoise* BeamNoise = Cast<UParticleModuleBeamNoise>(Module))
    {
        const EParticleCurveDomain NoiseRangeScaleDomain = BeamNoise->bNRScaleEmitterTime
            ? EParticleCurveDomain::EmitterTime
            : EParticleCurveDomain::RelativeParticleTime;
        AddVectorProperty("Noise Range", BeamNoise->NoiseRange, Module, EParticleCurveDomain::EmitterTime, EParticleCurvePropertyKind::VectorXYZ, OutProperties);
        AddFloatProperty("Noise Range Scale", BeamNoise->NoiseRangeScale, Module, NoiseRangeScaleDomain, OutProperties);
        AddVectorProperty("Noise Speed", BeamNoise->NoiseSpeed, Module, EParticleCurveDomain::EmitterTime, EParticleCurvePropertyKind::VectorXYZ, OutProperties);
        AddFloatProperty("Noise Tangent Strength", BeamNoise->NoiseTangentStrength, Module, EParticleCurveDomain::EmitterTime, OutProperties);
        AddFloatProperty("Noise Scale", BeamNoise->NoiseScale, Module, EParticleCurveDomain::DistanceAlongBeam, OutProperties);
    }
    else if (UParticleModuleTypeDataBeam2* BeamType = Cast<UParticleModuleTypeDataBeam2>(Module))
    {
        AddFloatProperty("Distance", BeamType->Distance, Module, EParticleCurveDomain::EmitterTime, OutProperties);
        AddFloatProperty("Taper Factor", BeamType->TaperFactor, Module, EParticleCurveDomain::BeamStep, OutProperties);
        AddFloatProperty("Taper Scale", BeamType->TaperScale, Module, EParticleCurveDomain::BeamStep, OutProperties);
    }
}

void CollectParticleCurveTracks(UParticleModule* Module, TArray<FParticleCurveTrack>& OutTracks)
{
    OutTracks.clear();

    TArray<FParticleCurveProperty> Properties;
    CollectParticleCurveProperties(Module, Properties);
    for (const FParticleCurveProperty& Property : Properties)
    {
        if (Property.FloatRaw)
        {
            AddFloatCurveTrack(Property, OutTracks);
        }
        else if (Property.VectorRaw)
        {
            AddVectorCurveTracks(Property, OutTracks);
        }
    }
}

bool IsParticleCurvePropertyActive(const FParticleCurveProperty& Property)
{
    if (Property.FloatRaw)
    {
        return Cast<UDistributionFloatCurve>(Property.FloatRaw->Distribution) != nullptr;
    }
    if (Property.VectorRaw)
    {
        return Cast<UDistributionVectorCurve>(Property.VectorRaw->Distribution) != nullptr;
    }
    return false;
}

bool ConvertParticleCurvePropertyToCurve(const FParticleCurveProperty& Property)
{
    if (!Property.OwnerModule)
    {
        return false;
    }

    if (Property.FloatRaw)
    {
        const float CurrentValue = Property.FloatRaw->Distribution
            ? Property.FloatRaw->Distribution->GetValue(0.0f)
            : 0.0f;

        UDistributionFloatCurve* NewDistribution = UObjectManager::Get().CreateObject<UDistributionFloatCurve>(Property.OwnerModule);
        if (!NewDistribution)
        {
            return false;
        }

        InitializeParticleCurve(NewDistribution->Curve, CurrentValue);
        Property.FloatRaw->Distribution = NewDistribution;
        return true;
    }

    if (Property.VectorRaw)
    {
        const FVector CurrentValue = Property.VectorRaw->Distribution
            ? Property.VectorRaw->Distribution->GetValue(0.0f)
            : FVector::ZeroVector;

        UDistributionVectorCurve* NewDistribution = UObjectManager::Get().CreateObject<UDistributionVectorCurve>(Property.OwnerModule);
        if (!NewDistribution)
        {
            return false;
        }

        InitializeParticleCurve(NewDistribution->X, CurrentValue.X);
        InitializeParticleCurve(NewDistribution->Y, CurrentValue.Y);
        InitializeParticleCurve(NewDistribution->Z, CurrentValue.Z);
        Property.VectorRaw->Distribution = NewDistribution;
        return true;
    }

    return false;
}

bool ParticleModuleHasCurveProperties(UParticleModule* Module)
{
    TArray<FParticleCurveProperty> Properties;
    CollectParticleCurveProperties(Module, Properties);
    return !Properties.empty();
}

bool ParticleModuleHasActiveCurveTracks(UParticleModule* Module)
{
    TArray<FParticleCurveTrack> Tracks;
    CollectParticleCurveTracks(Module, Tracks);
    return !Tracks.empty();
}

// -----------------------------------------------------------------------------
// Beam preview helper functions
// -----------------------------------------------------------------------------

    std::unordered_map<const UParticleModule*, FVector> GPreviewBeamUserSetSourcePoints;
    std::unordered_map<const UParticleModule*, FVector> GPreviewBeamUserSetTargetPoints;

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

// -----------------------------------------------------------------------------
// Material duplication/path helpers
// -----------------------------------------------------------------------------

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

// -----------------------------------------------------------------------------
// Thumbnail BMP writing helpers
// -----------------------------------------------------------------------------

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

