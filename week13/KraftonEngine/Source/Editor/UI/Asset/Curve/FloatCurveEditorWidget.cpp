#include "FloatCurveEditorWidget.h"

#include "FloatCurve/FloatCurveAsset.h"
#include "FloatCurve/FloatCurveManager.h"
#include "Object/Object.h"

#include <imgui.h>

namespace
{
    constexpr float CurveCanvasSize = 420.0f;
}

bool FFloatCurveEditorWidget::CanEdit(UObject* Object) const
{
    return Object && Object->IsA<UFloatCurveAsset>();
}

void FFloatCurveEditorWidget::Open(UObject* Object)
{
    if (!CanEdit(Object))
    {
        return;
    }

    EditedObject = Object;
    bOpen = true;
    ClearDirty();
    InlineEditor.ResetSelection();
    FitViewToCurve();
}

void FFloatCurveEditorWidget::FitViewToCurve()
{
    if (!EditedObject || !EditedObject->IsA<UFloatCurveAsset>())
    {
        return;
    }

    UFloatCurveAsset* CurveAsset = static_cast<UFloatCurveAsset*>(EditedObject);
    InlineEditor.FitViewToCurve(CurveAsset->GetCurve());
}

void FFloatCurveEditorWidget::Render(const FEditorPanelContext& Context)
{
    (void)Context;

    if (!IsOpen() || !EditedObject)
    {
        return;
    }

    UFloatCurveAsset* CurveAsset = static_cast<UFloatCurveAsset*>(EditedObject);
    FFloatCurve& Curve = CurveAsset->GetCurve();

    bool bWindowOpen = true;
    FString VisibleTitle = "Float Curve Editor";
    if (!CurveAsset->GetSourcePath().empty())
    {
        VisibleTitle += " - ";
        VisibleTitle += CurveAsset->GetSourcePath();
    }
    if (IsDirty())
    {
        VisibleTitle += " *";
    }

    ImGui::SetNextWindowSize(ImVec2(720.0f, 520.0f), ImGuiCond_Once);

    FString WindowTitle = VisibleTitle + "###FloatCurveEditor";
    if (!ImGui::Begin(WindowTitle.c_str(), &bWindowOpen))
    {
        ImGui::End();
        if (!bWindowOpen)
        {
            Close();
        }
        return;
    }

    if (ImGui::Button("Save"))
    {
        if (FFloatCurveManager::Get().Save(CurveAsset))
        {
            ClearDirty();
        }
    }

    ImGui::SameLine();
    ImGui::TextDisabled("%s", CurveAsset->GetSourcePath().empty() ? "Unsaved asset" : CurveAsset->GetSourcePath().c_str());
    ImGui::Separator();

    FInlineCurveEditResult Result = InlineEditor.Render(
        "FloatCurveAssetEditor",
        Curve,
        ImVec2(CurveCanvasSize, CurveCanvasSize),
        FInlineFloatCurveEditor::EInteractionMode::Pan,
        true);
    if (Result.bChanged)
    {
        MarkDirty();
    }

    ImGui::End();

    if (!bWindowOpen)
    {
        Close();
    }
}
