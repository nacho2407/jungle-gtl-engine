#pragma once

#include "Editor/Viewport/Viewer/ViewerViewportClient.h"

class FParticleViewerViewportClient : public FViewerViewportClient
{
public:
	void BuildViewerShowFlags(FShowFlags& OutShowFlags) const override;
};
