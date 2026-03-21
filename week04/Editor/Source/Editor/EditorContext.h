#pragma once

class UAssetManager;
class UObject;
class FD3D11DynamicRHI;
class FScene;
class FEditor;

struct FEditorContext
{
	FEditor* Editor = nullptr;
	FScene* Scene = nullptr;
	FD3D11DynamicRHI* RHI = nullptr;
	UAssetManager* AssetManager = nullptr;

	UObject* SelectedObject = nullptr;

    float CurrentFPS = 0.0f;
    float WindowWidth = 0.0f;
    float WindowHeight = 0.0f;
};

