#include "GameFramework/CameraModifier.h"
#include "Object/ObjectFactory.h"
#include <algorithm>

IMPLEMENT_CLASS(UCameraModifier, UObject)

void UCameraModifier::AddedToCamera(APlayerCameraManager* InCameraOwner)
{
	CameraOwner = InCameraOwner;
}

bool UCameraModifier::ModifyCamera(float DeltaTime, FMinimalViewInfo& /*InOutPOV*/)
{
	// Alpha 페이드 진행 — Disable 진행 중이면 0 으로, Enable 진행 중이면 1 로.
	if (bDisabled)
	{
		if (AlphaOutTime > 0.0f)
		{
			Alpha = std::max(Alpha - DeltaTime / AlphaOutTime, 0.0f);
		}
		else
		{
			Alpha = 0.0f;
		}
	}
	else
	{
		if (AlphaInTime > 0.0f)
		{
			Alpha = std::min(Alpha + DeltaTime / AlphaInTime, 1.0f);
		}
		else
		{
			Alpha = 1.0f;
		}
	}
	return false;  // 베이스는 exclusive 아님
}

void UCameraModifier::DisableModifier(bool bImmediate)
{
	bDisabled = true;
	if (bImmediate)
	{
		Alpha = 0.0f;
	}
}

void UCameraModifier::EnableModifier()
{
	bDisabled = false;
}
