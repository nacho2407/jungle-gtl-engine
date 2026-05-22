#include "ParticleEditorViewer.h"

#include "Component/SkeletalMeshComponent.h"

bool FParticleEditorViewer::ChangeTarget(const FString& InFileName)
{
	SetFileName(InFileName);
	ClearBaseSelection();

	if (USkeletalMeshComponent* SkelComp = GetSkeletalMeshComponent())
	{
		SkelComp->Stop();
		SkelComp->SetAnimation(nullptr);
		SkelComp->SetSkeletalMesh(nullptr);
	}

	return true;
}

EEditorTabKind FParticleEditorViewer::GetTabKind() const
{
	return EEditorTabKind::StaticMeshViewer;
}

const char* FParticleEditorViewer::GetViewerLabel() const
{
	return "Particle Viewer";
}
