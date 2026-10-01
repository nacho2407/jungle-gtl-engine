#pragma once

#include "CameraManage/CameraTypes.h"
#include "Core/CoreTypes.h"

class FCameraEffectAssetManager
{
public:
    static bool Load(const FString& AssetPath, FCameraEffectAsset& OutAsset);
    static bool GetOrLoad(const FString& AssetPath, FCameraEffectAsset& OutAsset);
    static void Invalidate(const FString& AssetPath);
    static void ClearCache();
};
