#pragma once
#include "Math/Vector.h"
#include "imgui.h"
#include <functional>
class CCore;
using FPropertyChangedCallback = std::function<void(const FVector&, const FVector&, const FVector&)>;

class CPropertyWindow
{
public:
	void Render(CCore* Core);
	void SetTarget(const FVector& Location, const FVector& Rotation, const FVector& Scale,
		const char* ActorName = nullptr);

	bool    IsModified()    const { return bModified; }
	FVector GetLocation()   const { return EditLocation; }
	FVector GetRotation()   const { return EditRotation; }
	FVector GetScale()      const { return EditScale; }

	void SetOnChanged(FPropertyChangedCallback Callback) { OnChanged = Callback; }

	FPropertyChangedCallback OnChanged;
private:
	void DrawTransformSection();

	FVector EditLocation = { 0.0f, 0.0f, 0.0f };
	FVector EditRotation = { 0.0f, 0.0f, 0.0f };
	FVector EditScale = { 1.0f, 1.0f, 1.0f };
	char    ActorNameBuf[128] = "None";
	bool    bModified = false;
};
