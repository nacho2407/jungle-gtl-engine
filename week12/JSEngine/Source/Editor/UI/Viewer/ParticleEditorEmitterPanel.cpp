#include "ParticleEditorInternal.h"

using namespace ParticleEditorInternal;

void FParticleEditorViewerWidget::RenderEmitterPanel(FParticleEditorViewer* Viewer)
{
	UParticleSystem* ParticleSystem = Viewer->GetParticleSystem();
	DrawParticlePanelTitle("Emitters", "Modules");

	if (!ParticleSystem)
	{
		ImGui::TextDisabled("No particle system");
		return;
	}

	if (ImGui::IsWindowFocused() && Viewer->GetSelectedEmitterIndex() >= 0)
	{
		if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow))
		{
			const int32 Index = Viewer->GetSelectedEmitterIndex();
			Viewer->MoveEmitter(Index, Index - 1);
		}
		if (ImGui::IsKeyPressed(ImGuiKey_RightArrow))
		{
			const int32 Index = Viewer->GetSelectedEmitterIndex();
			Viewer->MoveEmitter(Index, Index + 1);
		}
	}

	const int32 EmitterCount = static_cast<int32>(ParticleSystem->Emitters.size());
	if (EmitterCount > 0)
	{
		ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(0.0f, ImGui::GetStyle().CellPadding.y));
		constexpr ImGuiTableFlags TableFlags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollX;

		if (ImGui::BeginTable("##ParticleEmitterColumns", EmitterCount, TableFlags))
		{
			for (int32 EmitterIndex = 0; EmitterIndex < EmitterCount; ++EmitterIndex)
			{
				ImGui::TableSetupColumn(("Emitter " + std::to_string(EmitterIndex)).c_str(), ImGuiTableColumnFlags_WidthFixed, EmitterNodeWidth + EmitterSeparatorGap);
			}
			ImGui::TableNextRow();
			for (int32 EmitterIndex = 0; EmitterIndex < EmitterCount; ++EmitterIndex)
			{
				ImGui::TableSetColumnIndex(EmitterIndex);
				ImGui::PushID(EmitterIndex);
				DrawEmitterNode(Viewer, EmitterIndex);
				ImGui::PopID();
			}
			ImGui::EndTable();
		}
		ImGui::PopStyleVar();
	}

	// 패널에 남은 빈 공간 전체를 덮는 투명 버튼 생성
	const ImVec2 Avail = ImGui::GetContentRegionAvail();
	if (Avail.x > 0.0f && Avail.y > 0.0f)
	{
		ImGui::InvisibleButton("##EmitterPanelEmptySpace", Avail);
		if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
		{
			ImGui::OpenPopup("ParticleEmitterPanelContext");
		}
	}

	if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
	{
		ImGui::OpenPopup("ParticleEmitterPanelContext");
	}

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 6.0f));
	if (ImGui::BeginPopup("ParticleEmitterPanelContext"))
	{
		RenderEmitterContextMenu(Viewer);
		ImGui::EndPopup();
	}
	ImGui::PopStyleVar(2);
}

void FParticleEditorViewerWidget::RenderEmitterContextMenu(FParticleEditorViewer* Viewer)
{
	bool bDeletedEmitter = false;
	bool bDeletedModule = false;
	if (ImGui::MenuItem("Duplicate Emitter", nullptr, false, HasDeletableSelectedEmitter(Viewer)))
	{
		TArray<int32> EmitterIndices = MultiSelectedEmitterIndices;
		if (EmitterIndices.empty() || !ContainsIndex(EmitterIndices, Viewer->GetSelectedEmitterIndex()))
		{
			EmitterIndices.clear();
			EmitterIndices.push_back(Viewer->GetSelectedEmitterIndex());
		}
		Viewer->CopyEmittersToIndex(EmitterIndices, Viewer->GetSelectedEmitterIndex() + 1);
		MultiSelectedEmitterIndices.clear();
	}
	if (ImGui::MenuItem("Delete Emitter", nullptr, false, HasDeletableSelectedEmitter(Viewer)))
	{
		Viewer->DeleteSelectedEmitter();
		bDeletedEmitter = true;
		MultiSelectedEmitterIndices.clear();
	}
	if (!bDeletedEmitter && ImGui::MenuItem("Delete Module", nullptr, false, HasDeletableSelectedModule(Viewer)))
	{
		Viewer->DeleteSelectedModule();
		bDeletedModule = true;
	}
	if (!bDeletedEmitter && !bDeletedModule && ImGui::MenuItem("Add Emitter"))
	{
		Viewer->AddEmitter();
	}
	const bool bCanAddModule =
		Viewer->GetSelectedEmitterIndex() >= 0 &&
		Viewer->GetSelectedLODLevel() != nullptr &&
		Viewer->CanEditSelectedLODTopology();
	if (!bDeletedEmitter && !bDeletedModule && Viewer->GetSelectedEmitterIndex() >= 0 && Viewer->GetSelectedLODLevel() != nullptr && ImGui::BeginMenu("Add Module", bCanAddModule))
	{
		DrawParticleModuleClassMenu(Viewer);
		ImGui::EndMenu();
	}
}

void FParticleEditorViewerWidget::DrawEmitterNode(FParticleEditorViewer* Viewer, int32 EmitterIndex)
{
	UParticleSystem* ParticleSystem = Viewer->GetParticleSystem();
	if (!ParticleSystem || EmitterIndex < 0 || EmitterIndex >= static_cast<int32>(ParticleSystem->Emitters.size()))
	{
		return;
	}

	UParticleEmitter* Emitter = ParticleSystem->Emitters[EmitterIndex];
	const int32 LODIndex = Viewer->GetSelectedEmitterIndex() == EmitterIndex && Viewer->GetSelectedLODIndex() >= 0
							   ? Viewer->GetSelectedLODIndex()
							   : 0;
	UParticleLODLevel* LOD = Emitter && LODIndex >= 0 && LODIndex < static_cast<int32>(Emitter->LODLevels.size())
								 ? Emitter->LODLevels[LODIndex]
								 : nullptr;
	const bool bPrimarySelected =
		Viewer->GetSelectedEmitterIndex() == EmitterIndex &&
		(Viewer->GetSelectionType() == EParticleEditorSelectionType::Emitter ||
		 Viewer->GetSelectionType() == EParticleEditorSelectionType::LODLevel ||
		 Viewer->GetSelectionType() == EParticleEditorSelectionType::RequiredModule ||
		 Viewer->GetSelectionType() == EParticleEditorSelectionType::SpawnModule ||
		 Viewer->GetSelectionType() == EParticleEditorSelectionType::TypeDataModule ||
		 Viewer->GetSelectionType() == EParticleEditorSelectionType::Module);
	const bool bMultiSelected = ContainsIndex(MultiSelectedEmitterIndices, EmitterIndex);
	const bool bSelected = bPrimarySelected || bMultiSelected;

	const ImVec2 CardStart = ImGui::GetCursorScreenPos();
	const float CardWidth = EmitterNodeWidth;
	const float HeaderHeight = 64.5f;
	const float HeaderPreviewSize = 52.0f;
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	const float SeparatorX = CardStart.x + CardWidth + EmitterSeparatorGap * 0.5f;
	const float SeparatorBottom = ImGui::GetWindowPos().y + ImGui::GetWindowHeight() - ImGui::GetStyle().WindowPadding.y;

	DrawList->AddLine(ImVec2(SeparatorX, CardStart.y), ImVec2(SeparatorX, SeparatorBottom), IM_COL32(58, 60, 68, 255), 1.0f);
	DrawList->AddRectFilled(CardStart, ImVec2(CardStart.x + CardWidth, CardStart.y + HeaderHeight), IM_COL32(33, 34, 38, 255), 0.0f);
	DrawList->AddRect(CardStart, ImVec2(CardStart.x + CardWidth, CardStart.y + HeaderHeight), IM_COL32(75, 75, 82, 255), 0.0f);

	auto HandleSelectionClick = [&](bool bRightClick)
	{
		if (bRightClick)
		{
			if (!ContainsIndex(MultiSelectedEmitterIndices, EmitterIndex))
			{
				MultiSelectedEmitterIndices.clear();
			}
			Viewer->SelectEmitter(EmitterIndex);
			Viewer->SelectLOD(LODIndex);
			ImGui::OpenPopup("EmitterHeaderContext");
		}
		else
		{
			if (ImGui::GetIO().KeyCtrl && ImGui::GetIO().KeyShift)
			{
				SeedEmitterMultiSelectionFromViewer(Viewer, MultiSelectedEmitterIndices);
				ToggleIndex(MultiSelectedEmitterIndices, EmitterIndex);
				Viewer->SelectEmitter(EmitterIndex);
			}
			else
			{
				MultiSelectedEmitterIndices.clear();
				Viewer->SelectEmitter(EmitterIndex);
			}
		}
	};

	ImGui::InvisibleButton("##EmitterHeader", ImVec2(CardWidth, 30.0f));
	if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
		HandleSelectionClick(false);
	if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
		HandleSelectionClick(true);

	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload(ParticleModuleDragPayload))
		{
			const FParticleModuleDragPayload* DragPayload = static_cast<const FParticleModuleDragPayload*>(Payload->Data);
			if (DragPayload)
			{
				Viewer->SelectEmitter(DragPayload->EmitterIndex);
				Viewer->SelectLOD(DragPayload->LODIndex);
				ApplyModulePayloadToEmitter(Viewer, *DragPayload, EmitterIndex);
			}
		}
		if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload(ParticleEmitterDragPayload))
		{
			const FParticleEmitterDragPayload* DragPayload = static_cast<const FParticleEmitterDragPayload*>(Payload->Data);
			if (DragPayload)
			{
				const TArray<int32> EmitterIndices = GetPayloadEmitterIndices(*DragPayload);
				if (ImGui::GetIO().KeyCtrl)
				{
					Viewer->CopyEmittersToIndex(EmitterIndices, EmitterIndex);
				}
				else
				{
					Viewer->MoveEmittersToIndex(EmitterIndices, EmitterIndex);
				}
				MultiSelectedEmitterIndices.clear();
			}
		}
		ImGui::EndDragDropTarget();
	}

	if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
	{
		FParticleEmitterDragPayload Payload;
		BuildEmitterPayload(Payload, EmitterIndex, MultiSelectedEmitterIndices);
		ImGui::SetDragDropPayload(ParticleEmitterDragPayload, &Payload, sizeof(Payload));
		ImGui::Text(Payload.Count > 1 ? "%d Emitters" : "Emitter %d", Payload.Count > 1 ? Payload.Count : EmitterIndex);
		ImGui::EndDragDropSource();
	}

	ImGui::SetCursorScreenPos(ImVec2(CardStart.x + 12.0f, CardStart.y + 11.0f));
	ImGui::Text("Emitter %d", EmitterIndex);
	ImGui::SetCursorScreenPos(ImVec2(CardStart.x + 12.0f, CardStart.y + 36.0f));
	bool bEnabled = LOD ? LOD->bEnabled : false;
	if (!LOD)
	{
		ImGui::BeginDisabled();
	}
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1.0f, 1.0f));
	const float EmitterHeaderControlSize = ImGui::GetFrameHeight();
	if (ImGui::Checkbox("##EmitterEnabled", &bEnabled) && LOD)
	{
		Viewer->CaptureUndoSnapshot("EditEmitterEnabled");
		LOD->bEnabled = bEnabled;
		Viewer->SelectEmitter(EmitterIndex);
		Viewer->SelectLOD(LODIndex);
		Viewer->MarkDirty();
		Viewer->RestartSimulation();
	}
	ImGui::PopStyleVar(2);
	if (!LOD)
	{
		ImGui::EndDisabled();
	}
	ImGui::SameLine(0.0f, 7.0f);
	bool bSolo = LOD ? LOD->bSolo : false;
	const ImVec2 SoloSize(EmitterHeaderControlSize, EmitterHeaderControlSize);
	if (!LOD)
	{
		ImGui::BeginDisabled();
	}
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
	ImGui::PushStyleColor(ImGuiCol_Button, bSolo ? ImVec4(0.22f, 0.39f, 0.54f, 1.0f) : ImVec4(0.16f, 0.17f, 0.20f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, bSolo ? ImVec4(0.28f, 0.49f, 0.67f, 1.0f) : ImVec4(0.22f, 0.24f, 0.29f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, bSolo ? ImVec4(0.18f, 0.33f, 0.48f, 1.0f) : ImVec4(0.18f, 0.30f, 0.42f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_Text, bSolo ? ImVec4(0.70f, 0.90f, 1.0f, 1.0f) : ImVec4(0.62f, 0.65f, 0.70f, 1.0f));
	if (ImGui::Button("S##EmitterSolo", SoloSize) && LOD)
	{
		Viewer->CaptureUndoSnapshot("EditEmitterSolo");
		const bool bNewSolo = !LOD->bSolo;
		if (UParticleSystem* ParticleSystem = Viewer->GetParticleSystem())
		{
			for (UParticleEmitter* OtherEmitter : ParticleSystem->Emitters)
			{
				if (!OtherEmitter)
				{
					continue;
				}
				for (UParticleLODLevel* OtherLOD : OtherEmitter->LODLevels)
				{
					if (OtherLOD)
					{
						OtherLOD->bSolo = false;
					}
				}
			}
		}
		LOD->bSolo = bNewSolo;
		Viewer->SelectEmitter(EmitterIndex);
		Viewer->SelectLOD(LODIndex);
		Viewer->MarkDirty();
		Viewer->RestartSimulation();
	}
	ImGui::PopStyleColor(4);
	ImGui::PopStyleVar(2);
	if (!LOD)
	{
		ImGui::EndDisabled();
	}
	ImGui::SetCursorScreenPos(ImVec2(CardStart.x + CardWidth - HeaderPreviewSize - 12.0f, CardStart.y + 6.0f));
	DrawEmitterPreview(ImVec2(HeaderPreviewSize, HeaderPreviewSize), EmitterIndex, bSelected);
	if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
		HandleSelectionClick(false);
	if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
		HandleSelectionClick(true);

	const ImVec2 HeaderEnd(CardStart.x + CardWidth, CardStart.y + HeaderHeight);
	const bool bHeaderHovered = ImGui::IsMouseHoveringRect(CardStart, HeaderEnd);
	const bool bHeaderControlHovered = ImGui::IsAnyItemHovered();
	if (bHeaderHovered && !bHeaderControlHovered)
	{
		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
			HandleSelectionClick(false);
		if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
			HandleSelectionClick(true);
	}

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 6.0f));
	if (ImGui::BeginPopup("EmitterHeaderContext"))
	{
		RenderEmitterContextMenu(Viewer);
		ImGui::EndPopup();
	}
	ImGui::PopStyleVar(2);

	ImGui::SetCursorScreenPos(ImVec2(CardStart.x, CardStart.y + HeaderHeight + 6.0f));

	const float ModulesStartY = ImGui::GetCursorScreenPos().y;
	const float ChildHeight = SeparatorBottom - ModulesStartY;

	if (ChildHeight > 0.0f)
	{
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 8.0f);

		ImGui::BeginChild("ModuleList", ImVec2(CardWidth, ChildHeight), false, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_AlwaysVerticalScrollbar);

		if (!LOD)
		{
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 8.0f);
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.0f);
			ImGui::TextDisabled("No LOD");
		}
		else
		{
			FParticleModuleRowContext RowContext;
			RowContext.Viewer = Viewer;
			RowContext.CurveSource.Type = &CurveState.Type;
			RowContext.CurveSource.EmitterIndex = &CurveState.EmitterIndex;
			RowContext.CurveSource.LODIndex = &CurveState.LODIndex;
			RowContext.CurveSource.ModuleIndex = &CurveState.ModuleIndex;
			RowContext.MultiSelection.ModuleIndices = &MultiSelectedModuleIndices;
			RowContext.MultiSelection.EmitterIndex = &MultiSelectedModuleEmitterIndex;
			RowContext.MultiSelection.LODIndex = &MultiSelectedModuleLODIndex;

			auto DrawModuleRow = [&](UObject* Mod, EParticleEditorSelectionType SelType, int32 ModIdx, ImU32 BgColor)
			{
				if (Mod)
				{
					FParticleModuleRowDesc Row;
					Row.Label = GetObjectLabel(Mod);
					Row.Address.Type = SelType;
					Row.Address.EmitterIndex = EmitterIndex;
					Row.Address.LODIndex = LODIndex;
					Row.Address.ModuleIndex = ModIdx;
					Row.BackgroundColor = BgColor;
					DrawSelectableModuleRow(RowContext, Row);
				}
			};

			DrawModuleRow(LOD->RequiredModule, EParticleEditorSelectionType::RequiredModule, -1, IM_COL32(244, 232, 156, 62));
			DrawModuleRow(LOD->SpawnModule, EParticleEditorSelectionType::SpawnModule, -1, IM_COL32(244, 150, 150, 58));

			for (int32 ModuleIndex = 0; ModuleIndex < static_cast<int32>(LOD->Modules.size()); ++ModuleIndex)
			{
				UParticleModule* Module = LOD->Modules[ModuleIndex];
				const bool bSpawnModule = Cast<UParticleModuleSpawn>(Module) != nullptr;
				DrawModuleRow(Module, EParticleEditorSelectionType::Module, ModuleIndex, bSpawnModule ? IM_COL32(244, 150, 150, 58) : IM_COL32(0, 0, 0, 0));
			}

			DrawModuleRow(LOD->TypeDataModule, EParticleEditorSelectionType::TypeDataModule, -1, IM_COL32(150, 190, 244, 45));
		}

		const float EmptySpaceHeight = ImGui::GetContentRegionAvail().y;
		if (EmptySpaceHeight > 0.0f)
		{
			ImGui::InvisibleButton("##EmitterEmptySpace", ImVec2(ImGui::GetContentRegionAvail().x, EmptySpaceHeight));

			if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
			{
				Viewer->SelectEmitter(EmitterIndex);
			}
			else if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
			{
				Viewer->SelectEmitter(EmitterIndex);
				Viewer->SelectLOD(LODIndex);
			}

			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 6.0f));
			if (ImGui::BeginPopupContextItem("EmitterEmptySpaceContext"))
			{
				RenderEmitterContextMenu(Viewer);
				ImGui::EndPopup();
			}
			ImGui::PopStyleVar(2);

			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload(ParticleModuleDragPayload))
				{
					const FParticleModuleDragPayload* DragPayload = static_cast<const FParticleModuleDragPayload*>(Payload->Data);
					if (DragPayload)
					{
						Viewer->SelectEmitter(DragPayload->EmitterIndex);
						Viewer->SelectLOD(DragPayload->LODIndex);
						ApplyModulePayloadToEmitter(Viewer, *DragPayload, EmitterIndex);
						ClearModuleMultiSelection(MultiSelectedModuleIndices, MultiSelectedModuleEmitterIndex, MultiSelectedModuleLODIndex);
					}
				}
				ImGui::EndDragDropTarget();
			}
		}

		ImGui::EndChild();
		ImGui::PopStyleVar(2);
	}

	const ImVec2 CardEnd(CardStart.x + CardWidth, SeparatorBottom);
	if (bSelected)
	{
		const ImVec2 PanelMin = ImGui::GetWindowPos();
		const ImVec2 PanelMax(PanelMin.x + ImGui::GetWindowWidth(), PanelMin.y + ImGui::GetWindowHeight());
		ImDrawList* OutlineDrawList = IsAnyPopupOpen() ? DrawList : ImGui::GetForegroundDrawList();
		OutlineDrawList->PushClipRect(PanelMin, PanelMax, true);
		OutlineDrawList->AddRect(CardStart, CardEnd, ParticleSelectionOutlineColor, 0.0f, 0, 2.0f);
		OutlineDrawList->PopClipRect();
	}
}

void FParticleEditorViewerWidget::DrawLODNode(FParticleEditorViewer* Viewer, int32 EmitterIndex, int32 LODIndex)
{
	UParticleSystem* ParticleSystem = Viewer->GetParticleSystem();
	if (!ParticleSystem || EmitterIndex < 0 || EmitterIndex >= static_cast<int32>(ParticleSystem->Emitters.size()))
	{
		return;
	}

	UParticleEmitter* Emitter = ParticleSystem->Emitters[EmitterIndex];
	if (!Emitter || LODIndex < 0 || LODIndex >= static_cast<int32>(Emitter->LODLevels.size()))
	{
		return;
	}

	const bool bSelected = Viewer->GetSelectionType() == EParticleEditorSelectionType::LODLevel &&
						   Viewer->GetSelectedEmitterIndex() == EmitterIndex &&
						   Viewer->GetSelectedLODIndex() == LODIndex;
	ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
	if (bSelected)
	{
		Flags |= ImGuiTreeNodeFlags_Selected;
	}

	const bool bOpen = ImGui::TreeNodeEx((void*)(intptr_t)LODIndex, Flags, "LOD %d", LODIndex);
	if (ImGui::IsItemClicked())
	{
		Viewer->SelectEmitter(EmitterIndex);
		Viewer->SelectLOD(LODIndex);
	}

	if (bOpen)
	{
		UParticleLODLevel* LOD = Emitter->LODLevels[LODIndex];
		if (LOD && LOD->RequiredModule)
		{
			ImGuiTreeNodeFlags RequiredFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
			if (Viewer->GetSelectionType() == EParticleEditorSelectionType::RequiredModule &&
				Viewer->GetSelectedEmitterIndex() == EmitterIndex &&
				Viewer->GetSelectedLODIndex() == LODIndex)
			{
				RequiredFlags |= ImGuiTreeNodeFlags_Selected;
			}
			ImGui::TreeNodeEx("Required", RequiredFlags, "Required");
			if (ImGui::IsItemClicked())
			{
				Viewer->SelectEmitter(EmitterIndex);
				Viewer->SelectLOD(LODIndex);
				Viewer->SelectRequiredModule();
			}
		}

		if (LOD && LOD->SpawnModule)
		{
			ImGuiTreeNodeFlags SpawnFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
			if (Viewer->GetSelectionType() == EParticleEditorSelectionType::SpawnModule &&
				Viewer->GetSelectedEmitterIndex() == EmitterIndex &&
				Viewer->GetSelectedLODIndex() == LODIndex)
			{
				SpawnFlags |= ImGuiTreeNodeFlags_Selected;
			}
			ImGui::TreeNodeEx("Spawn", SpawnFlags, "Spawn");
			if (ImGui::IsItemClicked())
			{
				Viewer->SelectEmitter(EmitterIndex);
				Viewer->SelectLOD(LODIndex);
				Viewer->SelectSpawnModule();
			}
		}

		if (LOD && LOD->TypeDataModule)
		{
			ImGuiTreeNodeFlags TypeDataFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
			if (Viewer->GetSelectionType() == EParticleEditorSelectionType::TypeDataModule &&
				Viewer->GetSelectedEmitterIndex() == EmitterIndex &&
				Viewer->GetSelectedLODIndex() == LODIndex)
			{
				TypeDataFlags |= ImGuiTreeNodeFlags_Selected;
			}
			ImGui::TreeNodeEx("TypeData", TypeDataFlags, "Type Data");
			if (ImGui::IsItemClicked())
			{
				Viewer->SelectEmitter(EmitterIndex);
				Viewer->SelectLOD(LODIndex);
				Viewer->SelectTypeDataModule();
			}
		}

		for (int32 ModuleIndex = 0; LOD && ModuleIndex < static_cast<int32>(LOD->Modules.size()); ++ModuleIndex)
		{
			DrawModuleNode(Viewer, EmitterIndex, LODIndex, ModuleIndex);
		}
		ImGui::TreePop();
	}
}

void FParticleEditorViewerWidget::DrawModuleNode(FParticleEditorViewer* Viewer, int32 EmitterIndex, int32 LODIndex, int32 ModuleIndex)
{
	UParticleSystem* ParticleSystem = Viewer->GetParticleSystem();
	if (!ParticleSystem || EmitterIndex < 0 || EmitterIndex >= static_cast<int32>(ParticleSystem->Emitters.size()))
	{
		return;
	}

	UParticleEmitter* Emitter = ParticleSystem->Emitters[EmitterIndex];
	if (!Emitter || LODIndex < 0 || LODIndex >= static_cast<int32>(Emitter->LODLevels.size()))
	{
		return;
	}

	UParticleLODLevel* LOD = Emitter->LODLevels[LODIndex];
	if (!LOD || ModuleIndex < 0 || ModuleIndex >= static_cast<int32>(LOD->Modules.size()))
	{
		return;
	}

	UParticleModule* Module = LOD->Modules[ModuleIndex];
	const bool bMultiSelected =
		MultiSelectedModuleEmitterIndex == EmitterIndex &&
		MultiSelectedModuleLODIndex == LODIndex &&
		ContainsIndex(MultiSelectedModuleIndices, ModuleIndex);
	ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
	if (bMultiSelected ||
		(Viewer->GetSelectionType() == EParticleEditorSelectionType::Module &&
		 Viewer->GetSelectedEmitterIndex() == EmitterIndex &&
		 Viewer->GetSelectedLODIndex() == LODIndex &&
		 Viewer->GetSelectedModuleIndex() == ModuleIndex))
	{
		Flags |= ImGuiTreeNodeFlags_Selected;
	}

	ImGui::TreeNodeEx((void*)(intptr_t)ModuleIndex, Flags, "%s", GetObjectLabel(Module));
	if (ImGui::IsItemClicked())
	{
		if (ImGui::GetIO().KeyCtrl && ImGui::GetIO().KeyShift)
		{
			SeedModuleMultiSelectionFromViewer(
				Viewer,
				MultiSelectedModuleIndices,
				MultiSelectedModuleEmitterIndex,
				MultiSelectedModuleLODIndex,
				EmitterIndex,
				LODIndex);
			SetModuleMultiSelectionContext(
				MultiSelectedModuleIndices,
				MultiSelectedModuleEmitterIndex,
				MultiSelectedModuleLODIndex,
				EmitterIndex,
				LODIndex);
			ToggleIndex(MultiSelectedModuleIndices, ModuleIndex);
			Viewer->SelectEmitterModule(EmitterIndex, LODIndex, ModuleIndex);
		}
		else
		{
			ClearModuleMultiSelection(MultiSelectedModuleIndices, MultiSelectedModuleEmitterIndex, MultiSelectedModuleLODIndex);
			Viewer->SelectEmitterModule(EmitterIndex, LODIndex, ModuleIndex);
		}
	}
	if (ImGui::BeginDragDropSource())
	{
		FParticleModuleDragPayload Payload;
		FParticleModuleAddress PayloadAddress;
		PayloadAddress.Type = EParticleEditorSelectionType::Module;
		PayloadAddress.EmitterIndex = EmitterIndex;
		PayloadAddress.LODIndex = LODIndex;
		PayloadAddress.ModuleIndex = ModuleIndex;
		FParticleModuleMultiSelectionView PayloadSelection;
		PayloadSelection.ModuleIndices = &MultiSelectedModuleIndices;
		PayloadSelection.EmitterIndex = &MultiSelectedModuleEmitterIndex;
		PayloadSelection.LODIndex = &MultiSelectedModuleLODIndex;
		BuildModulePayload(Payload, PayloadAddress, PayloadSelection);
		ImGui::SetDragDropPayload(ParticleModuleDragPayload, &Payload, sizeof(Payload));
		if (Payload.Count > 1)
		{
			ImGui::Text("%d Modules", Payload.Count);
		}
		else
		{
			ImGui::Text("Module: %s", GetObjectLabel(Module));
		}
		ImGui::EndDragDropSource();
	}

	HandleModuleContextMenu(Viewer, EmitterIndex, LODIndex, ModuleIndex);

	ImGui::SameLine();
	if (ImGui::SmallButton(("C##Curve" + std::to_string(ModuleIndex)).c_str()))
	{
		CurveState.Type = EParticleEditorSelectionType::Module;
		CurveState.EmitterIndex = EmitterIndex;
		CurveState.LODIndex = LODIndex;
		CurveState.ModuleIndex = ModuleIndex;
		Viewer->SelectEmitterModule(EmitterIndex, LODIndex, ModuleIndex);
	}
}
