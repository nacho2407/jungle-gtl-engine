#include "Editor/Viewport/Viewer/ParticleViewerViewportClient.h"

void FParticleViewerViewportClient::BuildViewerShowFlags(FShowFlags& OutShowFlags) const
{
	FViewerViewportClient::BuildViewerShowFlags(OutShowFlags);
	OutShowFlags.bSkeletalMesh = false;
}
