#pragma once

#include "CameraComponent.h"
#include "Core/EngineTypes.h"

#include "Source/Engine/Component/CineCameraComponent.generated.h"
struct FCineLetterboxSettings
{
	bool bEnabled = false;
	float Amount = 1.0f;
	float Thickness = 0.12f;
	FLinearColor Color = FLinearColor::Black();
};

UCLASS()
class UCineCameraComponent : public UCameraComponent
{
public:
	GENERATED_BODY()
	UCineCameraComponent() = default;

	void SetLetterboxEnabled(bool bEnabled) { Letterbox.bEnabled = bEnabled; }
	void SetLetterboxAmount(float Amount) { Letterbox.Amount = Amount; }
	void SetLetterboxThickness(float Thickness) { Letterbox.Thickness = Thickness; }
	void SetLetterboxColor(FLinearColor Color) { Letterbox.Color = Color; }

	const FCineLetterboxSettings& GetLetterboxSettings() const { return Letterbox; }

private:
	UPROPERTY(Edit, Save, Category="Cinematic", DisplayName="Enable Letterbox", Member=Letterbox.bEnabled, Type=Bool);
	UPROPERTY(Edit, Save, Category="Cinematic", DisplayName="Letterbox Amount", Member=Letterbox.Amount, Type=Float, Min=0.0f, Max=1.0f, Speed=0.01f);
	UPROPERTY(Edit, Save, Category="Cinematic", DisplayName="Letterbox Thickness", Member=Letterbox.Thickness, Type=Float, Min=0.0f, Max=0.5f, Speed=0.01f);
	UPROPERTY(Edit, Save, Category="Cinematic", DisplayName="Letterbox Color", Member=Letterbox.Color, Type=Color4);
	FCineLetterboxSettings Letterbox;
};
