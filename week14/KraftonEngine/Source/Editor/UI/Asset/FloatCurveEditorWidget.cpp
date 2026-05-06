#include "FloatCurveEditorWidget.h"

#include "FloatCurve/FloatCurveAsset.h"
#include "Object/Object.h"

#include <algorithm>
#include <cmath>

#include <imgui.h>

namespace
{
	constexpr float CurveCanvasSize = 320.0f;
	constexpr float KeyRadius = 6.0f;
	constexpr float TimeEpsilon = 0.001f;

	static ImVec2 CurveToScreen(
		float Time,
		float Value,
		float ViewMinTime,
		float ViewMaxTime,
		float ViewMinValue,
		float ViewMaxValue,
		const ImVec2& Min,
		const ImVec2& Max)
	{
		const float Width = Max.x - Min.x;
		const float Height = Max.y - Min.y;
		const float SafeTimeSpan = (ViewMaxTime > ViewMinTime) ? (ViewMaxTime - ViewMinTime) : 1.0f;
		const float SafeValueSpan = (ViewMaxValue > ViewMinValue) ? (ViewMaxValue - ViewMinValue) : 1.0f;

		const float X = (Time - ViewMinTime) / SafeTimeSpan;
		const float Y = (Value - ViewMinValue) / SafeValueSpan;

		return ImVec2(Min.x + X * Width, Max.y - Y * Height);
	}

	static void ScreenToCurve(
		const ImVec2& Position,
		float ViewMinTime,
		float ViewMaxTime,
		float ViewMinValue,
		float ViewMaxValue,
		const ImVec2& Min,
		const ImVec2& Max,
		float& OutTime,
		float& OutValue)
	{
		const float Width = Max.x - Min.x;
		const float Height = Max.y - Min.y;
		const float SafeTimeSpan = (ViewMaxTime > ViewMinTime) ? (ViewMaxTime - ViewMinTime) : 1.0f;
		const float SafeValueSpan = (ViewMaxValue > ViewMinValue) ? (ViewMaxValue - ViewMinValue) : 1.0f;

		const float NormalizedX = (Width > 0.0f) ? ((Position.x - Min.x) / Width) : 0.0f;
		const float NormalizedY = (Height > 0.0f) ? ((Max.y - Position.y) / Height) : 0.0f;

		OutTime = ViewMinTime + NormalizedX * SafeTimeSpan;
		OutValue = ViewMinValue + NormalizedY * SafeValueSpan;
	}

	static bool IsFiniteCurvePoint(const ImVec2& Point)
	{
		return std::isfinite(Point.x) && std::isfinite(Point.y);
	}
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
	SelectedKeyIndex = -1;
	bDraggingSelectedKey = false;
}

void FFloatCurveEditorWidget::Render(float DeltaTime)
{
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
		const FString& SourcePath = CurveAsset->GetSourcePath();
		if (!SourcePath.empty() && CurveAsset->SaveToFile(SourcePath))
		{
			ClearDirty();
		}
	}

	ImGui::SameLine();
	ImGui::TextDisabled("%s", CurveAsset->GetSourcePath().empty() ? "Unsaved asset" : CurveAsset->GetSourcePath().c_str());
	ImGui::Separator();

	bool bChanged = false;

	ImGui::BeginChild("CurveCanvasPanel", ImVec2(CurveCanvasSize + 16.0f, 0.0f), true);
	{
		const ImVec2 CanvasSize(CurveCanvasSize, CurveCanvasSize);
		const ImVec2 CanvasMin = ImGui::GetCursorScreenPos();
		const ImVec2 CanvasMax(CanvasMin.x + CanvasSize.x, CanvasMin.y + CanvasSize.y);

		ImGui::InvisibleButton("##FloatCurveCanvas", CanvasSize);
		const bool bCanvasHovered = ImGui::IsItemHovered();
		ImDrawList* DrawList = ImGui::GetWindowDrawList();

		DrawList->AddRectFilled(CanvasMin, CanvasMax, IM_COL32(24, 24, 28, 255), 4.0f);
		DrawList->AddRect(CanvasMin, CanvasMax, IM_COL32(90, 90, 100, 255), 4.0f);

		for (int GridIndex = 1; GridIndex < 4; ++GridIndex)
		{
			const float T = static_cast<float>(GridIndex) / 4.0f;
			const float GridX = CanvasMin.x + T * CanvasSize.x;
			const float GridY = CanvasMin.y + T * CanvasSize.y;
			DrawList->AddLine(ImVec2(GridX, CanvasMin.y), ImVec2(GridX, CanvasMax.y), IM_COL32(60, 60, 68, 255));
			DrawList->AddLine(ImVec2(CanvasMin.x, GridY), ImVec2(CanvasMax.x, GridY), IM_COL32(60, 60, 68, 255));
		}

		if (!Curve.IsEmpty())
		{
			const int SampleCount = 64;
			ImVec2 PreviousPoint = CurveToScreen(
				ViewMinTime,
				Curve.Evaluate(ViewMinTime),
				ViewMinTime,
				ViewMaxTime,
				ViewMinValue,
				ViewMaxValue,
				CanvasMin,
				CanvasMax);

			for (int SampleIndex = 1; SampleIndex < SampleCount; ++SampleIndex)
			{
				const float Alpha = static_cast<float>(SampleIndex) / static_cast<float>(SampleCount - 1);
				const float SampleTime = ViewMinTime + (ViewMaxTime - ViewMinTime) * Alpha;
				const ImVec2 CurrentPoint = CurveToScreen(
					SampleTime,
					Curve.Evaluate(SampleTime),
					ViewMinTime,
					ViewMaxTime,
					ViewMinValue,
					ViewMaxValue,
					CanvasMin,
					CanvasMax);

				if (IsFiniteCurvePoint(PreviousPoint) && IsFiniteCurvePoint(CurrentPoint))
				{
					DrawList->AddLine(PreviousPoint, CurrentPoint, IM_COL32(80, 220, 120, 255), 2.0f);
				}
				PreviousPoint = CurrentPoint;
			}
		}

		int HoveredKeyIndex = -1;
		const ImVec2 MousePos = ImGui::GetIO().MousePos;

		for (int32 KeyIndex = 0; KeyIndex < static_cast<int32>(Curve.Keys.size()); ++KeyIndex)
		{
			const FCurveKey& Key = Curve.Keys[KeyIndex];
			const ImVec2 KeyPos = CurveToScreen(
				Key.Time,
				Key.Value,
				ViewMinTime,
				ViewMaxTime,
				ViewMinValue,
				ViewMaxValue,
				CanvasMin,
				CanvasMax);

			const float DX = MousePos.x - KeyPos.x;
			const float DY = MousePos.y - KeyPos.y;
			if ((DX * DX + DY * DY) <= (KeyRadius * KeyRadius))
			{
				HoveredKeyIndex = KeyIndex;
			}

			const ImU32 KeyColor =
				(SelectedKeyIndex == KeyIndex) ? IM_COL32(255, 255, 80, 255) :
				(HoveredKeyIndex == KeyIndex) ? IM_COL32(255, 210, 80, 255) :
				IM_COL32(255, 180, 60, 255);
			DrawList->AddCircleFilled(KeyPos, 5.0f, KeyColor);
		}

		if (HoveredKeyIndex != -1 && ImGui::IsMouseClicked(0))
		{
			SelectedKeyIndex = HoveredKeyIndex;
			bDraggingSelectedKey = true;
		}

		if (bDraggingSelectedKey && SelectedKeyIndex != -1 && ImGui::IsMouseDragging(0))
		{
			FCurveKey& Key = Curve.Keys[SelectedKeyIndex];
			const ImVec2 MouseDelta = ImGui::GetIO().MouseDelta;
			const float TimeDelta = (MouseDelta.x / CanvasSize.x) * (ViewMaxTime - ViewMinTime);
			const float ValueDelta = -(MouseDelta.y / CanvasSize.y) * (ViewMaxValue - ViewMinValue);

			Key.Time += TimeDelta;
			Key.Value += ValueDelta;

			if (SelectedKeyIndex > 0)
			{
				Key.Time = (std::max)(Key.Time, Curve.Keys[SelectedKeyIndex - 1].Time + TimeEpsilon);
			}
			if (SelectedKeyIndex + 1 < static_cast<int32>(Curve.Keys.size()))
			{
				Key.Time = (std::min)(Key.Time, Curve.Keys[SelectedKeyIndex + 1].Time - TimeEpsilon);
			}

			bChanged = true;
		}

		if (ImGui::IsMouseReleased(0) && SelectedKeyIndex != -1)
		{
			Curve.SortKeys();
			Curve.AutoSetTangents();
			bDraggingSelectedKey = false;
		}
		else if (ImGui::IsMouseReleased(0))
		{
			bDraggingSelectedKey = false;
		}

		if (HoveredKeyIndex != -1 && ImGui::IsMouseClicked(1))
		{
			SelectedKeyIndex = HoveredKeyIndex;
			ImGui::OpenPopup("CurveKeyContextMenu");
		}
		else if (bCanvasHovered && HoveredKeyIndex == -1 && ImGui::IsMouseClicked(1))
		{
			ImGui::OpenPopup("CurveCanvasContextMenu");
		}

		if (ImGui::BeginPopup("CurveKeyContextMenu"))
		{
			if (SelectedKeyIndex >= 0 && SelectedKeyIndex < static_cast<int32>(Curve.Keys.size()))
			{
				if (ImGui::MenuItem("Delete Key"))
				{
					Curve.Keys.erase(Curve.Keys.begin() + SelectedKeyIndex);
					SelectedKeyIndex = -1;
					bChanged = true;
				}
			}
			ImGui::EndPopup();
		}

		if (ImGui::BeginPopup("CurveCanvasContextMenu"))
		{
			if (ImGui::MenuItem("Add Key"))
			{
				float NewTime = 0.0f;
				float NewValue = 0.0f;
				ScreenToCurve(
					MousePos,
					ViewMinTime,
					ViewMaxTime,
					ViewMinValue,
					ViewMaxValue,
					CanvasMin,
					CanvasMax,
					NewTime,
					NewValue);

				FCurveKey NewKey{};
				NewKey.Time = NewTime;
				NewKey.Value = NewValue;
				Curve.Keys.push_back(NewKey);
				Curve.SortKeys();
				Curve.AutoSetTangents();
				bChanged = true;
			}
			ImGui::EndPopup();
		}

		if (HoveredKeyIndex != -1)
		{
			const FCurveKey& HoveredKey = Curve.Keys[HoveredKeyIndex];
			ImGui::SetTooltip("Time: %.3f\nValue: %.3f", HoveredKey.Time, HoveredKey.Value);
		}
	}
	ImGui::EndChild();

	ImGui::SameLine();

	ImGui::BeginChild("CurveInspectorPanel", ImVec2(0.0f, 0.0f), false);
	{
		if (ImGui::DragFloat("Default Value", &Curve.DefaultValue, 0.01f))
		{
			bChanged = true;
		}

		ImGui::Separator();
		ImGui::TextUnformatted("View");
		if (ImGui::DragFloat("Min Time", &ViewMinTime, 0.01f))
		{
			ViewMaxTime = (std::max)(ViewMaxTime, ViewMinTime + TimeEpsilon);
		}
		if (ImGui::DragFloat("Max Time", &ViewMaxTime, 0.01f))
		{
			ViewMaxTime = (std::max)(ViewMaxTime, ViewMinTime + TimeEpsilon);
		}
		if (ImGui::DragFloat("Min Value", &ViewMinValue, 0.01f))
		{
			ViewMaxValue = (std::max)(ViewMaxValue, ViewMinValue + TimeEpsilon);
		}
		if (ImGui::DragFloat("Max Value", &ViewMaxValue, 0.01f))
		{
			ViewMaxValue = (std::max)(ViewMaxValue, ViewMinValue + TimeEpsilon);
		}


		ImGui::Separator();
		if (SelectedKeyIndex >= 0 && SelectedKeyIndex < static_cast<int32>(Curve.Keys.size()))
		{
			FCurveKey& Key = Curve.Keys[SelectedKeyIndex];

			ImGui::Text("Selected Key: %d", SelectedKeyIndex);

			const char* InterpModeLabels[] = { "Constant", "Linear", "Cubic" };
			int32 CurrentInterpMode = static_cast<int32>(Key.InterpMode);
			if (ImGui::Combo("Interpolation Mode", &CurrentInterpMode, InterpModeLabels, IM_ARRAYSIZE(InterpModeLabels)))
			{
				Key.InterpMode = static_cast<ECurveInterpMode>(CurrentInterpMode);

				if (Key.InterpMode == ECurveInterpMode::Cubic)
				{
					Key.TangentMode = ECurveTangentMode::Auto;
					Curve.AutoSetTangents();
				}

				bChanged = true;
			}

			const char* TangentModeLabels[] = { "Auto", "User", "Break" };
			int32 CurrentTangentMode = static_cast<int32>(Key.TangentMode);
			if (ImGui::Combo("Tangent Mode", &CurrentTangentMode, TangentModeLabels, IM_ARRAYSIZE(TangentModeLabels)))
			{
				Key.TangentMode = static_cast<ECurveTangentMode>(CurrentTangentMode);

				if (Key.TangentMode == ECurveTangentMode::Auto)
				{
					Curve.AutoSetTangents();
				}

				bChanged = true;
			}

			if (Key.TangentMode != ECurveTangentMode::Auto)
			{
				if (ImGui::DragFloat("Arrive Tangent", &Key.ArriveTangent, 0.01f))
				{
					bChanged = true;
				}

				if (ImGui::DragFloat("Leave Tangent", &Key.LeaveTangent, 0.01f))
				{
					bChanged = true;
				}
			}

			if (ImGui::DragFloat("Time", &Key.Time, 0.01f))
			{
				if (SelectedKeyIndex > 0)
				{
					Key.Time = (std::max)(Key.Time, Curve.Keys[SelectedKeyIndex - 1].Time + TimeEpsilon);
				}
				if (SelectedKeyIndex + 1 < static_cast<int32>(Curve.Keys.size()))
				{
					Key.Time = (std::min)(Key.Time, Curve.Keys[SelectedKeyIndex + 1].Time - TimeEpsilon);
				}
				Curve.SortKeys();
				Curve.AutoSetTangents();
				bChanged = true;
			}
			if (ImGui::DragFloat("Value", &Key.Value, 0.01f))
			{
				bChanged = true;
			}
		}
		else
		{
			ImGui::TextDisabled("No key selected");
		}
	}
	ImGui::EndChild();

	if (bChanged)
	{
		MarkDirty();
	}

	ImGui::End();

	if (!bWindowOpen)
	{
		Close();
	}
}
