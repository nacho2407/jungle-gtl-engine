#pragma once

#include "Core/Types/CoreTypes.h"
#include "sol/sol.hpp"

class AActor;
class APawn;
class APlayerController;
class APlayerCameraManager;
class UClass;
class UWorld;

/**
 * @brief ProjectSettings.GameplayPreset 기반 world gameplay 부팅 런타임
 *
 * @details C++ gameplay rule actor 없이 PlayerController, Pawn, Lua Director 수명만 관리합니다
 */
class FGameplayRuntime
{
public:
	FGameplayRuntime() = default;
	~FGameplayRuntime();

	/**
	 * @brief 지정된 world의 gameplay runtime을 시작합니다
	 *
	 * @param InWorld gameplay를 시작할 world
	 */
	void StartWorld(UWorld* InWorld);

	/**
	 * @brief Lua Director Tick을 실행합니다
	 *
	 * @param DeltaTime 현재 frame delta time
	 */
	void Tick(float DeltaTime);

	/**
	 * @brief 현재 world gameplay runtime을 종료합니다
	 */
	void EndWorld();

	APlayerController* GetPlayerController() const { return PlayerController; }
	APawn* GetPlayerPawn() const { return PlayerPawn; }
	APlayerCameraManager* GetPlayerCameraManager() const;

private:
	UClass* ResolveClass(const FString& ClassName, UClass* RequiredBaseClass, UClass* FallbackClass, const char* SettingName) const;
	APlayerController* SpawnPlayerController();
	APawn* FindPlacedAutoPossessPawn() const;
	APawn* SpawnDefaultPawn();
	AActor* FindPlayerStart() const;
	void PossessPawn(APawn* Pawn);

	bool LoadDirector();
	void CallDirectorNoArg(const char* FunctionName);
	void CallDirectorWithContext(const char* FunctionName);
	void CallDirectorTick(float DeltaTime);
	sol::table MakeDirectorContext() const;
	void ClearDirector();

private:
	UWorld* World = nullptr;
	APlayerController* PlayerController = nullptr;
	APawn* PlayerPawn = nullptr;
	FString DirectorModule;
	sol::table DirectorTable;
	bool bStarted = false;
};
