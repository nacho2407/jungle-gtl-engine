#include "ParticleSystemEditorWidget.h"

#include "imgui.h"
#include "Object/Object.h"
#include "Particle/ParticleSystem.h"
#include "Particle/ParticleSystemManager.h"

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

    if (UParticleSystem* ParticleSystem = GetParticleSystem())
    {
        WindowTitle = "Particle System Editor - ";
        WindowTitle += ParticleSystem->GetSourcePath();
    }
}

void FParticleSystemEditorWidget::Close()
{
    FAssetEditorWidget::Close();
}

void FParticleSystemEditorWidget::Tick(float DeltaTime)
{
    (void)DeltaTime;
}

void FParticleSystemEditorWidget::Render(float DeltaTime)
{
    (void)DeltaTime;

    if (!IsOpen())
    {
        return;
    }
}

UParticleSystem* FParticleSystemEditorWidget::GetParticleSystem() const
{
    return Cast<UParticleSystem>(EditedObject);
}
