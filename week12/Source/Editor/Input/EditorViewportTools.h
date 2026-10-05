#pragma once

#include "Editor/Input/EditorViewportInputContexts.h"
#include "Engine/Input/InputTypes.h"

class FEditorViewportClient;

class IEditorViewportTool
{
public:
    virtual ~IEditorViewportTool() = default;
    virtual EEditorViewportInputContextType GetContextType() const = 0;
    virtual int32 GetPriority() const = 0;
    virtual bool HandleInput(FEditorViewportClient& Owner, const FViewportInputContext& Context) = 0;
};

class FEditorViewportCommandTool final : public IEditorViewportTool
{
public:
    EEditorViewportInputContextType GetContextType() const override { return EEditorViewportInputContextType::Command; }
    int32 GetPriority() const override { return static_cast<int32>(EEditorInputContextPriority::Command); }
    bool HandleInput(FEditorViewportClient& Owner, const FViewportInputContext& Context) override;
};

class FEditorViewportGizmoTool final : public IEditorViewportTool
{
public:
    EEditorViewportInputContextType GetContextType() const override { return EEditorViewportInputContextType::Gizmo; }
    int32 GetPriority() const override { return static_cast<int32>(EEditorInputContextPriority::Gizmo); }
    bool HandleInput(FEditorViewportClient& Owner, const FViewportInputContext& Context) override;
};

class FEditorViewportSelectionTool final : public IEditorViewportTool
{
public:
    EEditorViewportInputContextType GetContextType() const override { return EEditorViewportInputContextType::Selection; }
    int32 GetPriority() const override { return static_cast<int32>(EEditorInputContextPriority::Selection); }
    bool HandleInput(FEditorViewportClient& Owner, const FViewportInputContext& Context) override;
};

class FEditorViewportNavigationTool final : public IEditorViewportTool
{
public:
    EEditorViewportInputContextType GetContextType() const override { return EEditorViewportInputContextType::Navigation; }
    int32 GetPriority() const override { return static_cast<int32>(EEditorInputContextPriority::Navigation); }
    bool HandleInput(FEditorViewportClient& Owner, const FViewportInputContext& Context) override;
};
