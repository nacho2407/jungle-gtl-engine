#pragma once

#include <memory>

#include "Core/CoreTypes.h"
#include "EditorViewportTool.h"

class FEditorViewportClient;

class FEditorViewportInputController
{
public:
    explicit FEditorViewportInputController(FEditorViewportClient* InOwner);

    bool HandleInput(float DeltaTime);

private:
    FEditorViewportClient* Owner = nullptr;

    TArray<std::unique_ptr<FEditorViewportTool>> Tools;
};
