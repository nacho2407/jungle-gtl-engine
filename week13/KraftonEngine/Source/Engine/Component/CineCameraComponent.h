#pragma once

#include "CameraComponent.h"
#include "Core/EngineTypes.h"

struct FCineLetterboxSettings
{
	bool bEnabled = false;
	float Amount = 1.0f;
	float Thickness = 0.12f;
	FLinearColor Color = FLinearColor::Black();
};

class UCineCameraComponent : public UCameraComponent
{
public:
	DECLARE_CLASS(UCineCameraComponent, UCameraComponent)

	UCineCameraComponent() = default;

	void Serialize(FArchive& Ar) override;
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;

	void SetLetterboxEnabled(bool bEnabled) { Letterbox.bEnabled = bEnabled; }
	void SetLetterboxAmount(float Amount) { Letterbox.Amount = Amount; }
	void SetLetterboxThickness(float Thickness) { Letterbox.Thickness = Thickness; }
	void SetLetterboxColor(FLinearColor Color) { Letterbox.Color = Color; }

	const FCineLetterboxSettings& GetLetterboxSettings() const { return Letterbox; }

private:
	FCineLetterboxSettings Letterbox;
};
