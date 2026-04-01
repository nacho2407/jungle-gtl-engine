#pragma once

#include "Render/Types/ViewTypes.h"

class UCameraComponent;

struct FRenderCollectorContext
{
	UCameraComponent* Camera = nullptr;
	EViewMode ViewMode = EViewMode::Lit;
	FShowFlags ShowFlags;
};
