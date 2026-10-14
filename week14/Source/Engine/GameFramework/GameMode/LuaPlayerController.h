#pragma once

#include "GameFramework/GameMode/PlayerController.h"

#include "Source/Engine/GameFramework/GameMode/LuaPlayerController.generated.h"

/**
 * @brief Lua gameplay runtime용 player controller
 *
 * @details 입력, possession, camera manager 동작은 APlayerController 기본 구현을 그대로 사용합니다
 */
UCLASS()
class ALuaPlayerController : public APlayerController
{
public:
	GENERATED_BODY()
	ALuaPlayerController() = default;
	~ALuaPlayerController() override = default;
};
