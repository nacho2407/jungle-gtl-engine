#include "ParticleEditorInternal.h"

using namespace ParticleEditorInternal;

void FParticleEditorViewerWidget::RenderDetailsPanel(FParticleEditorViewer* Viewer)
{
	UObject* SelectedObject = Viewer->GetSelectedObject();
	const char* SelectedTypeLabel = SelectedObject ? GetObjectLabel(SelectedObject) : GetSelectionLabel(Viewer->GetSelectionType());
	DrawParticlePanelTitle("Details", SelectedTypeLabel);
	DrawParticleDetailsSection("Selection");
	DrawParticleDetailsText("Type", SelectedTypeLabel);
	DrawParticleDetailsText("Object", GetObjectLabel(SelectedObject));

	if (!SelectedObject)
	{
		ImGui::Separator();
		ImGui::TextDisabled("Select a particle system, emitter, LOD, or module.");
		return;
	}

	ImGui::Separator();
	if (Viewer->GetSelectionType() == EParticleEditorSelectionType::ParticleSystem)
	{
		UParticleSystem* ParticleSystem = Viewer->GetParticleSystem();
		DrawParticleDetailsSection("Particle System");
		ImGui::Text("Emitter Count");
		ImGui::SameLine(150.0f);
		ImGui::Text("%d", ParticleSystem ? static_cast<int32>(ParticleSystem->Emitters.size()) : 0);
	}
	else if (UParticleEmitter* Emitter = Cast<UParticleEmitter>(SelectedObject))
	{
		DrawParticleDetailsSection("Emitter");
		ImGui::Text("LOD Count");
		ImGui::SameLine(150.0f);
		ImGui::Text("%d", static_cast<int32>(Emitter->LODLevels.size()));
		ImGui::Text("Runtime Caches");
		ImGui::SameLine(150.0f);
		ImGui::Text("%d", static_cast<int32>(Emitter->LODLevelRuntimeCaches.size()));
	}

	DrawParticleDetailsSection("Properties");
	FParticlePropertyRenderContext PropertyContext;
	PropertyContext.Viewer = Viewer;
	PropertyContext.Object = SelectedObject;
	PropertyContext.EditorEngine = EditorEngine;
	PropertyContext.bUndoCaptured = &bPropertyEditUndoCaptured;
	if (!RenderParticleReflectionProperties(PropertyContext))
	{
		ImGui::TextDisabled("No reflected editable properties.");
	}
}


