#pragma once

#include "Render/Common/ViewTypes.h"

class UCameraComponent;

struct FRenderCollectorContext
{
	UCameraComponent* Camera = nullptr;
	EViewMode ViewMode = EViewMode::Lit;
	FShowFlags ShowFlags;
};
