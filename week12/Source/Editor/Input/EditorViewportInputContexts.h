#pragma once

#include "Core/CoreMinimal.h"

enum class EEditorViewportInputContextType : uint8
{
    Navigation,
    Selection,
    Gizmo,
    Command
};

enum class EEditorInputContextPriority : int32
{
    Navigation = 100,
    Selection = 200,
    Gizmo = 300,
    Command = 400
};

struct FEditorViewportInputContextDescriptor
{
    EEditorViewportInputContextType Type = EEditorViewportInputContextType::Navigation;
    int32 Priority = 0;
};

namespace EditorViewportInputContexts
{
    inline const TArray<FEditorViewportInputContextDescriptor>& GetOrderedContexts()
    {
        static const TArray<FEditorViewportInputContextDescriptor> Contexts =
        {
            { EEditorViewportInputContextType::Command, static_cast<int32>(EEditorInputContextPriority::Command) },
            { EEditorViewportInputContextType::Gizmo, static_cast<int32>(EEditorInputContextPriority::Gizmo) },
            { EEditorViewportInputContextType::Selection, static_cast<int32>(EEditorInputContextPriority::Selection) },
            { EEditorViewportInputContextType::Navigation, static_cast<int32>(EEditorInputContextPriority::Navigation) }
        };
        return Contexts;
    }
}
