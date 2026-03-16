#pragma once

class FWindowsApplication;
class UCameraComponent;

class FEditorCameraController
{
public:
	void Initialize(FWindowsApplication* InWindowApp, UCameraComponent* InCamera);
	void Tick(float DeltaTime);

private:
	FWindowsApplication* WindowApp = nullptr;
	UCameraComponent* Camera = nullptr;
	int PrevMouseX = 0;
	int PrevMouseY = 0;
};