#pragma once

#include "Input/InputAction.h"

class UCameraComponent;
class CInputManager;
class CEnhancedInputManager;
struct FInputMappingContext;


class CEditorViewportController
{
public:
	~CEditorViewportController();

	// ← EnhancedInput 포인터 추가
	void Initialize(UCameraComponent* InCameraComp, CInputManager* InInput, CEnhancedInputManager* InEnhancedInput);
	void Cleanup();

	void Tick(float DeltaTime);



private:
	void SetupInputBindings(); // 기존 ProcessCameraInput 대체

	UCameraComponent* CameraComponent = nullptr;
	CInputManager* InputManager = nullptr;
	CEnhancedInputManager* EnhancedInput = nullptr;

	FInputMappingContext* CameraContext = nullptr; // 소멸자에서 정리

	// Action 정의 (포인터 아닌 값으로 소유)
	FInputAction MoveForwardAction{ "MoveForward", EInputActionValueType::Float };
	FInputAction MoveRightAction{ "MoveRight",   EInputActionValueType::Float };
	FInputAction MoveUpAction{ "MoveUp",      EInputActionValueType::Float };
	FInputAction LookXAction{ "LookX",       EInputActionValueType::Float };
	FInputAction LookYAction{ "LookY",       EInputActionValueType::Float };

	float CurrentDeltaTime = 0.0f; // 콜백에서 DeltaTime 쓰기 위해 보관


};