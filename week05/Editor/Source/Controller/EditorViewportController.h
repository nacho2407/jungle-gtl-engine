#pragma once

#include "Input/InputAction.h"

class UCameraComponent;
class FInputManager;
class FEnhancedInputManager;
struct FInputMappingContext;
struct FViewportLocalState;


class FEditorViewportController
{
public:
	~FEditorViewportController();

	void Initialize(UCameraComponent* InCameraComp, FInputManager* InInput, FEnhancedInputManager* InEnhancedInput);
	void Cleanup();
	void SetFrameDeltaTime(float DeltaTime);
	void SetActiveLocalState(FViewportLocalState* InLocalState);

private:
	void SetupInputBindings();

	UCameraComponent* CameraComponent = nullptr;
	FInputManager* InputManager = nullptr;
	FEnhancedInputManager* EnhancedInput = nullptr;
	FViewportLocalState* ActiveLocalState = nullptr;

	FInputMappingContext* CameraContext = nullptr; // 소멸자에서 정리

	// Action 정의 (포인터 아닌 값으로 소유)
	FInputAction MoveForwardAction{ "MoveForward", EInputActionValueType::Float };
	FInputAction MoveRightAction{ "MoveRight",   EInputActionValueType::Float };
	FInputAction MoveUpAction{ "MoveUp",      EInputActionValueType::Float };

	float CurrentDeltaTime = 0.0f; // 콜백에서 DeltaTime 쓰기 위해 보관


};
