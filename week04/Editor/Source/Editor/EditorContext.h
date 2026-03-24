#pragma once

#include "Core/Containers/Array.h"

class UAssetManager;
class UObject;
class FD3D11RHI;
class FScene;
class FEditor;
class AActor;

struct FEditorContext
{
    FEditor*       Editor = nullptr;
    FScene*        Scene = nullptr;
    FD3D11RHI*     RHI = nullptr;
    UAssetManager* AssetManager = nullptr;

    TArray<AActor*> SelectedActors;
    UObject*        SelectedObject = nullptr;

    float CurrentFPS = 0.0f;
    float DeltaTime = 0.0f;
    float WindowWidth = 0.0f;
    float WindowHeight = 0.0f;
};
