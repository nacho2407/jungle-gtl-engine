#include "EditorViewportController.h"
#include "Component/CameraComponent.h"
#include "Input/InputManager.h"
#include "Input/EnhancedInputManager.h"
#include "Input/InputTrigger.h"
#include "Input/InputModifier.h"
#include "Input/InputMappingContext.h"
#include "Camera/Camera.h"
CEditorViewportController::~CEditorViewportController()
{
	Cleanup();
}

void CEditorViewportController::Cleanup()
{
	if (EnhancedInput && CameraContext)
		EnhancedInput->RemoveMappingContext(CameraContext);
	delete CameraContext;
	CameraContext = nullptr;
	EnhancedInput = nullptr;
}

void CEditorViewportController::Initialize(UCameraComponent* InCameraComp, CInputManager* InInput, CEnhancedInputManager* InEnhancedInput)
{
	CameraComponent = InCameraComp;
	InputManager = InInput;
	EnhancedInput = InEnhancedInput;
	SetupInputBindings();
}

void CEditorViewportController::Tick(float DeltaTime)
{
	CurrentDeltaTime = DeltaTime;
}


void CEditorViewportController::SetupInputBindings()
{
	CameraContext = new FInputMappingContext();


	auto& W = CameraContext->AddMapping(&MoveForwardAction, 'W');
	W.Triggers.push_back(new FTriggerDown());

	auto& S = CameraContext->AddMapping(&MoveForwardAction, 'S');
	S.Triggers.push_back(new FTriggerDown());
	S.Modifiers.push_back(new FModifierNegative()); // -1.0f

	auto& D = CameraContext->AddMapping(&MoveRightAction, 'D');
	D.Triggers.push_back(new FTriggerDown());

	auto& A = CameraContext->AddMapping(&MoveRightAction, 'A');
	A.Triggers.push_back(new FTriggerDown());
	A.Modifiers.push_back(new FModifierNegative());

	auto& E = CameraContext->AddMapping(&MoveUpAction, 'E');
	E.Triggers.push_back(new FTriggerDown());

	auto& Q = CameraContext->AddMapping(&MoveUpAction, 'Q');
	Q.Triggers.push_back(new FTriggerDown());
	Q.Modifiers.push_back(new FModifierNegative());


	CameraContext->AddMapping(&LookXAction, static_cast<int32>(EInputKey::MouseX));
	CameraContext->AddMapping(&LookYAction, static_cast<int32>(EInputKey::MouseY));

	EnhancedInput->AddMappingContext(CameraContext, 0);


	EnhancedInput->BindAction(&MoveForwardAction, ETriggerEvent::Triggered,
		[this](const FInputActionValue& Value) {
		if (InputManager && InputManager->IsMouseButtonDown(CInputManager::MOUSE_RIGHT))
			CameraComponent->MoveForward(Value.Get() * CurrentDeltaTime);
	});

	EnhancedInput->BindAction(&MoveRightAction, ETriggerEvent::Triggered,
		[this](const FInputActionValue& Value) {
		if (InputManager && InputManager->IsMouseButtonDown(CInputManager::MOUSE_RIGHT))
			CameraComponent->MoveRight(Value.Get() * CurrentDeltaTime);
	});

	EnhancedInput->BindAction(&MoveUpAction, ETriggerEvent::Triggered,
		[this](const FInputActionValue& Value) {
		if (InputManager && InputManager->IsMouseButtonDown(CInputManager::MOUSE_RIGHT))
			CameraComponent->MoveUp(Value.Get() * CurrentDeltaTime);
	});

	EnhancedInput->BindAction(&LookXAction, ETriggerEvent::Triggered,
		[this](const FInputActionValue& Value) {
		if (InputManager && InputManager->IsMouseButtonDown(CInputManager::MOUSE_RIGHT))
			CameraComponent->Rotate(Value.Get() * CameraComponent->GetCamera()->GetMouseSensitivity(), 0.0f);
	});

	EnhancedInput->BindAction(&LookYAction, ETriggerEvent::Triggered,
		[this](const FInputActionValue& Value) {
		if (InputManager && InputManager->IsMouseButtonDown(CInputManager::MOUSE_RIGHT))
			CameraComponent->Rotate(0.0f, -Value.Get() * CameraComponent->GetCamera()->GetMouseSensitivity());
	});

}
