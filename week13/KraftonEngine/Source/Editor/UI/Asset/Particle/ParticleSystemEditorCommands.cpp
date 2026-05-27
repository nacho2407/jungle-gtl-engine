#include "ParticleSystemEditorWidget.h"
#include "ParticleSystemEditorPrivate.h"

// =============================================================================
// Particle system editor commands and state operations
// =============================================================================

// -----------------------------------------------------------------------------
// Preview simulation and keyboard interaction
// -----------------------------------------------------------------------------

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

// -----------------------------------------------------------------------------
// Emitter commands
// -----------------------------------------------------------------------------

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

// -----------------------------------------------------------------------------
// Module commands
// -----------------------------------------------------------------------------

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

// -----------------------------------------------------------------------------
// LOD commands
// -----------------------------------------------------------------------------

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

void FParticleSystemEditorWidget::SelectLOD(int32 LODIndex)
{
    SelectedLODIndex    = (std::max)(0, LODIndex);
    SelectedModuleIndex = -1; // sub-LOD 전환 시 모듈 선택은 리셋 (구조가 다를 수 있음).
    SelectedCurveTrackIndex = -1;
    SelectedCurveTrackModule = nullptr;
    InlineCurveEditor.ResetSelection();
}

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

// -----------------------------------------------------------------------------
// Asset commands
// -----------------------------------------------------------------------------

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

void FParticleSystemEditorWidget::FindInContentBrowser()
{
    UParticleSystem* PS = GetParticleSystem();
    if (!PS) return;
    // OpenPopup 은 BeginPopup 과 같은 nesting level 에서 호출되어야 한다 — 메뉴/툴바
    // 깊숙한 곳에서 바로 OpenPopup 하면 안 잡힘. Render 루프에서 deferred 처리.
    bFindCBPopupRequested = true;
}

// -----------------------------------------------------------------------------
// Undo and redo
// -----------------------------------------------------------------------------

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

