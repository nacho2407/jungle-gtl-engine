#include "FloatCurveEditorWidget.h"

#include "FloatCurve/FloatCurveAsset.h"
#include "FloatCurve/FloatCurveManager.h"
#include "Object/Object.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include <imgui.h>

namespace
{
	constexpr float CurveCanvasSize = 420.0f;
	constexpr float KeyHitRadius = 7.0f;
	constexpr float TangentHandleLength = 48.0f;
	constexpr float TangentHandleHitRadius = 6.0f;
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
		const float TimeSpan = (ViewMaxTime > ViewMinTime) ? (ViewMaxTime - ViewMinTime) : 1.0f;
		const float ValueSpan = (ViewMaxValue > ViewMinValue) ? (ViewMaxValue - ViewMinValue) : 1.0f;

		const float X = (Time - ViewMinTime) / TimeSpan;
		const float Y = (Value - ViewMinValue) / ValueSpan;
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
		const float TimeSpan = (ViewMaxTime > ViewMinTime) ? (ViewMaxTime - ViewMinTime) : 1.0f;
		const float ValueSpan = (ViewMaxValue > ViewMinValue) ? (ViewMaxValue - ViewMinValue) : 1.0f;

		const float NormalizedX = (Width > 0.0f) ? ((Position.x - Min.x) / Width) : 0.0f;
		const float NormalizedY = (Height > 0.0f) ? ((Max.y - Position.y) / Height) : 0.0f;
		OutTime = ViewMinTime + NormalizedX * TimeSpan;
		OutValue = ViewMinValue + NormalizedY * ValueSpan;
	}

	static bool IsFinitePoint(const ImVec2& Point)
	{
		return std::isfinite(Point.x) && std::isfinite(Point.y);
	}

	static bool IsPointNear(const ImVec2& A, const ImVec2& B, float Radius)
	{
		const float DX = A.x - B.x;
		const float DY = A.y - B.y;
		return (DX * DX + DY * DY) <= Radius * Radius;
	}

	static ImVec2 GetTangentHandlePosition(
		const FCurveKey& Key,
		bool bArrive,
		float ViewMinTime,
		float ViewMaxTime,
		float ViewMinValue,
		float ViewMaxValue,
		const ImVec2& Min,
		const ImVec2& Max)
	{
		const float Tangent = bArrive ? Key.ArriveTangent : Key.LeaveTangent;
		const float Direction = bArrive ? -1.0f : 1.0f;
		const float Width = Max.x - Min.x;
		const float Height = Max.y - Min.y;
		const float TimeSpan = (ViewMaxTime > ViewMinTime) ? (ViewMaxTime - ViewMinTime) : 1.0f;
		const float ValueSpan = (ViewMaxValue > ViewMinValue) ? (ViewMaxValue - ViewMinValue) : 1.0f;

		ImVec2 DirectionVector(
			Direction * Width / TimeSpan,
			-Direction * Tangent * Height / ValueSpan);
		const float Length = std::sqrt(DirectionVector.x * DirectionVector.x + DirectionVector.y * DirectionVector.y);
		if (Length <= 1e-6f)
		{
			DirectionVector = ImVec2(Direction, 0.0f);
		}
		else
		{
			DirectionVector.x /= Length;
			DirectionVector.y /= Length;
		}

		const ImVec2 KeyPos = CurveToScreen(
			Key.Time,
			Key.Value,
			ViewMinTime,
			ViewMaxTime,
			ViewMinValue,
			ViewMaxValue,
			Min,
			Max);
		return ImVec2(
			KeyPos.x + DirectionVector.x * TangentHandleLength,
			KeyPos.y + DirectionVector.y * TangentHandleLength);
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
	DraggingTangentHandle = ETangentHandle::None;
	bPanningView = false;
	bSuppressNextCanvasContextMenu = false;
	FitViewToCurve();
}

void FFloatCurveEditorWidget::FitViewToCurve()
{
	if (!EditedObject || !EditedObject->IsA<UFloatCurveAsset>())
	{
		return;
	}

	UFloatCurveAsset* CurveAsset = static_cast<UFloatCurveAsset*>(EditedObject);
	const FFloatCurve& Curve = CurveAsset->GetCurve();
	if (Curve.Keys.empty())
	{
		ViewMinTime = 0.0f;
		ViewMaxTime = 1.0f;
		ViewMinValue = -1.0f;
		ViewMaxValue = 1.0f;
		return;
	}

	ViewMinTime = Curve.Keys.front().Time;
	ViewMaxTime = Curve.Keys.front().Time;
	ViewMinValue = Curve.Keys.front().Value;
	ViewMaxValue = Curve.Keys.front().Value;
	for (const FCurveKey& Key : Curve.Keys)
	{
		ViewMinTime = (std::min)(ViewMinTime, Key.Time);
		ViewMaxTime = (std::max)(ViewMaxTime, Key.Time);
		ViewMinValue = (std::min)(ViewMinValue, Key.Value);
		ViewMaxValue = (std::max)(ViewMaxValue, Key.Value);
	}

	if (ViewMaxTime <= ViewMinTime + TimeEpsilon)
	{
		ViewMinTime -= 0.5f;
		ViewMaxTime += 0.5f;
	}

	if (ViewMaxValue <= ViewMinValue + TimeEpsilon)
	{
		ViewMinValue -= 0.5f;
		ViewMaxValue += 0.5f;
	}
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
		if (FFloatCurveManager::Get().Save(CurveAsset))
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
		const ImGuiIO& IO = ImGui::GetIO();

		DrawList->AddRectFilled(CanvasMin, CanvasMax, IM_COL32(22, 22, 25, 255), 4.0f);
		DrawList->AddRect(CanvasMin, CanvasMax, IM_COL32(86, 86, 96, 255), 4.0f);

		for (int GridIndex = 1; GridIndex < 10; ++GridIndex)
		{
			const float T = static_cast<float>(GridIndex) / 10.0f;
			const ImU32 GridColor = (GridIndex == 5) ? IM_COL32(76, 76, 86, 255) : IM_COL32(48, 48, 56, 255);
			const float GridX = CanvasMin.x + T * CanvasSize.x;
			const float GridY = CanvasMin.y + T * CanvasSize.y;
			DrawList->AddLine(ImVec2(GridX, CanvasMin.y), ImVec2(GridX, CanvasMax.y), GridColor);
			DrawList->AddLine(ImVec2(CanvasMin.x, GridY), ImVec2(CanvasMax.x, GridY), GridColor);
		}

		if (ViewMinTime <= 0.0f && ViewMaxTime >= 0.0f)
		{
			const ImVec2 A = CurveToScreen(0.0f, ViewMinValue, ViewMinTime, ViewMaxTime, ViewMinValue, ViewMaxValue, CanvasMin, CanvasMax);
			DrawList->AddLine(ImVec2(A.x, CanvasMin.y), ImVec2(A.x, CanvasMax.y), IM_COL32(95, 95, 110, 255), 2.0f);
		}
		if (ViewMinValue <= 0.0f && ViewMaxValue >= 0.0f)
		{
			const ImVec2 A = CurveToScreen(ViewMinTime, 0.0f, ViewMinTime, ViewMaxTime, ViewMinValue, ViewMaxValue, CanvasMin, CanvasMax);
			DrawList->AddLine(ImVec2(CanvasMin.x, A.y), ImVec2(CanvasMax.x, A.y), IM_COL32(95, 95, 110, 255), 2.0f);
		}

		if (!Curve.IsEmpty())
		{
			const int32 SampleCount = 128;
			ImVec2 PreviousPoint = CurveToScreen(
				ViewMinTime,
				Curve.Evaluate(ViewMinTime),
				ViewMinTime,
				ViewMaxTime,
				ViewMinValue,
				ViewMaxValue,
				CanvasMin,
				CanvasMax);

			for (int32 SampleIndex = 1; SampleIndex < SampleCount; ++SampleIndex)
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

				if (IsFinitePoint(PreviousPoint) && IsFinitePoint(CurrentPoint))
				{
					DrawList->AddLine(PreviousPoint, CurrentPoint, IM_COL32(80, 220, 120, 255), 2.0f);
				}
				PreviousPoint = CurrentPoint;
			}
		}

		ETangentHandle HoveredTangentHandle = ETangentHandle::None;
		const bool bHasSelectedKey = SelectedKeyIndex >= 0 && SelectedKeyIndex < static_cast<int32>(Curve.Keys.size());
		const bool bCanShowArriveHandle =
			bHasSelectedKey &&
			SelectedKeyIndex > 0 &&
			Curve.Keys[SelectedKeyIndex - 1].InterpMode == ECurveInterpMode::Cubic;
		const bool bCanShowLeaveHandle =
			bHasSelectedKey &&
			SelectedKeyIndex + 1 < static_cast<int32>(Curve.Keys.size()) &&
			Curve.Keys[SelectedKeyIndex].InterpMode == ECurveInterpMode::Cubic;

		if (bHasSelectedKey && (bCanShowArriveHandle || bCanShowLeaveHandle))
		{
			const FCurveKey& Key = Curve.Keys[SelectedKeyIndex];
			const ImVec2 KeyPos = CurveToScreen(
				Key.Time,
				Key.Value,
				ViewMinTime,
				ViewMaxTime,
				ViewMinValue,
				ViewMaxValue,
				CanvasMin,
				CanvasMax);

			if (bCanShowArriveHandle)
			{
				const ImVec2 HandlePos = GetTangentHandlePosition(
					Key,
					true,
					ViewMinTime,
					ViewMaxTime,
					ViewMinValue,
					ViewMaxValue,
					CanvasMin,
					CanvasMax);
				if (IsFinitePoint(HandlePos))
				{
					if (IsPointNear(IO.MousePos, HandlePos, TangentHandleHitRadius))
					{
						HoveredTangentHandle = ETangentHandle::Arrive;
					}
					DrawList->AddLine(KeyPos, HandlePos, IM_COL32(95, 150, 255, 180), 1.5f);
					DrawList->AddCircleFilled(HandlePos, 4.5f, IM_COL32(95, 150, 255, 255));
					DrawList->AddCircle(HandlePos, 4.5f, IM_COL32(15, 20, 30, 220), 12, 1.0f);
				}
			}

			if (bCanShowLeaveHandle)
			{
				const ImVec2 HandlePos = GetTangentHandlePosition(
					Key,
					false,
					ViewMinTime,
					ViewMaxTime,
					ViewMinValue,
					ViewMaxValue,
					CanvasMin,
					CanvasMax);
				if (IsFinitePoint(HandlePos))
				{
					if (IsPointNear(IO.MousePos, HandlePos, TangentHandleHitRadius))
					{
						HoveredTangentHandle = ETangentHandle::Leave;
					}
					DrawList->AddLine(KeyPos, HandlePos, IM_COL32(95, 150, 255, 180), 1.5f);
					DrawList->AddCircleFilled(HandlePos, 4.5f, IM_COL32(95, 150, 255, 255));
					DrawList->AddCircle(HandlePos, 4.5f, IM_COL32(15, 20, 30, 220), 12, 1.0f);
				}
			}
		}

		int32 HoveredKeyIndex = -1;
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

			const float DX = IO.MousePos.x - KeyPos.x;
			const float DY = IO.MousePos.y - KeyPos.y;
			if ((DX * DX + DY * DY) <= KeyHitRadius * KeyHitRadius)
			{
				HoveredKeyIndex = KeyIndex;
			}

			const ImU32 KeyColor =
				(SelectedKeyIndex == KeyIndex) ? IM_COL32(255, 245, 110, 255) :
				(HoveredKeyIndex == KeyIndex) ? IM_COL32(255, 205, 90, 255) :
				IM_COL32(255, 165, 60, 255);
			DrawList->AddCircleFilled(KeyPos, 5.0f, KeyColor);
			DrawList->AddCircle(KeyPos, 5.0f, IM_COL32(20, 20, 22, 220), 12, 1.0f);
		}

		if (bCanvasHovered && IO.MouseWheel != 0.0f)
		{
			float MouseTime = 0.0f;
			float MouseValue = 0.0f;
			ScreenToCurve(IO.MousePos, ViewMinTime, ViewMaxTime, ViewMinValue, ViewMaxValue, CanvasMin, CanvasMax, MouseTime, MouseValue);

			const float Zoom = (IO.MouseWheel > 0.0f) ? 0.85f : 1.1764706f;
			const bool bZoomTime = !IO.KeyCtrl;
			const bool bZoomValue = !IO.KeyShift;

			if (bZoomTime)
			{
				ViewMinTime = MouseTime + (ViewMinTime - MouseTime) * Zoom;
				ViewMaxTime = MouseTime + (ViewMaxTime - MouseTime) * Zoom;
				if (ViewMaxTime <= ViewMinTime + TimeEpsilon)
				{
					ViewMaxTime = ViewMinTime + TimeEpsilon;
				}
			}
			if (bZoomValue)
			{
				ViewMinValue = MouseValue + (ViewMinValue - MouseValue) * Zoom;
				ViewMaxValue = MouseValue + (ViewMaxValue - MouseValue) * Zoom;
				if (ViewMaxValue <= ViewMinValue + TimeEpsilon)
				{
					ViewMaxValue = ViewMinValue + TimeEpsilon;
				}
			}
		}

		if (HoveredTangentHandle != ETangentHandle::None && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
		{
			DraggingTangentHandle = HoveredTangentHandle;
			bDraggingSelectedKey = false;
		}
		else if (HoveredKeyIndex != -1 && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
		{
			SelectedKeyIndex = HoveredKeyIndex;
			bDraggingSelectedKey = true;
		}
		else if (bCanvasHovered && HoveredKeyIndex == -1 && HoveredTangentHandle == ETangentHandle::None && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
		{
			SelectedKeyIndex = -1;
		}

		if (DraggingTangentHandle != ETangentHandle::None && bHasSelectedKey && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
		{
			FCurveKey& Key = Curve.Keys[SelectedKeyIndex];
			float MouseTime = 0.0f;
			float MouseValue = 0.0f;
			ScreenToCurve(IO.MousePos, ViewMinTime, ViewMaxTime, ViewMinValue, ViewMaxValue, CanvasMin, CanvasMax, MouseTime, MouseValue);

			float NewTangent = 0.0f;
			if (DraggingTangentHandle == ETangentHandle::Arrive)
			{
				const float DeltaTime = Key.Time - MouseTime;
				if (std::fabs(DeltaTime) > TimeEpsilon)
				{
					NewTangent = (Key.Value - MouseValue) / DeltaTime;
				}
				else
				{
					NewTangent = Key.ArriveTangent;
				}
			}
			else
			{
				const float DeltaTime = MouseTime - Key.Time;
				if (std::fabs(DeltaTime) > TimeEpsilon)
				{
					NewTangent = (MouseValue - Key.Value) / DeltaTime;
				}
				else
				{
					NewTangent = Key.LeaveTangent;
				}
			}

			if (Key.TangentMode == ECurveTangentMode::Auto)
			{
				Key.TangentMode = ECurveTangentMode::User;
			}

			if (Key.TangentMode == ECurveTangentMode::Break)
			{
				if (DraggingTangentHandle == ETangentHandle::Arrive)
				{
					Key.ArriveTangent = NewTangent;
				}
				else
				{
					Key.LeaveTangent = NewTangent;
				}
			}
			else
			{
				Key.ArriveTangent = NewTangent;
				Key.LeaveTangent = NewTangent;
			}
			bChanged = true;
		}

		if (bDraggingSelectedKey && SelectedKeyIndex >= 0 && SelectedKeyIndex < static_cast<int32>(Curve.Keys.size()) && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
		{
			FCurveKey& Key = Curve.Keys[SelectedKeyIndex];
			const float TimeSpan = ViewMaxTime - ViewMinTime;
			const float ValueSpan = ViewMaxValue - ViewMinValue;
			Key.Time += (IO.MouseDelta.x / CanvasSize.x) * TimeSpan;
			Key.Value -= (IO.MouseDelta.y / CanvasSize.y) * ValueSpan;

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

		if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
		{
			if (bDraggingSelectedKey)
			{
				Curve.SortKeys();
				Curve.AutoSetTangents();
			}
			bDraggingSelectedKey = false;
			DraggingTangentHandle = ETangentHandle::None;
		}

		if (bCanvasHovered && HoveredKeyIndex == -1 && HoveredTangentHandle == ETangentHandle::None && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
		{
			bPanningView = true;
			bSuppressNextCanvasContextMenu = false;
		}
		if (bPanningView && ImGui::IsMouseDragging(ImGuiMouseButton_Right, 3.0f))
		{
			const float TimeSpan = ViewMaxTime - ViewMinTime;
			const float ValueSpan = ViewMaxValue - ViewMinValue;
			const float TimeDelta = (IO.MouseDelta.x / CanvasSize.x) * TimeSpan;
			const float ValueDelta = (IO.MouseDelta.y / CanvasSize.y) * ValueSpan;
			ViewMinTime -= TimeDelta;
			ViewMaxTime -= TimeDelta;
			ViewMinValue += ValueDelta;
			ViewMaxValue += ValueDelta;
			bSuppressNextCanvasContextMenu = true;
		}
		if (ImGui::IsMouseReleased(ImGuiMouseButton_Right))
		{
			bPanningView = false;
		}

		if (HoveredKeyIndex != -1 && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
		{
			SelectedKeyIndex = HoveredKeyIndex;
			ImGui::OpenPopup("FloatCurveKeyContext");
		}
		else if (bCanvasHovered && HoveredKeyIndex == -1 && HoveredTangentHandle == ETangentHandle::None && ImGui::IsMouseReleased(ImGuiMouseButton_Right) && !bSuppressNextCanvasContextMenu)
		{
			ScreenToCurve(IO.MousePos, ViewMinTime, ViewMaxTime, ViewMinValue, ViewMaxValue, CanvasMin, CanvasMax, PendingContextTime, PendingContextValue);
			ImGui::OpenPopup("FloatCurveCanvasContext");
		}

		if (ImGui::BeginPopup("FloatCurveKeyContext"))
		{
			if (SelectedKeyIndex >= 0 && SelectedKeyIndex < static_cast<int32>(Curve.Keys.size()))
			{
				bool bKeyDeleted = false;
				if (ImGui::MenuItem("Delete Key"))
				{
					Curve.Keys.erase(Curve.Keys.begin() + SelectedKeyIndex);
					SelectedKeyIndex = -1;
					bChanged = true;
					bKeyDeleted = true;
				}
				if (!bKeyDeleted)
				{
					ImGui::Separator();
					if (ImGui::MenuItem("Constant", nullptr, Curve.Keys[SelectedKeyIndex].InterpMode == ECurveInterpMode::Constant))
					{
						Curve.Keys[SelectedKeyIndex].InterpMode = ECurveInterpMode::Constant;
						bChanged = true;
					}
					if (ImGui::MenuItem("Linear", nullptr, Curve.Keys[SelectedKeyIndex].InterpMode == ECurveInterpMode::Linear))
					{
						Curve.Keys[SelectedKeyIndex].InterpMode = ECurveInterpMode::Linear;
						bChanged = true;
					}
					if (ImGui::MenuItem("Cubic", nullptr, Curve.Keys[SelectedKeyIndex].InterpMode == ECurveInterpMode::Cubic))
					{
						Curve.Keys[SelectedKeyIndex].InterpMode = ECurveInterpMode::Cubic;
						Curve.Keys[SelectedKeyIndex].TangentMode = ECurveTangentMode::Auto;
						Curve.AutoSetTangents();
						bChanged = true;
					}
				}
			}
			ImGui::EndPopup();
		}

		if (ImGui::BeginPopup("FloatCurveCanvasContext"))
		{
			if (ImGui::MenuItem("Add Key"))
			{
				FCurveKey NewKey{};
				NewKey.Time = PendingContextTime;
				NewKey.Value = PendingContextValue;
				NewKey.InterpMode = ECurveInterpMode::Linear;
				Curve.Keys.push_back(NewKey);
				Curve.SortKeys();
				Curve.AutoSetTangents();
				for (int32 KeyIndex = 0; KeyIndex < static_cast<int32>(Curve.Keys.size()); ++KeyIndex)
				{
					if (std::fabs(Curve.Keys[KeyIndex].Time - NewKey.Time) <= TimeEpsilon)
					{
						SelectedKeyIndex = KeyIndex;
						break;
					}
				}
				bChanged = true;
			}
			if (ImGui::MenuItem("Fit To Keys"))
			{
				FitViewToCurve();
			}
			ImGui::EndPopup();
		}

		if (HoveredTangentHandle != ETangentHandle::None && bHasSelectedKey)
		{
			const FCurveKey& Key = Curve.Keys[SelectedKeyIndex];
			const float Tangent = (HoveredTangentHandle == ETangentHandle::Arrive) ? Key.ArriveTangent : Key.LeaveTangent;
			ImGui::SetTooltip("%s Tangent %.3f", (HoveredTangentHandle == ETangentHandle::Arrive) ? "Arrive" : "Leave", Tangent);
		}
		else if (HoveredKeyIndex != -1)
		{
			const FCurveKey& HoveredKey = Curve.Keys[HoveredKeyIndex];
			ImGui::SetTooltip("Time %.3f\nValue %.3f", HoveredKey.Time, HoveredKey.Value);
		}

		ImGui::TextDisabled("RMB drag: pan  |  Wheel: zoom  |  Shift/Ctrl+Wheel: zoom one axis");
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
		ImGui::TextUnformatted("Edit Range");
		if (ImGui::Button("Fit To Keys"))
		{
			FitViewToCurve();
		}
		if (ImGui::DragFloat("Min Time (sec)", &ViewMinTime, 0.01f))
		{
			ViewMaxTime = (std::max)(ViewMaxTime, ViewMinTime + TimeEpsilon);
		}
		if (ImGui::DragFloat("Max Time (sec)", &ViewMaxTime, 0.01f))
		{
			ViewMaxTime = (std::max)(ViewMaxTime, ViewMinTime + TimeEpsilon);
		}
		if (ImGui::DragFloat("Min Value (actual)", &ViewMinValue, 0.01f))
		{
			ViewMaxValue = (std::max)(ViewMaxValue, ViewMinValue + TimeEpsilon);
		}
		if (ImGui::DragFloat("Max Value (actual)", &ViewMaxValue, 0.01f))
		{
			ViewMaxValue = (std::max)(ViewMaxValue, ViewMinValue + TimeEpsilon);
		}


		ImGui::Separator();
		ImGui::TextUnformatted("Keys");
		for (int32 KeyIndex = 0; KeyIndex < static_cast<int32>(Curve.Keys.size()); ++KeyIndex)
		{
			const FCurveKey& Key = Curve.Keys[KeyIndex];
			char Label[128];
			std::snprintf(Label, sizeof(Label), "%02d  T %.3f  V %.3f", KeyIndex, Key.Time, Key.Value);
			if (ImGui::Selectable(Label, SelectedKeyIndex == KeyIndex))
			{
				SelectedKeyIndex = KeyIndex;
			}
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
