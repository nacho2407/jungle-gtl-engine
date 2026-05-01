#pragma once

#include "GameFramework/AActor.h"

class APlayerController : public AActor
{
public:
	DECLARE_CLASS(APlayerController, AActor)

	APlayerController() = default;
	~APlayerController() override = default;
};
