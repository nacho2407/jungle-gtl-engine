#include "ParticleSystemEditorWidget.h"

#include "Component/Light/DirectionalLightComponent.h"
#include "Component/Particle/ParticleSystemComponent.h"
#include "Core/Property/SoftObjectProperty.h"
#include "Editor/UI/Util/EditorTextureManager.h"
#include "GameFramework/AActor.h"
#include "GameFramework/Light/DirectionalLightActor.h"
#include "Math/Distribution.h"
#include "Math/FloatCurve.h"
#include "Object/Object.h"
#include "Object/Reflection/UStruct.h"
#include "Object/ObjectIterator.h"
#include "Object/Reflection/ObjectFactory.h"
#include "Particles/Module/ParticleModule.h"
#include "Particles/Module/ParticleModuleCollision.h"
#include "Particles/Module/ParticleModuleTypeDataBase.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemManager.h"
#include "Platform/Paths.h"
#include "Render/Scene/FScene.h"
#include "Render/Types/MinimalViewInfo.h"
#include "Runtime/Engine.h"
#include "Serialization/MemoryArchive.h"
#include "Slate/SlateApplication.h"
#include "Viewport/Viewport.h"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <imgui.h>
#include <utility>

namespace
{
	constexpr float MinColumnWidth = 360.0f;
	constexpr float MinViewportHeight = 220.0f;
	constexpr float MinDetailsHeight = 160.0f;
	constexpr float MinEmittersHeight = 180.0f;
	constexpr float MinCurveEditorHeight = 140.0f;
	constexpr float SplitterThickness = 6.0f;
	constexpr float ToolbarHeight = 34.0f;
	constexpr float DetailsNameColumnWidth = 275.0f;
	constexpr float DetailsScalarInputWidth = 124.0f;
	constexpr float EmitterColumnWidth = 176.0f;
	constexpr float EmitterHeaderHeight = 58.0f;
	constexpr float ModuleRowHeight = 24.0f;
	constexpr float ModuleDropTargetHeight = 8.0f;
	constexpr float CurveKeyHitRadius = 7.0f;
	constexpr float CurveTangentHandleLength = 48.0f;
	constexpr float CurveTangentHandleHitRadius = 6.0f;
	constexpr float CurveTimeEpsilon = 0.001f;
	constexpr int32 NoModuleIndex = -1;
	constexpr int32 TypeDataModuleIndex = -2;
	constexpr const char* ParticleModuleDragPayloadType = "ParticleModuleDragPayload";

	struct FModuleRowAction
	{
		bool bSelect = false;
		bool bDelete = false;
		bool bRefresh = false;
		bool bToggleEnabled = false;
		bool bShowCurves = false;
		bool bDuplicateFromHigher = false;
		bool bShareFromHigher = false;
		bool bDuplicateFromHighest = false;
		bool bContextMenuOpen = false;
	};

	struct FParticleModuleDragPayload
	{
		int32 EmitterIndex = -1;
		int32 LODIndex = -1;
		int32 ModuleIndex = -1;
		UParticleModule* Module = nullptr;
	};

	struct FParticleModuleDropRequest
	{
		bool bRequested = false;
		FParticleModuleDragPayload Payload;
		int32 TargetEmitterIndex = -1;
		int32 TargetLODIndex = -1;
		int32 TargetInsertIndex = 0;
	};

	float ClampFloat(float Value, float MinValue, float MaxValue)
	{
		return (std::max)(MinValue, (std::min)(Value, MaxValue));
	}

	float CalculateSplitLeadingSize(float TotalSize, float Ratio, float MinLeadingSize, float MinTrailingSize)
	{
		TotalSize = (std::max)(TotalSize, 1.0f);
		if (TotalSize >= MinLeadingSize + MinTrailingSize)
		{
			return ClampFloat(TotalSize * Ratio, MinLeadingSize, TotalSize - MinTrailingSize);
		}

		if (TotalSize > 2.0f)
		{
			return ClampFloat(TotalSize * Ratio, 1.0f, TotalSize - 1.0f);
		}

		return (std::max)(1.0f, TotalSize * 0.5f);
	}

	float CalculateSplitRatio(float LeadingSize, float TotalSize, float MinLeadingSize, float MinTrailingSize)
	{
		TotalSize = (std::max)(TotalSize, 1.0f);
		const float ClampedLeadingSize = CalculateSplitLeadingSize(TotalSize, LeadingSize / TotalSize, MinLeadingSize, MinTrailingSize);
		return ClampedLeadingSize / TotalSize;
	}

	bool DrawSplitterHandle(const char* Id, const ImVec2& Size, bool bVertical)
	{
		ImGui::InvisibleButton(Id, Size);
		const bool bHovered = ImGui::IsItemHovered();
		const bool bActive = ImGui::IsItemActive();
		if (bHovered || bActive)
		{
			ImGui::SetMouseCursor(bVertical ? ImGuiMouseCursor_ResizeEW : ImGuiMouseCursor_ResizeNS);
		}

		const ImVec2 Min = ImGui::GetItemRectMin();
		const ImVec2 Max = ImGui::GetItemRectMax();
		const ImU32 Color = ImGui::GetColorU32(bActive ? ImGuiCol_SeparatorActive : (bHovered ? ImGuiCol_SeparatorHovered : ImGuiCol_Separator));
		ImDrawList* DrawList = ImGui::GetWindowDrawList();
		if (bVertical)
		{
			const float X = (Min.x + Max.x) * 0.5f;
			DrawList->AddLine(ImVec2(X, Min.y), ImVec2(X, Max.y), Color, 2.0f);
		}
		else
		{
			const float Y = (Min.y + Max.y) * 0.5f;
			DrawList->AddLine(ImVec2(Min.x, Y), ImVec2(Max.x, Y), Color, 2.0f);
		}

		return bActive;
	}

	ImVec2 CurveToScreen(
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

	void ScreenToCurve(
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

	bool IsFinitePoint(const ImVec2& Point)
	{
		return std::isfinite(Point.x) && std::isfinite(Point.y);
	}

	bool IsPointNear(const ImVec2& A, const ImVec2& B, float Radius)
	{
		const float DX = A.x - B.x;
		const float DY = A.y - B.y;
		return (DX * DX + DY * DY) <= Radius * Radius;
	}

	ImVec2 GetTangentHandlePosition(
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
		if (Length <= 1.0e-6f)
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
			KeyPos.x + DirectionVector.x * CurveTangentHandleLength,
			KeyPos.y + DirectionVector.y * CurveTangentHandleLength);
	}

	bool IsRawDistributionFloatProperty(const FPropertyValue& PropertyValue)
	{
		UStruct* StructType = PropertyValue.GetStructType();
		return StructType && StructType->GetName() && std::strcmp(StructType->GetName(), "FRawDistributionFloat") == 0;
	}

	bool IsRawDistributionVectorProperty(const FPropertyValue& PropertyValue)
	{
		UStruct* StructType = PropertyValue.GetStructType();
		return StructType && StructType->GetName() && std::strcmp(StructType->GetName(), "FRawDistributionVector") == 0;
	}

	void SetFloatCurveToConstant(FFloatCurve& Curve, float Value)
	{
		Curve.Reset();
		Curve.DefaultValue = Value;
		Curve.AddKey(0.0f, Value);
		Curve.AddKey(1.0f, Value);
	}

	void SetVectorCurveToConstant(FFloatVectorCurve& Curve, const FVector& Value)
	{
		SetFloatCurveToConstant(Curve.X, Value.X);
		SetFloatCurveToConstant(Curve.Y, Value.Y);
		SetFloatCurveToConstant(Curve.Z, Value.Z);
	}

	bool EnsureFloatDistributionHasCurve(FRawDistributionFloat& Distribution)
	{
		if (Distribution.Mode == EDistributionValueMode::ConstantCurve)
		{
			if (Distribution.ConstantCurve.IsEmpty())
			{
				SetFloatCurveToConstant(Distribution.ConstantCurve, Distribution.Constant);
				return true;
			}
			return false;
		}

		if (Distribution.Mode == EDistributionValueMode::UniformCurve)
		{
			bool bChanged = false;
			if (Distribution.MinCurve.IsEmpty())
			{
				SetFloatCurveToConstant(Distribution.MinCurve, Distribution.MinValue);
				bChanged = true;
			}
			if (Distribution.MaxCurve.IsEmpty())
			{
				SetFloatCurveToConstant(Distribution.MaxCurve, Distribution.MaxValue);
				bChanged = true;
			}
			return bChanged;
		}

		if (Distribution.Mode == EDistributionValueMode::Uniform)
		{
			Distribution.Mode = EDistributionValueMode::UniformCurve;
			SetFloatCurveToConstant(Distribution.MinCurve, Distribution.MinValue);
			SetFloatCurveToConstant(Distribution.MaxCurve, Distribution.MaxValue);
			return true;
		}

		Distribution.Mode = EDistributionValueMode::ConstantCurve;
		SetFloatCurveToConstant(Distribution.ConstantCurve, Distribution.Constant);
		return true;
	}

	bool EnsureVectorDistributionHasCurve(FRawDistributionVector& Distribution)
	{
		if (Distribution.Mode == EDistributionValueMode::ConstantCurve)
		{
			bool bChanged = false;
			if (Distribution.ConstantCurve.X.IsEmpty())
			{
				SetFloatCurveToConstant(Distribution.ConstantCurve.X, Distribution.Constant.X);
				bChanged = true;
			}
			if (Distribution.ConstantCurve.Y.IsEmpty())
			{
				SetFloatCurveToConstant(Distribution.ConstantCurve.Y, Distribution.Constant.Y);
				bChanged = true;
			}
			if (Distribution.ConstantCurve.Z.IsEmpty())
			{
				SetFloatCurveToConstant(Distribution.ConstantCurve.Z, Distribution.Constant.Z);
				bChanged = true;
			}
			return bChanged;
		}

		if (Distribution.Mode == EDistributionValueMode::UniformCurve)
		{
			bool bChanged = false;
			if (Distribution.MinCurve.X.IsEmpty() || Distribution.MinCurve.Y.IsEmpty() || Distribution.MinCurve.Z.IsEmpty())
			{
				SetVectorCurveToConstant(Distribution.MinCurve, Distribution.MinValue);
				bChanged = true;
			}
			if (Distribution.MaxCurve.X.IsEmpty() || Distribution.MaxCurve.Y.IsEmpty() || Distribution.MaxCurve.Z.IsEmpty())
			{
				SetVectorCurveToConstant(Distribution.MaxCurve, Distribution.MaxValue);
				bChanged = true;
			}
			return bChanged;
		}

		if (Distribution.Mode == EDistributionValueMode::Uniform)
		{
			Distribution.Mode = EDistributionValueMode::UniformCurve;
			SetVectorCurveToConstant(Distribution.MinCurve, Distribution.MinValue);
			SetVectorCurveToConstant(Distribution.MaxCurve, Distribution.MaxValue);
			return true;
		}

		Distribution.Mode = EDistributionValueMode::ConstantCurve;
		SetVectorCurveToConstant(Distribution.ConstantCurve, Distribution.Constant);
		return true;
	}

	bool HasModuleCurveDistribution(const UParticleModule* Module)
	{
		return Cast<UParticleModuleLifetime>(Module)
			|| Cast<UParticleModuleLocation>(Module)
			|| Cast<UParticleModuleVelocity>(Module)
			|| Cast<UParticleModuleSize>(Module);
	}

	ImU32 GetCurveTrackColor(int32 CurveIndex, bool bDim = false)
	{
		const ImU32 Palette[] =
		{
			IM_COL32(255, 70, 70, 255),
			IM_COL32(78, 235, 102, 255),
			IM_COL32(80, 135, 255, 255),
			IM_COL32(255, 210, 55, 255),
			IM_COL32(255, 70, 180, 255),
			IM_COL32(225, 230, 235, 255),
		};
		const ImU32 DimPalette[] =
		{
			IM_COL32(255, 70, 70, 90),
			IM_COL32(78, 235, 102, 90),
			IM_COL32(80, 135, 255, 90),
			IM_COL32(255, 210, 55, 90),
			IM_COL32(255, 70, 180, 90),
			IM_COL32(225, 230, 235, 90),
		};
		return bDim ? DimPalette[CurveIndex % IM_ARRAYSIZE(DimPalette)] : Palette[CurveIndex % IM_ARRAYSIZE(Palette)];
	}

	void DrawCurveIconGlyph(ImDrawList* DrawList, const ImVec2& Min, const ImVec2& Max, bool bMuted = false)
	{
		const ImU32 FillColor = bMuted ? IM_COL32(28, 32, 28, 210) : IM_COL32(10, 18, 12, 255);
		const ImU32 BorderColor = bMuted ? IM_COL32(92, 118, 88, 170) : IM_COL32(126, 205, 94, 255);
		const ImU32 GridColor = bMuted ? IM_COL32(82, 105, 78, 100) : IM_COL32(76, 120, 58, 145);
		const ImU32 LineColor = bMuted ? IM_COL32(134, 170, 110, 190) : IM_COL32(162, 245, 105, 255);
		DrawList->AddRectFilled(Min, Max, FillColor, 1.0f);
		DrawList->AddRect(Min, Max, BorderColor, 1.0f);

		const float W = Max.x - Min.x;
		const float H = Max.y - Min.y;
		for (int32 i = 1; i < 3; ++i)
		{
			const float X = Min.x + W * (static_cast<float>(i) / 3.0f);
			const float Y = Min.y + H * (static_cast<float>(i) / 3.0f);
			DrawList->AddLine(ImVec2(X, Min.y + 1.0f), ImVec2(X, Max.y - 1.0f), GridColor);
			DrawList->AddLine(ImVec2(Min.x + 1.0f, Y), ImVec2(Max.x - 1.0f, Y), GridColor);
		}

		DrawList->AddLine(ImVec2(Min.x + 2.0f, Max.y - 3.0f), ImVec2(Min.x + W * 0.45f, Min.y + H * 0.55f), LineColor, 1.2f);
		DrawList->AddLine(ImVec2(Min.x + W * 0.45f, Min.y + H * 0.55f), ImVec2(Max.x - 2.0f, Min.y + 2.0f), LineColor, 1.2f);
	}

	bool DrawCurveIconButton(const char* Id, const ImVec2& Size, bool bMuted = false)
	{
		const ImVec2 Pos = ImGui::GetCursorScreenPos();
		const bool bClicked = ImGui::InvisibleButton(Id, Size);
		const bool bHovered = ImGui::IsItemHovered();
		ImDrawList* DrawList = ImGui::GetWindowDrawList();
		const ImVec2 Min(Pos.x + 1.0f, Pos.y + 1.0f);
		const ImVec2 Max(Pos.x + Size.x - 1.0f, Pos.y + Size.y - 1.0f);
		DrawCurveIconGlyph(DrawList, Min, Max, bMuted);
		if (bHovered)
		{
			DrawList->AddRect(Pos, ImVec2(Pos.x + Size.x, Pos.y + Size.y), IM_COL32(235, 240, 210, 210), 2.0f);
			ImGui::SetTooltip("Show curves");
		}
		return bClicked;
	}

	ImVec2 ProjectWorldAxisToViewport(const FVector& Axis, const FMinimalViewInfo& POV)
	{
		const FVector CameraRight = POV.Rotation.GetRightVector();
		const FVector CameraUp = POV.Rotation.GetUpVector();
		return ImVec2(Axis.Dot(CameraRight), -Axis.Dot(CameraUp));
	}

	void DrawViewportAxisGizmo(ImDrawList* DrawList, const ImVec2& ViewportPos, const ImVec2& ViewportSize, FParticleSystemEditorViewportClient& ViewportClient)
	{
		FMinimalViewInfo POV;
		if (!ViewportClient.GetCameraView(POV))
		{
			return;
		}

		const float AxisLength = (std::max)(20.0f, (std::min)(30.0f, (std::min)(ViewportSize.x, ViewportSize.y) * 0.01f));
		const float Padding = AxisLength + 25.0f;
		const ImVec2 Origin(ViewportPos.x + Padding, ViewportPos.y + ViewportSize.y - Padding);
		const FVector CameraForward = POV.Rotation.GetForwardVector();

		struct FAxisDrawInfo
		{
			const char* Label = "";
			FVector Direction;
			ImU32 Color = 0;
			ImVec2 End;
			float Depth = 0.0f;
		};

		FAxisDrawInfo Axes[3] =
		{
			{ "X", FVector::XAxisVector, IM_COL32(255, 70, 55, 255), ImVec2(), 0.0f },
			{ "Y", FVector::YAxisVector, IM_COL32(105, 220, 70, 255), ImVec2(), 0.0f },
			{ "Z", FVector::ZAxisVector, IM_COL32(70, 135, 255, 255), ImVec2(), 0.0f },
		};

		for (FAxisDrawInfo& Axis : Axes)
		{
			const ImVec2 Projected = ProjectWorldAxisToViewport(Axis.Direction, POV);
			Axis.End = ImVec2(Origin.x + Projected.x * AxisLength, Origin.y + Projected.y * AxisLength);
			Axis.Depth = Axis.Direction.Dot(CameraForward);
		}

		std::sort(Axes, Axes + 3, [](const FAxisDrawInfo& A, const FAxisDrawInfo& B)
		{
			return A.Depth > B.Depth;
		});

		DrawList->AddCircleFilled(Origin, 3.0f, IM_COL32(22, 24, 26, 255));
		for (const FAxisDrawInfo& Axis : Axes)
		{
			DrawList->AddLine(Origin, Axis.End, Axis.Color, 2.0f);
			DrawList->AddCircleFilled(Axis.End, 2.5f, Axis.Color);

			const ImVec2 LabelDir(Axis.End.x - Origin.x, Axis.End.y - Origin.y);
			const float LabelLen = std::sqrt(LabelDir.x * LabelDir.x + LabelDir.y * LabelDir.y);
			const ImVec2 LabelOffset = LabelLen > 1.0f
				? ImVec2((LabelDir.x / LabelLen) * 15.0f, (LabelDir.y / LabelLen) * 15.0f)
				: ImVec2(4.0f, -10.0f);
			DrawList->AddText(ImVec2(Axis.End.x + LabelOffset.x - 4.0f, Axis.End.y + LabelOffset.y - 6.0f), Axis.Color, Axis.Label);
		}
	}

	void DrawPanelHeader(const char* Label, float MinWidth = 0.0f)
	{
		const ImVec2 Pos = ImGui::GetCursorScreenPos();
		const float Width = (std::max)(ImGui::GetContentRegionAvail().x, MinWidth);
		const float Height = 24.0f;
		ImDrawList* DrawList = ImGui::GetWindowDrawList();
		DrawList->AddRectFilled(Pos, ImVec2(Pos.x + Width, Pos.y + Height), IM_COL32(34, 34, 36, 255));
		DrawList->AddText(ImVec2(Pos.x + 8.0f, Pos.y + 4.0f), IM_COL32(220, 224, 232, 255), Label);
		ImGui::Dummy(ImVec2(Width, Height + 4.0f));
	}

	bool IsModuleOrderLocked(const UParticleModule* Module)
	{
		return Cast<UParticleModuleRequired>(Module) || Cast<UParticleModuleSpawn>(Module);
	}

	int32 GetFirstMovableModuleIndex(const TArray<UParticleModule*>& Modules)
	{
		int32 FirstMovableIndex = 0;
		for (int32 ModuleIndex = 0; ModuleIndex < static_cast<int32>(Modules.size()); ++ModuleIndex)
		{
			if (IsModuleOrderLocked(Modules[ModuleIndex]))
			{
				FirstMovableIndex = (std::max)(FirstMovableIndex, ModuleIndex + 1);
			}
		}
		return FirstMovableIndex;
	}

	int32 ClampModuleInsertIndex(const TArray<UParticleModule*>& Modules, int32 InsertIndex)
	{
		const int32 FirstMovableIndex = GetFirstMovableModuleIndex(Modules);
		const int32 ModuleCount = static_cast<int32>(Modules.size());
		return (std::max)(FirstMovableIndex, (std::min)(InsertIndex, ModuleCount));
	}

	bool CanDirectEditModulesInLOD(int32 LODIndex)
	{
		return LODIndex == 0;
	}

	const char* GetModuleEditStateLabel(EParticleModuleEditState State)
	{
		switch (State)
		{
		case EParticleModuleEditState::InheritedLocked: return "Inherited Locked";
		case EParticleModuleEditState::Duplicated: return "Duplicated";
		case EParticleModuleEditState::Shared: return "Shared";
		default: return "Unknown";
		}
	}

	ImU32 GetModuleRowBackgroundColor(const UParticleModule* Module, bool bSelected)
	{
		if (Cast<UParticleModuleRequired>(Module))
		{
			return bSelected ? IM_COL32(205, 205, 112, 255) : IM_COL32(188, 190, 88, 255);
		}
		if (Cast<UParticleModuleSpawn>(Module))
		{
			return bSelected ? IM_COL32(210, 112, 112, 255) : IM_COL32(188, 92, 92, 255);
		}
		return bSelected ? IM_COL32(78, 82, 92, 255) : IM_COL32(29, 30, 35, 255);
	}

	ImU32 GetModuleRowTextColor(const UParticleModule* Module, bool bDirectEditLocked)
	{
		(void)Module;
		if (bDirectEditLocked)
		{
			return IM_COL32(255, 255, 255, 255);
		}
		return IM_COL32(235, 238, 242, 255);
	}

	void DrawLockedModuleRowOverlay(ImDrawList* DrawList, const ImVec2& Pos, float Width)
	{
		const ImVec2 Max(Pos.x + Width, Pos.y + ModuleRowHeight);
		DrawList->AddRectFilled(Pos, Max, IM_COL32(0, 0, 0, 36));

		DrawList->PushClipRect(Pos, Max, true);
		for (float X = Pos.x - ModuleRowHeight; X < Pos.x + Width + ModuleRowHeight; X += 14.0f)
		{
			const ImVec2 LineStart(X, Pos.y + ModuleRowHeight);
			const ImVec2 LineEnd(X + ModuleRowHeight, Pos.y);
			DrawList->AddLine(
				LineStart,
				LineEnd,
				IM_COL32(10, 12, 14, 150),
				2.0f);
			DrawList->AddLine(
				ImVec2(LineStart.x + 1.0f, LineStart.y),
				ImVec2(LineEnd.x + 1.0f, LineEnd.y),
				IM_COL32(220, 224, 210, 72),
				1.0f);
		}
		DrawList->PopClipRect();
	}

	bool IsModuleEnableToggleAllowed(const UParticleModule* Module)
	{
		return Module
			&& !Cast<UParticleModuleRequired>(Module)
			&& !Cast<UParticleModuleTypeDataBase>(Module);
	}

	void DrawModuleRow(const char* Label, bool bSelected, const UParticleModule* Module, bool bDirectEditLocked = false)
	{
		const ImVec2 Pos = ImGui::GetCursorScreenPos();
		const float Width = ImGui::GetContentRegionAvail().x;
		constexpr float CheckSize = 13.0f;
		constexpr float CurveIconSize = 13.0f;
		constexpr float IconGap = 4.0f;
		constexpr float CheckPadding = 6.0f;
		const bool bHasCurveIcon = HasModuleCurveDistribution(Module);
		const bool bHasCheckSlot = IsModuleEnableToggleAllowed(Module);
		ImDrawList* DrawList = ImGui::GetWindowDrawList();
		const ImU32 BackgroundColor = GetModuleRowBackgroundColor(Module, bSelected);
		const ImU32 TextColor = GetModuleRowTextColor(Module, bDirectEditLocked);
		DrawList->AddRectFilled(Pos, ImVec2(Pos.x + Width, Pos.y + ModuleRowHeight), BackgroundColor);
		auto DrawEnabledCheck = [&]()
		{
			if (!IsModuleEnableToggleAllowed(Module))
			{
				return;
			}

			const float CheckRight = Pos.x + Width - CheckPadding - (bHasCheckSlot ? (CurveIconSize + IconGap) : 0.0f);
			const ImVec2 CheckMin(CheckRight - CheckSize, Pos.y + (ModuleRowHeight - CheckSize) * 0.5f);
			const ImVec2 CheckMax(CheckMin.x + CheckSize, CheckMin.y + CheckSize);
			const ImU32 FillColor = Module->IsEnabled() ? IM_COL32(58, 65, 72, 255) : IM_COL32(24, 26, 30, 255);
			const ImU32 BorderColor = bDirectEditLocked ? IM_COL32(130, 134, 138, 190) : IM_COL32(205, 210, 216, 230);
			DrawList->AddRectFilled(CheckMin, CheckMax, FillColor, 1.0f);
			DrawList->AddRect(CheckMin, CheckMax, BorderColor, 1.0f);
			if (Module->IsEnabled())
			{
				DrawList->AddLine(ImVec2(CheckMin.x + 3.0f, CheckMin.y + 7.0f), ImVec2(CheckMin.x + 5.8f, CheckMin.y + 10.0f), IM_COL32(235, 238, 242, 255), 1.4f);
				DrawList->AddLine(ImVec2(CheckMin.x + 5.8f, CheckMin.y + 10.0f), ImVec2(CheckMin.x + 10.0f, CheckMin.y + 3.5f), IM_COL32(235, 238, 242, 255), 1.4f);
			}
		};
		auto DrawCurveButton = [&]()
		{
			if (!bHasCurveIcon)
			{
				return;
			}

			const ImVec2 IconMin(Pos.x + Width - CheckPadding - CurveIconSize, Pos.y + (ModuleRowHeight - CurveIconSize) * 0.5f);
			DrawCurveIconGlyph(DrawList, IconMin, ImVec2(IconMin.x + CurveIconSize, IconMin.y + CurveIconSize), bDirectEditLocked);
		};

		if (bDirectEditLocked)
		{
			DrawLockedModuleRowOverlay(DrawList, Pos, Width);
			const ImVec2 TextPos(Pos.x + 8.0f, Pos.y + 4.0f);
			DrawList->AddText(ImVec2(TextPos.x + 1.0f, TextPos.y + 1.0f), IM_COL32(0, 0, 0, 220), Label);
			DrawList->AddText(TextPos, TextColor, Label);
			DrawEnabledCheck();
			DrawCurveButton();
			return;
		}
		DrawList->AddText(ImVec2(Pos.x + 8.0f, Pos.y + 4.0f), TextColor, Label);
		DrawEnabledCheck();
		DrawCurveButton();
	}

	void StartModuleDragSource(const FParticleModuleDragPayload& DragPayload, const char* Label)
	{
		if (!DragPayload.Module || IsModuleOrderLocked(DragPayload.Module))
		{
			return;
		}

		if (ImGui::BeginDragDropSource())
		{
			ImGui::SetDragDropPayload(ParticleModuleDragPayloadType, &DragPayload, sizeof(DragPayload));
			ImGui::TextUnformatted(Label);
			ImGui::EndDragDropSource();
		}
	}

	void AcceptModuleDropTarget(
		FParticleModuleDropRequest& DropRequest,
		int32 TargetEmitterIndex,
		int32 TargetLODIndex,
		int32 InsertBeforeIndex,
		int32 InsertAfterIndex)
	{
		const ImVec2 TargetMin = ImGui::GetItemRectMin();
		const ImVec2 TargetMax = ImGui::GetItemRectMax();
		const bool bDropAfter = ImGui::GetIO().MousePos.y > (TargetMin.y + TargetMax.y) * 0.5f;
		const int32 TargetInsertIndex = bDropAfter ? InsertAfterIndex : InsertBeforeIndex;

		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload(ParticleModuleDragPayloadType, ImGuiDragDropFlags_AcceptBeforeDelivery))
			{
				const float DropY = bDropAfter ? TargetMax.y : TargetMin.y;
				ImGui::GetWindowDrawList()->AddLine(
					ImVec2(TargetMin.x, DropY),
					ImVec2(TargetMax.x, DropY),
					IM_COL32(120, 180, 255, 255),
					2.0f);

				if (Payload->IsDelivery() && Payload->DataSize == sizeof(FParticleModuleDragPayload))
				{
					DropRequest.bRequested = true;
					DropRequest.Payload = *static_cast<const FParticleModuleDragPayload*>(Payload->Data);
					DropRequest.TargetEmitterIndex = TargetEmitterIndex;
					DropRequest.TargetLODIndex = TargetLODIndex;
					DropRequest.TargetInsertIndex = TargetInsertIndex;
				}
			}
			ImGui::EndDragDropTarget();
		}
	}

	void RenderModuleContextMenu(
		FModuleRowAction& Action,
		bool bCanDelete,
		bool bCanOverrideFromHigher,
		bool bCanOverrideFromHighest,
		const ImVec2& RowMin,
		const ImVec2& RowMax)
	{
		if (ImGui::IsMouseReleased(ImGuiMouseButton_Right) && ImGui::IsMouseHoveringRect(RowMin, RowMax))
		{
			ImGui::OpenPopup("##ModuleContextMenu");
			Action.bContextMenuOpen = true;
		}

		if (ImGui::BeginPopup("##ModuleContextMenu"))
		{
			Action.bContextMenuOpen = true;

			ImGui::BeginDisabled(!bCanDelete);
			if (ImGui::MenuItem("Delete Module"))
			{
				Action.bDelete = true;
			}
			ImGui::EndDisabled();

			if (ImGui::MenuItem("Refresh Module"))
			{
				Action.bRefresh = true;
			}

			ImGui::BeginDisabled(!bCanOverrideFromHigher);
			if (ImGui::MenuItem("Duplicate From Higher"))
			{
				Action.bDuplicateFromHigher = true;
			}
			if (ImGui::MenuItem("Share From Higher"))
			{
				Action.bShareFromHigher = true;
			}
			ImGui::EndDisabled();

			ImGui::BeginDisabled(!bCanOverrideFromHighest);
			if (ImGui::MenuItem("Duplicate From Highest"))
			{
				Action.bDuplicateFromHighest = true;
			}
			ImGui::EndDisabled();

			ImGui::EndPopup();
		}
	}

	bool SelectableModuleRow(const char* Label, bool bSelected, const UParticleModule* Module = nullptr, bool bDirectEditLocked = false)
	{
		DrawModuleRow(Label, bSelected, Module, bDirectEditLocked);
		return ImGui::InvisibleButton("##ModuleRow", ImVec2(ImGui::GetContentRegionAvail().x, ModuleRowHeight));
	}

	bool IsModuleDeleteLocked(const UParticleModule* Module)
	{
		return Cast<UParticleModuleRequired>(Module) || Cast<UParticleModuleSpawn>(Module);
	}

	FModuleRowAction EditableModuleRow(
		const char* Label,
		const UParticleModule* Module,
		bool bSelected,
		bool bCanDelete,
		bool bDirectEditLocked,
		bool bCanOverrideFromHigher,
		bool bCanOverrideFromHighest,
		const FParticleModuleDragPayload* DragPayload,
		FParticleModuleDropRequest* DropRequest,
		int32 TargetEmitterIndex,
		int32 TargetLODIndex,
		int32 InsertBeforeIndex,
		int32 InsertAfterIndex)
	{
		FModuleRowAction Action;

		const ImVec2 Pos = ImGui::GetCursorScreenPos();
		const float Width = ImGui::GetContentRegionAvail().x;
		constexpr float EnableHitWidth = 24.0f;
		constexpr float CurveHitWidth = 22.0f;
		const bool bCanToggleEnabled = IsModuleEnableToggleAllowed(Module) && !bDirectEditLocked;
		const bool bCanUseCurveButton = HasModuleCurveDistribution(Module) && !bDirectEditLocked;
		const bool bReserveCurveSlot = IsModuleEnableToggleAllowed(Module);

		DrawModuleRow(Label, bSelected, Module, bDirectEditLocked);
		float SelectWidth = Width;
		if (bCanUseCurveButton)
		{
			ImGui::SetCursorScreenPos(ImVec2(Pos.x + Width - CurveHitWidth, Pos.y));
			Action.bShowCurves = ImGui::InvisibleButton("##ShowModuleCurves", ImVec2(CurveHitWidth, ModuleRowHeight));
		}
		if (bReserveCurveSlot)
		{
			SelectWidth -= CurveHitWidth;
		}
		if (bCanToggleEnabled)
		{
			ImGui::SetCursorScreenPos(ImVec2(Pos.x + SelectWidth - EnableHitWidth, Pos.y));
			Action.bToggleEnabled = ImGui::InvisibleButton("##ToggleModuleEnabled", ImVec2(EnableHitWidth, ModuleRowHeight));
			SelectWidth -= EnableHitWidth;
		}
		ImGui::SetCursorScreenPos(Pos);
		Action.bSelect = ImGui::InvisibleButton("##SelectModuleRow", ImVec2((std::max)(1.0f, SelectWidth), ModuleRowHeight));
		RenderModuleContextMenu(Action, bCanDelete, bCanOverrideFromHigher, bCanOverrideFromHighest, Pos, ImVec2(Pos.x + Width, Pos.y + ModuleRowHeight));
		if (DragPayload)
		{
			StartModuleDragSource(*DragPayload, Label);
		}
		if (DropRequest)
		{
			AcceptModuleDropTarget(*DropRequest, TargetEmitterIndex, TargetLODIndex, InsertBeforeIndex, InsertAfterIndex);
		}

		ImGui::SetCursorScreenPos(ImVec2(Pos.x, Pos.y + ModuleRowHeight));
		return Action;
	}

	void DrawDetailRow(const char* Label, const char* Value)
	{
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::SetWindowFontScale(0.92f);
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(Label);
		ImGui::SetWindowFontScale(1.0f);

		ImGui::TableSetColumnIndex(1);
		ImGui::AlignTextToFramePadding();
		ImGui::TextDisabled("%s", Value ? Value : "");
	}

	void DrawDetailRowF(const char* Label, const char* Format, ...)
	{
		char Buffer[256];
		va_list Args;
		va_start(Args, Format);
		std::vsnprintf(Buffer, sizeof(Buffer), Format, Args);
		va_end(Args);
		DrawDetailRow(Label, Buffer);
	}

	bool DrawDetailFloatInputRow(const char* Label, float& Value, const char* Format = "%.3f", bool bReadOnly = false)
	{
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::SetWindowFontScale(0.92f);
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(Label);
		ImGui::SetWindowFontScale(1.0f);

		ImGui::TableSetColumnIndex(1);
		ImGui::BeginDisabled(bReadOnly);
		ImGui::SetNextItemWidth(DetailsScalarInputWidth);
		const bool bChanged = ImGui::InputFloat("##Value", &Value, 0.0f, 0.0f, Format);
		ImGui::EndDisabled();
		return bChanged && !bReadOnly;
	}

	bool DrawDetailIntInputRow(const char* Label, int32& Value, bool bReadOnly = false)
	{
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::SetWindowFontScale(0.92f);
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(Label);
		ImGui::SetWindowFontScale(1.0f);

		ImGui::TableSetColumnIndex(1);
		ImGui::BeginDisabled(bReadOnly);
		ImGui::SetNextItemWidth(DetailsScalarInputWidth);
		const bool bChanged = ImGui::InputInt("##Value", &Value, 0, 0);
		ImGui::EndDisabled();
		return bChanged && !bReadOnly;
	}

	bool DrawDetailsCategoryHeader(const char* Label)
	{
		ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.22f, 0.22f, 0.22f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.27f, 0.27f, 0.27f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.30f, 0.30f, 0.30f, 1.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5.0f, 3.0f));

		const bool bOpen = ImGui::CollapsingHeader(Label, ImGuiTreeNodeFlags_DefaultOpen);

		ImGui::PopStyleVar();
		ImGui::PopStyleColor(3);
		return bOpen;
	}

	FString GetParticleEditorToolIconPath(const wchar_t* FileName)
	{
		return FPaths::ToUtf8(FPaths::Combine(FPaths::AssetDir(), L"Editor/ToolIcons/", FileName));
	}

	ID3D11ShaderResourceView* GetParticleEditorToolIcon(const wchar_t* FileName)
	{
		return FEditorTextureManager::Get().GetOrLoadIcon(GetParticleEditorToolIconPath(FileName));
	}

	bool DrawParticleToolbarIconButton(const char* Id, const wchar_t* IconFileName, const char* Label, const char* Tooltip)
	{
		constexpr float IconSize = 16.0f;
		constexpr float ButtonHeight = 26.0f;
		constexpr float PaddingX = 7.0f;
		constexpr float IconTextGap = 5.0f;

		const bool bHasLabel = Label && Label[0] != '\0';
		const ImVec2 TextSize = bHasLabel ? ImGui::CalcTextSize(Label) : ImVec2(0.0f, 0.0f);
		const float ButtonWidth = PaddingX + IconSize + (bHasLabel ? IconTextGap + TextSize.x : 0.0f) + PaddingX;
		const ImVec2 Pos = ImGui::GetCursorScreenPos();
		const bool bClicked = ImGui::InvisibleButton(Id, ImVec2(ButtonWidth, ButtonHeight));
		const bool bHovered = ImGui::IsItemHovered();
		const bool bActive = ImGui::IsItemActive();

		ImDrawList* DrawList = ImGui::GetWindowDrawList();
		const ImU32 BackgroundColor = ImGui::GetColorU32(bActive ? ImGuiCol_ButtonActive : (bHovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button));
		DrawList->AddRectFilled(Pos, ImVec2(Pos.x + ButtonWidth, Pos.y + ButtonHeight), BackgroundColor, 2.0f);

		const ImVec2 IconMin(Pos.x + PaddingX, Pos.y + (ButtonHeight - IconSize) * 0.5f);
		if (ID3D11ShaderResourceView* Icon = GetParticleEditorToolIcon(IconFileName))
		{
			DrawList->AddImage(reinterpret_cast<ImTextureID>(Icon), IconMin, ImVec2(IconMin.x + IconSize, IconMin.y + IconSize));
		}
		else
		{
			DrawList->AddRect(IconMin, ImVec2(IconMin.x + IconSize, IconMin.y + IconSize), ImGui::GetColorU32(ImGuiCol_TextDisabled));
		}

		if (bHasLabel)
		{
			const ImVec2 TextPos(IconMin.x + IconSize + IconTextGap, Pos.y + (ButtonHeight - TextSize.y) * 0.5f);
			DrawList->AddText(TextPos, ImGui::GetColorU32(ImGuiCol_Text), Label);
		}
		if (Tooltip && bHovered)
		{
			ImGui::SetTooltip("%s", Tooltip);
		}

		return bClicked;
	}

	void DrawToolbarGroupSeparator()
	{
		constexpr float SeparatorHeight = 24.0f;
		ImGui::SameLine(0.0f, 8.0f);
		const ImVec2 Pos = ImGui::GetCursorScreenPos();
		ImDrawList* DrawList = ImGui::GetWindowDrawList();
		DrawList->AddLine(
			ImVec2(Pos.x, Pos.y + 1.0f),
			ImVec2(Pos.x, Pos.y + SeparatorHeight - 1.0f),
			IM_COL32(82, 84, 90, 180),
			1.0f);
		ImGui::Dummy(ImVec2(1.0f, SeparatorHeight));
		ImGui::SameLine(0.0f, 8.0f);
	}

	bool BeginDetailsTable(const char* Id)
	{
		const ImGuiTableFlags Flags =
			ImGuiTableFlags_SizingStretchProp |
			ImGuiTableFlags_BordersInnerV |
			ImGuiTableFlags_PadOuterX |
			ImGuiTableFlags_RowBg;

		if (!ImGui::BeginTable(Id, 2, Flags))
		{
			return false;
		}

		ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, DetailsNameColumnWidth);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
		ImGui::PushStyleColor(ImGuiCol_TableRowBg, ImVec4(0.13f, 0.13f, 0.13f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, ImVec4(0.145f, 0.145f, 0.145f, 1.0f));
		return true;
	}

	void EndDetailsTable()
	{
		ImGui::EndTable();
		ImGui::PopStyleColor(2);
	}

	const char* GetRenderTypeLabel(EParticleRenderType RenderType)
	{
		switch (RenderType)
		{
		case EParticleRenderType::Sprite: return "Sprite";
		case EParticleRenderType::Mesh: return "Mesh";
		case EParticleRenderType::Ribbon: return "Ribbon";
		case EParticleRenderType::Beam: return "Beam";
		case EParticleRenderType::GPU: return "GPU";
		default: return "Unknown";
		}
	}

	EParticleRenderType GetLODRenderType(const UParticleLODLevel* LODLevel)
	{
		const UParticleModuleTypeDataBase* TypeDataModule = LODLevel ? LODLevel->GetTypeDataModule() : nullptr;
		return TypeDataModule ? TypeDataModule->GetRenderType() : EParticleRenderType::Sprite;
	}

	const char* GetTypeDataDisplayName(const UParticleModuleTypeDataBase* TypeDataModule)
	{
		if (!TypeDataModule)
		{
			return "";
		}

		switch (TypeDataModule->GetRenderType())
		{
		case EParticleRenderType::Sprite: return "";
		case EParticleRenderType::Mesh: return "Mesh Data";
		case EParticleRenderType::Ribbon: return "Ribbon Data";
		case EParticleRenderType::Beam: return "Beam Data";
		case EParticleRenderType::GPU: return "GPU Sprites";
		default: return TypeDataModule->GetClass()->GetName();
		}
	}

	const char* GetModuleDisplayName(const UParticleModule* Module)
	{
		if (!Module)
		{
			return "(null module)";
		}
		if (Cast<UParticleModuleRequired>(Module)) return "Required";
		if (Cast<UParticleModuleSpawn>(Module)) return "Spawn";
		if (Cast<UParticleModuleLifetime>(Module)) return "Lifetime";
		if (Cast<UParticleModuleLocation>(Module)) return "Initial Location";
		if (Cast<UParticleModuleVelocity>(Module)) return "Initial Velocity";
		if (Cast<UParticleModuleAcceleration>(Module)) return "Acceleration";
		if (Cast<UParticleModuleColor>(Module)) return "Color Over Life";
		if (Cast<UParticleModuleSize>(Module)) return "Initial Size";
		if (Cast<UParticleModuleSubImageIndex>(Module)) return "Sub Image Index";
		if (Cast<UParticleModuleBeamSource>(Module)) return "Beam Source";
		if (Cast<UParticleModuleBeamNoise>(Module)) return "Beam Noise";
		if (Cast<UParticleModuleBeamTarget>(Module)) return "Beam Target";
		if (Cast<UParticleModuleCollision>(Module)) return "Collision";
		return Module->GetClass()->GetName();
	}

	UParticleEmitter* DuplicateParticleEmitter(UParticleEmitter* SourceEmitter)
	{
		if (!SourceEmitter)
		{
			return nullptr;
		}

		FMemoryArchive Writer(/*bInIsSaving=*/true);
		SourceEmitter->Serialize(Writer);

		UParticleEmitter* DuplicatedEmitter = UObjectManager::Get().CreateObject<UParticleEmitter>();
		if (!DuplicatedEmitter)
		{
			return nullptr;
		}

		const FName UniqueName = DuplicatedEmitter->GetFName();
		FMemoryArchive Reader(Writer.GetBuffer(), /*bInIsSaving=*/false);
		DuplicatedEmitter->Serialize(Reader);
		DuplicatedEmitter->SetFName(UniqueName);

		const TArray<UParticleLODLevel*>& SourceLODLevels = SourceEmitter->GetLODLevels();
		TArray<UParticleLODLevel*>& DuplicatedLODLevels = DuplicatedEmitter->GetMutableLODLevels();
		const int32 LODCopyCount = (std::min)(static_cast<int32>(SourceLODLevels.size()), static_cast<int32>(DuplicatedLODLevels.size()));
		for (int32 LODIndex = 0; LODIndex < LODCopyCount; ++LODIndex)
		{
			if (!SourceLODLevels[LODIndex] || !DuplicatedLODLevels[LODIndex])
			{
				continue;
			}

			DuplicatedLODLevels[LODIndex]->GetMutableModuleEditStates() = SourceLODLevels[LODIndex]->GetModuleEditStates();
			DuplicatedLODLevels[LODIndex]->NormalizeModuleEditStates(LODIndex == 0 ? EParticleModuleEditState::Duplicated : EParticleModuleEditState::InheritedLocked);
		}
		return DuplicatedEmitter;
	}

	UParticleModule* DuplicateParticleModule(UParticleModule* SourceModule)
	{
		if (!SourceModule)
		{
			return nullptr;
		}

		UObject* NewObject = FObjectFactory::Get().Create(SourceModule->GetClass()->GetName(), nullptr);
		UParticleModule* DuplicatedModule = Cast<UParticleModule>(NewObject);
		if (!DuplicatedModule)
		{
			if (NewObject)
			{
				UObjectManager::Get().DestroyObject(NewObject);
			}
			return nullptr;
		}

		FMemoryArchive Writer(/*bInIsSaving=*/true);
		SourceModule->SerializeProperties(Writer, PF_Save);

		FMemoryArchive Reader(Writer.GetBuffer(), /*bInIsSaving=*/false);
		DuplicatedModule->SerializeProperties(Reader, PF_Save);
		return DuplicatedModule;
	}

	UParticleLODLevel* CreateEditorDefaultLODLevel(const UParticleEmitter* Emitter)
	{
		UParticleLODLevel* LODLevel = UObjectManager::Get().CreateObject<UParticleLODLevel>();
		if (!LODLevel)
		{
			return nullptr;
		}

		LODLevel->SetLevel(0);
		LODLevel->SetEnabled(true);

		UParticleModuleRequired* RequiredModule = UObjectManager::Get().CreateObject<UParticleModuleRequired>();
		if (RequiredModule)
		{
			RequiredModule->EmitterDuration = Emitter ? Emitter->GetEmitterDuration() : 1.0f;
			RequiredModule->bLooping = Emitter ? Emitter->IsLooping() : true;
			LODLevel->GetMutableModules().push_back(RequiredModule);
		}

		LODLevel->GetMutableModules().push_back(UObjectManager::Get().CreateObject<UParticleModuleSpawn>());
		LODLevel->GetMutableModules().push_back(UObjectManager::Get().CreateObject<UParticleModuleLifetime>());
		LODLevel->GetMutableModules().push_back(UObjectManager::Get().CreateObject<UParticleModuleSize>());
		LODLevel->GetMutableModules().push_back(UObjectManager::Get().CreateObject<UParticleModuleVelocity>());
		LODLevel->GetMutableModules().push_back(UObjectManager::Get().CreateObject<UParticleModuleColor>());
		LODLevel->SetAllModuleEditStates(EParticleModuleEditState::Duplicated);
		return LODLevel;
	}

	UParticleLODLevel* DuplicateParticleLODLevel(UParticleLODLevel* SourceLODLevel, const UParticleEmitter* Emitter)
	{
		if (!SourceLODLevel)
		{
			return CreateEditorDefaultLODLevel(Emitter);
		}

		FMemoryArchive Writer(/*bInIsSaving=*/true);
		SourceLODLevel->Serialize(Writer);

		UParticleLODLevel* DuplicatedLODLevel = UObjectManager::Get().CreateObject<UParticleLODLevel>();
		if (!DuplicatedLODLevel)
		{
			return nullptr;
		}

		const FName UniqueName = DuplicatedLODLevel->GetFName();
		FMemoryArchive Reader(Writer.GetBuffer(), /*bInIsSaving=*/false);
		DuplicatedLODLevel->Serialize(Reader);
		DuplicatedLODLevel->SetFName(UniqueName);
		DuplicatedLODLevel->GetMutableModuleEditStates() = SourceLODLevel->GetModuleEditStates();
		DuplicatedLODLevel->NormalizeModuleEditStates(EParticleModuleEditState::InheritedLocked);
		return DuplicatedLODLevel;
	}

	int32 ClampSystemLODIndex(const UParticleSystem* ParticleSystem, int32 LODIndex)
	{
		const int32 LODCount = ParticleSystem ? ParticleSystem->GetLODCount() : 0;
		if (LODCount <= 0)
		{
			return 0;
		}

		return (std::max)(0, (std::min)(LODIndex, LODCount - 1));
	}

	int32 ResolveSelectionEmitterIndex(const UParticleSystem* ParticleSystem, int32 PreferredEmitterIndex)
	{
		if (!ParticleSystem)
		{
			return -1;
		}

		const TArray<UParticleEmitter*>& Emitters = ParticleSystem->GetEmitters();
		if (PreferredEmitterIndex >= 0 && PreferredEmitterIndex < static_cast<int32>(Emitters.size()) && Emitters[PreferredEmitterIndex])
		{
			return PreferredEmitterIndex;
		}

		for (int32 EmitterIndex = 0; EmitterIndex < static_cast<int32>(Emitters.size()); ++EmitterIndex)
		{
			if (Emitters[EmitterIndex])
			{
				return EmitterIndex;
			}
		}

		return -1;
	}

	float CalculateNextLODDistance(const UParticleSystem* ParticleSystem)
	{
		constexpr float DefaultLODDistanceStep = 1000.0f;
		if (!ParticleSystem)
		{
			return DefaultLODDistanceStep;
		}

		float MaxDistance = 0.0f;
		for (const float Distance : ParticleSystem->GetLODDistances())
		{
			MaxDistance = (std::max)(MaxDistance, Distance);
		}
		return MaxDistance + DefaultLODDistanceStep;
	}

	float CalculateInsertedLODDistance(const UParticleSystem* ParticleSystem, int32 InsertIndex)
	{
		constexpr float DefaultLODDistanceStep = 1000.0f;
		if (!ParticleSystem || ParticleSystem->GetLODDistances().empty())
		{
			return 0.0f;
		}

		const TArray<float>& LODDistances = ParticleSystem->GetLODDistances();
		const int32 LODCount = static_cast<int32>(LODDistances.size());
		if (InsertIndex <= 0)
		{
			return 0.0f;
		}
		if (InsertIndex >= LODCount)
		{
			return CalculateNextLODDistance(ParticleSystem);
		}

		const float PreviousDistance = LODDistances[InsertIndex - 1];
		const float NextDistance = LODDistances[InsertIndex];
		return NextDistance > PreviousDistance ? (PreviousDistance + NextDistance) * 0.5f : PreviousDistance + DefaultLODDistanceStep;
	}

	FString GetParticleSystemTitle(const UParticleSystem* ParticleSystem, bool bDirty)
	{
		FString Title = "Particle System Editor";
		if (ParticleSystem)
		{
			const FString& AssetPath = ParticleSystem->GetAssetPathFileName();
			if (!AssetPath.empty() && AssetPath != "None")
			{
				Title += " - ";
				Title += AssetPath;
			}
		}
		if (bDirty)
		{
			Title += " *";
		}
		return Title;
	}
}

static uint32 GNextParticleSystemEditorInstanceId = 0;

struct FParticleSystemEditorWidget::FEditorLayoutSizes
{
	float LeftWidth = 0.0f;
	float RightWidth = 0.0f;
	float ViewportHeight = 0.0f;
	float DetailsHeight = 0.0f;
	float EmittersHeight = 0.0f;
	float CurveEditorHeight = 0.0f;
};

FParticleSystemEditorWidget::FParticleSystemEditorWidget()
	: InstanceId(GNextParticleSystemEditorInstanceId++)
{
	const FString Id = std::to_string(InstanceId);
	PreviewWorldHandle = FName("ParticleSystemEditorPreview_" + Id);
	WindowIdSuffix = "###ParticleSystemEditor_" + Id;
}

FParticleSystemEditorWidget::~FParticleSystemEditorWidget()
{
	DestroyPreviewWorld();
}

bool FParticleSystemEditorWidget::CanEdit(UObject* Object) const
{
	return Object && Object->IsA<UParticleSystem>();
}

void FParticleSystemEditorWidget::Open(UObject* Object)
{
	if (!CanEdit(Object))
	{
		return;
	}

	DestroyPreviewWorld();
	FAssetEditorWidget::Open(Object);
	ResetEditorState();
	CreatePreviewWorld();
}

void FParticleSystemEditorWidget::Close()
{
	ResetEditorState();
	DestroyPreviewWorld();
	FAssetEditorWidget::Close();
}

void FParticleSystemEditorWidget::Tick(float DeltaTime)
{
	if (!IsOpen())
	{
		return;
	}

	if (ViewportClient.IsRenderable())
	{
		ViewportClient.Tick(DeltaTime);
	}

	if (PreviewParticleComponent)
	{
		const int32 DesiredPreviewLODIndex = GetCurrentSystemLODIndex(GetParticleSystem());
		if (PreviewParticleComponent->GetPreviewLODIndex() != DesiredPreviewLODIndex)
		{
			PreviewParticleComponent->SetPreviewLODIndex(DesiredPreviewLODIndex);
			ViewState.bRestartPreviewRequested = true;
		}
	}

	if (ViewState.bRestartPreviewRequested)
	{
		RestartPreviewSimulation();
		ViewState.bRestartPreviewRequested = false;
	}

	if (!ViewState.bPreviewPlaying)
	{
		return;
	}

	const float SimDeltaTime = DeltaTime * (std::max)(0.0f, ViewState.PreviewAnimSpeed);
	if (UWorld* PreviewWorld = ViewportClient.GetPreviewWorld())
	{
		PreviewWorld->Tick(SimDeltaTime, ELevelTick::LEVELTICK_All);
	}

	ViewState.PreviewTime += SimDeltaTime;
}

void FParticleSystemEditorWidget::CollectPreviewViewports(TArray<IEditorPreviewViewportClient*>& OutClients) const
{
	if (IsOpen())
	{
		OutClients.push_back(const_cast<FParticleSystemEditorViewportClient*>(&ViewportClient));
	}
}

void FParticleSystemEditorWidget::Render(float DeltaTime)
{
	(void)DeltaTime;

	UParticleSystem* ParticleSystem = GetParticleSystem();
	if (!IsOpen() || !ParticleSystem)
	{
		return;
	}
	ValidateSelectionState(ParticleSystem);

	bool bWindowOpen = true;
	FString VisibleTitle = GetParticleSystemTitle(ParticleSystem, IsDirty());
	FString WindowTitle = VisibleTitle + WindowIdSuffix;
	ImGuiWindowFlags WindowFlags = ImGuiWindowFlags_None;
	if (ViewportClient.IsMouseOverViewport())
	{
		WindowFlags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;
	}

	ImGui::SetNextWindowSize(ImVec2(1120.0f, 720.0f), ImGuiCond_Once);
	if (ConsumeFocusRequest())
	{
		ImGui::SetNextWindowFocus();
	}

	if (!ImGui::Begin(WindowTitle.c_str(), &bWindowOpen, WindowFlags))
	{
		ImGui::End();
		if (!bWindowOpen)
		{
			Close();
		}
		return;
	}

	if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
	{
		FSlateApplication::Get().BringViewportToFront(&ViewportClient);
	}

	RenderToolbar();
	ImGui::Separator();

	const ImVec2 Available = ImGui::GetContentRegionAvail();
	const FEditorLayoutSizes Layout = CalculateLayoutSizes(Available);

	ImGui::BeginChild("##ParticleEditorLeftColumn", ImVec2(Layout.LeftWidth, 0.0f), ImGuiChildFlags_None);
	RenderViewportPanel(ImVec2(0.0f, Layout.ViewportHeight));
	const float LeftSplitTotalHeight = Layout.ViewportHeight + Layout.DetailsHeight;
	if (DrawSplitterHandle("##ParticleViewportDetailsSplitter", ImVec2(ImGui::GetContentRegionAvail().x, SplitterThickness), false))
	{
		ViewState.ViewportDetailsSplitRatio = CalculateSplitRatio(
			Layout.ViewportHeight + ImGui::GetIO().MouseDelta.y,
			LeftSplitTotalHeight,
			MinViewportHeight,
			MinDetailsHeight);
	}
	RenderDetailsPanel(ImVec2(0.0f, Layout.DetailsHeight));
	ImGui::EndChild();

	ImGui::SameLine(0.0f, 0.0f);
	const float MainSplitTotalWidth = Layout.LeftWidth + Layout.RightWidth;
	if (DrawSplitterHandle("##ParticleEditorMainSplitter", ImVec2(SplitterThickness, ImGui::GetContentRegionAvail().y), true))
	{
		ViewState.MainSplitRatio = CalculateSplitRatio(
			Layout.LeftWidth + ImGui::GetIO().MouseDelta.x,
			MainSplitTotalWidth,
			MinColumnWidth,
			MinColumnWidth);
	}

	ImGui::SameLine(0.0f, 0.0f);
	ImGui::BeginChild("##ParticleEditorRightColumn", ImVec2(Layout.RightWidth, 0.0f), ImGuiChildFlags_None);
	if (ViewState.bShowCurveEditor)
	{
		RenderEmittersPanel(ImVec2(0.0f, Layout.EmittersHeight));
		const float RightSplitTotalHeight = Layout.EmittersHeight + Layout.CurveEditorHeight;
		if (DrawSplitterHandle("##ParticleEmittersCurveSplitter", ImVec2(ImGui::GetContentRegionAvail().x, SplitterThickness), false))
		{
			ViewState.EmittersCurveSplitRatio = CalculateSplitRatio(
				Layout.EmittersHeight + ImGui::GetIO().MouseDelta.y,
				RightSplitTotalHeight,
				MinEmittersHeight,
				MinCurveEditorHeight);
		}
		RenderCurveEditorPanel(ImVec2(0.0f, Layout.CurveEditorHeight));
	}
	else
	{
		RenderEmittersPanel(ImVec2(0.0f, 0.0f));
	}
	ImGui::EndChild();

	ImGui::End();

	if (!bWindowOpen)
	{
		Close();
	}
}

UParticleSystem* FParticleSystemEditorWidget::GetParticleSystem() const
{
	if (!EditedObject || !EditedObject->IsA<UParticleSystem>())
	{
		return nullptr;
	}

	return static_cast<UParticleSystem*>(EditedObject);
}

void FParticleSystemEditorWidget::ResetEditorState()
{
	ViewState = FEditorViewState{};
	ClearSelectedCurve();
}

void FParticleSystemEditorWidget::ValidateSelectionState(const UParticleSystem* ParticleSystem)
{
	if (!ParticleSystem)
	{
		SelectParticleSystem();
		return;
	}

	const TArray<UParticleEmitter*>& Emitters = ParticleSystem->GetEmitters();
	if (Emitters.empty())
	{
		ViewState.SoloEmitterIndex = -1;
		SelectParticleSystem();
		return;
	}

	if (ViewState.SoloEmitterIndex >= static_cast<int32>(Emitters.size()))
	{
		ViewState.SoloEmitterIndex = -1;
	}

	FEditorSelectionState& Selection = ViewState.Selection;
	const int32 SystemLODCount = ParticleSystem->GetLODCount();
	if (SystemLODCount <= 0)
	{
		Selection.LODIndex = 0;
		if (Selection.Kind == ESelectionKind::LOD || Selection.Kind == ESelectionKind::Module)
		{
			SelectEmitter(Selection.EmitterIndex);
		}
		return;
	}

	Selection.LODIndex = ClampSystemLODIndex(ParticleSystem, Selection.LODIndex);

	if (Selection.EmitterIndex < 0 || Selection.EmitterIndex >= static_cast<int32>(Emitters.size()))
	{
		if (Selection.Kind == ESelectionKind::ParticleSystem)
		{
			return;
		}
		SelectEmitter(0);
		return;
	}

	const UParticleEmitter* Emitter = Emitters[Selection.EmitterIndex];
	const UParticleLODLevel* LODLevel = Emitter ? Emitter->GetLODLevel(Selection.LODIndex) : nullptr;
	const int32 ModuleCount = LODLevel ? static_cast<int32>(LODLevel->GetModules().size()) : 0;
	if (Selection.Kind == ESelectionKind::Module)
	{
		const bool bValidTypeDataSelection = Selection.ModuleIndex == TypeDataModuleIndex && LODLevel && LODLevel->GetTypeDataModule();
		const bool bValidModuleSelection = Selection.ModuleIndex >= 0 && Selection.ModuleIndex < ModuleCount;
		if (!bValidTypeDataSelection && !bValidModuleSelection)
		{
			SelectEmitter(Selection.EmitterIndex);
		}
	}
}

void FParticleSystemEditorWidget::SelectParticleSystem()
{
	ViewState.Selection = FEditorSelectionState{};
	ClearSelectedCurve();
}

void FParticleSystemEditorWidget::SelectEmitter(int32 EmitterIndex)
{
	const int32 CurrentLODIndex = GetCurrentSystemLODIndex(GetParticleSystem());
	ViewState.Selection.Kind = ESelectionKind::Emitter;
	ViewState.Selection.EmitterIndex = EmitterIndex;
	ViewState.Selection.LODIndex = CurrentLODIndex;
	ViewState.Selection.ModuleIndex = NoModuleIndex;
	ClearSelectedCurve();
}

void FParticleSystemEditorWidget::SelectLOD(int32 EmitterIndex, int32 LODIndex)
{
	ViewState.Selection.Kind = ESelectionKind::LOD;
	ViewState.Selection.EmitterIndex = EmitterIndex;
	ViewState.Selection.LODIndex = ClampSystemLODIndex(GetParticleSystem(), LODIndex);
	ViewState.Selection.ModuleIndex = NoModuleIndex;
	ClearSelectedCurve();
}

void FParticleSystemEditorWidget::SelectModule(int32 EmitterIndex, int32 LODIndex, int32 ModuleIndex)
{
	ViewState.Selection.Kind = ESelectionKind::Module;
	ViewState.Selection.EmitterIndex = EmitterIndex;
	ViewState.Selection.LODIndex = ClampSystemLODIndex(GetParticleSystem(), LODIndex);
	ViewState.Selection.ModuleIndex = ModuleIndex;
	ClearSelectedCurve();
}

bool FParticleSystemEditorWidget::SelectLODByIndex(int32 LODIndex)
{
	UParticleSystem* ParticleSystem = GetParticleSystem();
	if (!ParticleSystem)
	{
		return false;
	}

	if (ParticleSystem->GetLODCount() <= 0)
	{
		return false;
	}

	const int32 TargetLODIndex = ClampSystemLODIndex(ParticleSystem, LODIndex);
	const int32 EmitterIndex = ResolveSelectionEmitterIndex(ParticleSystem, ViewState.Selection.EmitterIndex);
	if (EmitterIndex >= 0)
	{
		SelectLOD(EmitterIndex, TargetLODIndex);
	}
	else
	{
		ViewState.Selection.Kind = ESelectionKind::ParticleSystem;
		ViewState.Selection.EmitterIndex = -1;
		ViewState.Selection.LODIndex = TargetLODIndex;
		ViewState.Selection.ModuleIndex = NoModuleIndex;
	}
	return true;
}

bool FParticleSystemEditorWidget::SelectAdjacentLOD(int32 Direction)
{
	UParticleSystem* ParticleSystem = GetParticleSystem();
	if (!ParticleSystem || ParticleSystem->GetLODCount() <= 0)
	{
		return false;
	}

	return SelectLODByIndex(ClampSystemLODIndex(ParticleSystem, ViewState.Selection.LODIndex) + Direction);
}

bool FParticleSystemEditorWidget::SelectExtremeLOD(bool bLowest)
{
	UParticleSystem* ParticleSystem = GetParticleSystem();
	if (!ParticleSystem || ParticleSystem->GetLODCount() <= 0)
	{
		return false;
	}

	const int32 LODCount = ParticleSystem->GetLODCount();
	return SelectLODByIndex(bLowest ? LODCount - 1 : 0);
}

bool FParticleSystemEditorWidget::AddLODToSystem(bool bInsertAfterCurrent)
{
	UParticleSystem* ParticleSystem = GetParticleSystem();
	if (!ParticleSystem)
	{
		return false;
	}

	ParticleSystem->NormalizeLODLevels();
	TArray<float>& LODDistances = ParticleSystem->GetMutableLODDistances();
	const int32 LODCount = ParticleSystem->GetLODCount();
	const int32 CurrentLODIndex = ClampSystemLODIndex(ParticleSystem, ViewState.Selection.LODIndex);
	const int32 RequestedInsertIndex = CurrentLODIndex + (bInsertAfterCurrent ? 1 : 0);
	const int32 InsertIndex = (std::max)(0, (std::min)(RequestedInsertIndex, LODCount));
	const float NewDistance = CalculateInsertedLODDistance(ParticleSystem, InsertIndex);

	LODDistances.insert(LODDistances.begin() + InsertIndex, NewDistance);

	TArray<UParticleEmitter*>& Emitters = ParticleSystem->GetMutableEmitters();
	for (UParticleEmitter* Emitter : Emitters)
	{
		if (!Emitter)
		{
			continue;
		}

		TArray<UParticleLODLevel*>& LODLevels = Emitter->GetMutableLODLevels();
		UParticleLODLevel* SourceLODLevel = LODLevels.empty() ? nullptr : Emitter->GetLODLevel((std::min)(CurrentLODIndex, static_cast<int32>(LODLevels.size()) - 1));
		UParticleLODLevel* NewLODLevel = DuplicateParticleLODLevel(SourceLODLevel, Emitter);
		if (!NewLODLevel)
		{
			continue;
		}

		NewLODLevel->SetLevel(InsertIndex);
		NewLODLevel->SetEnabled(true);
		NewLODLevel->SetAllModuleEditStates(InsertIndex == 0 ? EParticleModuleEditState::Duplicated : EParticleModuleEditState::InheritedLocked);
		const int32 EmitterInsertIndex = (std::max)(0, (std::min)(InsertIndex, static_cast<int32>(LODLevels.size())));
		LODLevels.insert(LODLevels.begin() + EmitterInsertIndex, NewLODLevel);
	}

	ParticleSystem->NormalizeLODLevels();

	const int32 EmitterIndex = ResolveSelectionEmitterIndex(ParticleSystem, ViewState.Selection.EmitterIndex);
	if (EmitterIndex >= 0)
	{
		SelectLOD(EmitterIndex, InsertIndex);
	}
	else
	{
		ViewState.Selection.Kind = ESelectionKind::ParticleSystem;
		ViewState.Selection.EmitterIndex = -1;
		ViewState.Selection.LODIndex = InsertIndex;
		ViewState.Selection.ModuleIndex = NoModuleIndex;
	}
	MarkDirty();
	RefreshParticleSystemComponents();
	return true;
}

bool FParticleSystemEditorWidget::RegenerateLowestLOD(bool bDuplicateHighest)
{
	UParticleSystem* ParticleSystem = GetParticleSystem();
	if (!ParticleSystem)
	{
		return false;
	}

	ParticleSystem->NormalizeLODLevels();
	TArray<float>& LODDistances = ParticleSystem->GetMutableLODDistances();
	if (ParticleSystem->GetLODCount() <= 1)
	{
		LODDistances.push_back(CalculateNextLODDistance(ParticleSystem));
		ParticleSystem->NormalizeLODLevels();
	}

	const int32 LODCount = ParticleSystem->GetLODCount();
	if (LODCount <= 0)
	{
		return false;
	}

	const int32 LowestLODIndex = LODCount - 1;
	const int32 SourceLODIndex = bDuplicateHighest ? 0 : (std::max)(0, LowestLODIndex - 1);
	TArray<UParticleEmitter*>& Emitters = ParticleSystem->GetMutableEmitters();
	for (UParticleEmitter* Emitter : Emitters)
	{
		if (!Emitter)
		{
			continue;
		}

		TArray<UParticleLODLevel*>& LODLevels = Emitter->GetMutableLODLevels();
		UParticleLODLevel* SourceLODLevel = Emitter->GetLODLevel(SourceLODIndex);
		UParticleLODLevel* NewLowestLOD = DuplicateParticleLODLevel(SourceLODLevel, Emitter);
		if (!NewLowestLOD)
		{
			continue;
		}

		NewLowestLOD->SetLevel(LowestLODIndex);
		NewLowestLOD->SetEnabled(true);
		NewLowestLOD->SetAllModuleEditStates(EParticleModuleEditState::InheritedLocked);
		if (LowestLODIndex < static_cast<int32>(LODLevels.size()))
		{
			UParticleLODLevel* RemovedLOD = LODLevels[LowestLODIndex];
			LODLevels[LowestLODIndex] = NewLowestLOD;
			UObjectManager::Get().DestroyObject(RemovedLOD);
		}
		else
		{
			LODLevels.push_back(NewLowestLOD);
		}
	}

	ParticleSystem->NormalizeLODLevels();

	const int32 EmitterIndex = ResolveSelectionEmitterIndex(ParticleSystem, ViewState.Selection.EmitterIndex);
	if (EmitterIndex >= 0)
	{
		SelectLOD(EmitterIndex, LowestLODIndex);
	}
	else
	{
		ViewState.Selection.Kind = ESelectionKind::ParticleSystem;
		ViewState.Selection.EmitterIndex = -1;
		ViewState.Selection.LODIndex = LowestLODIndex;
		ViewState.Selection.ModuleIndex = NoModuleIndex;
	}
	MarkDirty();
	RefreshParticleSystemComponents();
	return true;
}

bool FParticleSystemEditorWidget::DeleteSelectedLOD()
{
	UParticleSystem* ParticleSystem = GetParticleSystem();
	if (!ParticleSystem)
	{
		return false;
	}

	ParticleSystem->NormalizeLODLevels();
	const int32 LODCount = ParticleSystem->GetLODCount();
	if (LODCount <= 1)
	{
		return false;
	}

	const int32 LODIndex = ClampSystemLODIndex(ParticleSystem, ViewState.Selection.LODIndex);
	TArray<float>& LODDistances = ParticleSystem->GetMutableLODDistances();
	LODDistances.erase(LODDistances.begin() + LODIndex);

	TArray<UParticleEmitter*>& Emitters = ParticleSystem->GetMutableEmitters();
	for (UParticleEmitter* Emitter : Emitters)
	{
		if (!Emitter)
		{
			continue;
		}

		TArray<UParticleLODLevel*>& LODLevels = Emitter->GetMutableLODLevels();
		if (LODIndex >= 0 && LODIndex < static_cast<int32>(LODLevels.size()))
		{
			UParticleLODLevel* RemovedLOD = LODLevels[LODIndex];
			LODLevels.erase(LODLevels.begin() + LODIndex);
			UObjectManager::Get().DestroyObject(RemovedLOD);
		}
	}

	ParticleSystem->NormalizeLODLevels();
	const int32 NewLODIndex = ClampSystemLODIndex(ParticleSystem, LODIndex);
	const int32 EmitterIndex = ResolveSelectionEmitterIndex(ParticleSystem, ViewState.Selection.EmitterIndex);
	if (EmitterIndex >= 0)
	{
		SelectLOD(EmitterIndex, NewLODIndex);
	}
	else
	{
		ViewState.Selection.Kind = ESelectionKind::ParticleSystem;
		ViewState.Selection.EmitterIndex = -1;
		ViewState.Selection.LODIndex = NewLODIndex;
		ViewState.Selection.ModuleIndex = NoModuleIndex;
	}
	MarkDirty();
	RefreshParticleSystemComponents();
	return true;
}

bool FParticleSystemEditorWidget::IsEmitterSelected(int32 EmitterIndex) const
{
	const FEditorSelectionState& Selection = ViewState.Selection;
	return Selection.EmitterIndex == EmitterIndex &&
		(Selection.Kind == ESelectionKind::Emitter || Selection.Kind == ESelectionKind::LOD || Selection.Kind == ESelectionKind::Module);
}

bool FParticleSystemEditorWidget::IsLODSelected(int32 EmitterIndex, int32 LODIndex) const
{
	const FEditorSelectionState& Selection = ViewState.Selection;
	return Selection.EmitterIndex == EmitterIndex &&
		Selection.LODIndex == LODIndex &&
		(Selection.Kind == ESelectionKind::LOD || Selection.Kind == ESelectionKind::Module);
}

bool FParticleSystemEditorWidget::IsModuleSelected(int32 EmitterIndex, int32 LODIndex, int32 ModuleIndex) const
{
	const FEditorSelectionState& Selection = ViewState.Selection;
	return Selection.Kind == ESelectionKind::Module &&
		Selection.EmitterIndex == EmitterIndex &&
		Selection.LODIndex == LODIndex &&
		Selection.ModuleIndex == ModuleIndex;
}

const char* FParticleSystemEditorWidget::GetSelectionKindLabel() const
{
	switch (ViewState.Selection.Kind)
	{
	case ESelectionKind::ParticleSystem: return "Particle System";
	case ESelectionKind::Emitter: return "Emitter";
	case ESelectionKind::LOD: return "LOD";
	case ESelectionKind::Module: return "Module";
	default: return "Unknown";
	}
}

int32 FParticleSystemEditorWidget::GetCurrentSystemLODIndex(const UParticleSystem* ParticleSystem) const
{
	return ClampSystemLODIndex(ParticleSystem, ViewState.Selection.LODIndex);
}

const UParticleEmitter* FParticleSystemEditorWidget::GetSelectedEmitter(const UParticleSystem* ParticleSystem) const
{
	if (!ParticleSystem)
	{
		return nullptr;
	}

	const TArray<UParticleEmitter*>& Emitters = ParticleSystem->GetEmitters();
	const int32 EmitterIndex = ViewState.Selection.EmitterIndex;
	if (EmitterIndex < 0 || EmitterIndex >= static_cast<int32>(Emitters.size()))
	{
		return nullptr;
	}

	return Emitters[EmitterIndex];
}

const UParticleLODLevel* FParticleSystemEditorWidget::GetSelectedLODLevel(const UParticleSystem* ParticleSystem) const
{
	const UParticleEmitter* Emitter = GetSelectedEmitter(ParticleSystem);
	if (!Emitter)
	{
		return nullptr;
	}

	return Emitter->GetLODLevel(GetCurrentSystemLODIndex(ParticleSystem));
}

const UParticleModule* FParticleSystemEditorWidget::GetSelectedModule(const UParticleSystem* ParticleSystem) const
{
	if (ViewState.Selection.Kind != ESelectionKind::Module)
	{
		return nullptr;
	}

	const UParticleEmitter* Emitter = GetSelectedEmitter(ParticleSystem);
	if (!Emitter)
	{
		return nullptr;
	}

	const UParticleLODLevel* LODLevel = GetSelectedLODLevel(ParticleSystem);
	if (!LODLevel)
	{
		return nullptr;
	}

	if (ViewState.Selection.ModuleIndex == TypeDataModuleIndex)
	{
		return LODLevel->GetTypeDataModule();
	}

	const TArray<UParticleModule*>& Modules = LODLevel->GetModules();
	const int32 ModuleIndex = ViewState.Selection.ModuleIndex;
	if (ModuleIndex < 0 || ModuleIndex >= static_cast<int32>(Modules.size()))
	{
		return nullptr;
	}

	return LODLevel->ResolveModule(ModuleIndex, Emitter);
}

FParticleSystemEditorWidget::FEditorLayoutSizes FParticleSystemEditorWidget::CalculateLayoutSizes(const ImVec2& Available) const
{
	const ImGuiStyle& Style = ImGui::GetStyle();
	const float AvailableWidth = (std::max)(Available.x, 1.0f);
	const float AvailableHeight = (std::max)(Available.y, 1.0f);

	FEditorLayoutSizes Layout;

	const float HorizontalSplitTotal = (std::max)(1.0f, AvailableWidth - SplitterThickness);
	Layout.LeftWidth = CalculateSplitLeadingSize(HorizontalSplitTotal, ViewState.MainSplitRatio, MinColumnWidth, MinColumnWidth);
	Layout.RightWidth = (std::max)(1.0f, HorizontalSplitTotal - Layout.LeftWidth);

	const float VerticalSplitterGap = SplitterThickness + Style.ItemSpacing.y * 2.0f;
	const float LeftSplitTotalHeight = (std::max)(1.0f, AvailableHeight - VerticalSplitterGap);
	Layout.ViewportHeight = CalculateSplitLeadingSize(LeftSplitTotalHeight, ViewState.ViewportDetailsSplitRatio, MinViewportHeight, MinDetailsHeight);
	Layout.DetailsHeight = (std::max)(1.0f, LeftSplitTotalHeight - Layout.ViewportHeight);

	if (ViewState.bShowCurveEditor)
	{
		const float RightSplitTotalHeight = (std::max)(1.0f, AvailableHeight - VerticalSplitterGap);
		Layout.EmittersHeight = CalculateSplitLeadingSize(RightSplitTotalHeight, ViewState.EmittersCurveSplitRatio, MinEmittersHeight, MinCurveEditorHeight);
		Layout.CurveEditorHeight = (std::max)(1.0f, RightSplitTotalHeight - Layout.EmittersHeight);
	}
	else
	{
		Layout.EmittersHeight = AvailableHeight;
		Layout.CurveEditorHeight = 0.0f;
	}

	return Layout;
}

void FParticleSystemEditorWidget::RenderToolbar()
{
	ImGui::BeginChild("##ParticleEditorToolbar", ImVec2(0.0f, ToolbarHeight), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	ImGui::AlignTextToFramePadding();
	UParticleSystem* ParticleSystem = GetParticleSystem();

	ImGui::BeginDisabled(ParticleSystem == nullptr);
	if (DrawParticleToolbarIconButton("##ParticleSave", L"icon_file_save_40x.png", "", "Save"))
	{
		if (FParticleSystemManager::Get().Save(ParticleSystem))
		{
			ClearDirty();
		}
	}
	ImGui::EndDisabled();
	DrawToolbarGroupSeparator();

	if (DrawParticleToolbarIconButton("##ParticleRestartSim", L"icon_Cascade_RestartSim_40x.png", "Restart Sim", "Restart Sim"))
	{
		ViewState.bRestartPreviewRequested = true;
		ViewState.PreviewTime = 0.0f;
	}
	DrawToolbarGroupSeparator();

	const bool bCanAddLOD = ParticleSystem != nullptr;
	DrawParticleToolbarIconButton("##ParticleBounds", L"icon_Cascade_Bounds_40x.png", "Bounds", "Bounds");

	const int32 LODCount = ParticleSystem ? ParticleSystem->GetLODCount() : 0;
	const int32 CurrentLODIndex = GetCurrentSystemLODIndex(ParticleSystem);
	const bool bCanUseLODControls = bCanAddLOD && LODCount > 0;
	const bool bCanDeleteLOD = bCanUseLODControls && LODCount > 1;

	DrawToolbarGroupSeparator();
	ImGui::BeginDisabled(!bCanUseLODControls);
	if (DrawParticleToolbarIconButton("##ParticleLowestLOD", L"icon_Cascade_LowestLOD_40x.png", "Lowest LOD", "Select Lowest LOD"))
	{
		SelectExtremeLOD(/*bLowest=*/true);
	}
	ImGui::SameLine();
	if (DrawParticleToolbarIconButton("##ParticleLowerLOD", L"icon_Cascade_LowerLOD_40x.png", "Lower LOD", "Select Lower LOD"))
	{
		SelectAdjacentLOD(+1);
	}
	ImGui::EndDisabled();

	DrawToolbarGroupSeparator();
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted("LOD:");
	ImGui::SameLine();
	int32 EditableLODIndex = CurrentLODIndex;
	ImGui::SetNextItemWidth(38.0f);
	ImGui::BeginDisabled(!bCanUseLODControls);
	if (ImGui::InputInt("##ParticleCurrentLODIndex", &EditableLODIndex, 0, 0))
	{
		SelectLODByIndex(EditableLODIndex);
	}
	ImGui::EndDisabled();

	DrawToolbarGroupSeparator();
	ImGui::BeginDisabled(!bCanAddLOD);
	if (DrawParticleToolbarIconButton("##ParticleAddLODAfter", L"icon_Cascade_AddLOD1_40x.png", "Add LOD", "Add LOD After Current"))
	{
		AddLODToSystem(/*bInsertAfterCurrent=*/true);
	}
	ImGui::EndDisabled();
	ImGui::SameLine();

	ImGui::BeginDisabled(!bCanUseLODControls);
	if (DrawParticleToolbarIconButton("##ParticleHigherLOD", L"icon_Cascade_HigherLOD_40x.png", "Higher LOD", "Select Higher LOD"))
	{
		SelectAdjacentLOD(-1);
	}
	ImGui::SameLine();
	if (DrawParticleToolbarIconButton("##ParticleHighestLOD", L"icon_Cascade_HighestLOD_40x.png", "Highest LOD", "Select Highest LOD"))
	{
		SelectExtremeLOD(/*bLowest=*/false);
	}
	ImGui::EndDisabled();
	ImGui::SameLine();

	ImGui::BeginDisabled(!bCanDeleteLOD);
	if (DrawParticleToolbarIconButton("##ParticleDeleteLOD", L"icon_Cascade_DeleteLOD_40x.png", "Delete LOD", "Delete Selected LOD"))
	{
		DeleteSelectedLOD();
	}
	ImGui::EndDisabled();
	ImGui::EndChild();
}

void FParticleSystemEditorWidget::RenderViewportPanel(const ImVec2& Size)
{
	ImGui::BeginChild("##ParticleViewportPanel", Size, ImGuiChildFlags_Borders);
	DrawPanelHeader("Viewport");

	ImVec2 ViewportSize = ImGui::GetContentRegionAvail();
	if (ViewportSize.x <= 0.0f || ViewportSize.y <= 0.0f)
	{
		ImGui::EndChild();
		return;
	}

	ImVec2 ViewportPos = ImGui::GetCursorScreenPos();
	ViewportClient.SetViewportRect(ViewportPos.x, ViewportPos.y, ViewportSize.x, ViewportSize.y);

	FViewport* VP = ViewportClient.GetViewport();
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	if (VP)
	{
		VP->RequestResize(static_cast<uint32>((std::max)(ViewportSize.x, 1.0f)), static_cast<uint32>((std::max)(ViewportSize.y, 1.0f)));
		ViewportClient.NotifyViewportResized(static_cast<int32>(ViewportSize.x), static_cast<int32>(ViewportSize.y));

		if (VP->GetSRV())
		{
			ImGui::Image((ImTextureID)VP->GetSRV(), ViewportSize);
		}
		else
		{
			ImGui::Dummy(ViewportSize);
		}
		FSlateApplication::Get().SetViewportImGuiHovered(&ViewportClient, ImGui::IsItemHovered());
	}
	else
	{
		ImGui::InvisibleButton("##ParticlePreviewViewport", ViewportSize);
		const ImVec2 CanvasMax(ViewportPos.x + ViewportSize.x, ViewportPos.y + ViewportSize.y);
		DrawList->AddRectFilled(ViewportPos, CanvasMax, IM_COL32(75, 77, 77, 255));
	}

	auto DrawViewportOverlayButton = [&](const char* Label, const ImVec2& Min, const ImVec2& Size) -> bool
	{
		const ImVec2 Max(Min.x + Size.x, Min.y + Size.y);
		const bool bHovered = ImGui::IsMouseHoveringRect(Min, Max);
		const bool bActive = bHovered && ImGui::IsMouseDown(ImGuiMouseButton_Left);
		const bool bClicked = bHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
		const ImU32 FillColor = bActive
			? IM_COL32(46, 48, 54, 220)
			: (bHovered ? IM_COL32(38, 40, 46, 220) : IM_COL32(26, 27, 30, 190));
		DrawList->AddRectFilled(Min, Max, FillColor, 8.0f);
		DrawList->AddRect(Min, Max, IM_COL32(8, 9, 11, 180), 8.0f);
		const ImVec2 TextSize = ImGui::CalcTextSize(Label);
		DrawList->AddText(
			ImVec2(Min.x + (Size.x - TextSize.x) * 0.5f, Min.y + (Size.y - TextSize.y) * 0.5f),
			IM_COL32(230, 234, 240, 255),
			Label);
		return bClicked;
	};

	DrawViewportOverlayButton("View", ImVec2(ViewportPos.x + 8.0f, ViewportPos.y + 8.0f), ImVec2(42.0f, 22.0f));
	if (DrawViewportOverlayButton("Time", ImVec2(ViewportPos.x + 58.0f, ViewportPos.y + 8.0f), ImVec2(48.0f, 22.0f)))
	{
		ImGui::OpenPopup("##ParticleViewportTimePopup");
	}
	if (ImGui::BeginPopup("##ParticleViewportTimePopup"))
	{
		if (ImGui::MenuItem(ViewState.bPreviewPlaying ? "Pause" : "Play"))
		{
			ViewState.bPreviewPlaying = !ViewState.bPreviewPlaying;
		}
		ImGui::Separator();
		ImGui::SetNextItemWidth(170.0f);
		if (ImGui::SliderFloat("Anim Speed", &ViewState.PreviewAnimSpeed, 0.0f, 4.0f, "%.2fx"))
		{
			ViewState.PreviewAnimSpeed = (std::max)(0.0f, ViewState.PreviewAnimSpeed);
		}
		ImGui::EndPopup();
	}
	DrawViewportAxisGizmo(DrawList, ViewportPos, ViewportSize, ViewportClient);

	ImGui::EndChild();
}

void FParticleSystemEditorWidget::CreatePreviewWorld()
{
	if (!GEngine || !GetParticleSystem())
	{
		return;
	}

	FWorldContext& WorldContext = GEngine->CreateWorldContext(EWorldType::EditorPreview, PreviewWorldHandle);
	WorldContext.World->SetWorldType(EWorldType::EditorPreview);
	WorldContext.World->InitWorld();

	PreviewActor = WorldContext.World->SpawnActor<AActor>();
	PreviewParticleComponent = PreviewActor->AddComponent<UParticleSystemComponent>();
	PreviewParticleComponent->SetTemplate(GetParticleSystem());
	PreviewParticleComponent->SetPreviewLODIndex(GetCurrentSystemLODIndex(GetParticleSystem()));
	PreviewParticleComponent->SetPreviewSoloEmitterIndex(ViewState.SoloEmitterIndex);
	PreviewParticleComponent->ResetSystem();
	PreviewParticleComponent->Activate();
	PreviewActor->SetRootComponent(PreviewParticleComponent);
	PreviewActor->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));

	ADirectionalLightActor* LightActor = WorldContext.World->SpawnActor<ADirectionalLightActor>();
	LightActor->InitDefaultComponents();
	LightActor->SetActorRotation(FVector(0.0f, 45.0f, -45.0f));
	if (UDirectionalLightComponent* LightComp = LightActor->GetComponentByClass<UDirectionalLightComponent>())
	{
		LightComp->SetShadowBias(0.0f);
		LightComp->PushToScene();
	}

	ViewportClient.Initialize(GEngine->GetRenderer().GetFD3DDevice().GetDevice(), 512, 512);
	ViewportClient.SetPreviewWorld(WorldContext.World);
	ViewportClient.SetPreviewActor(PreviewActor);
	ViewportClient.SetPreviewParticleComponent(PreviewParticleComponent);
	ViewportClient.ResetCameraToPreviewBounds();

	WorldContext.World->SetEditorPOVProvider(&ViewportClient);
	WorldContext.World->BeginPlay();

	FSlateApplication::Get().RegisterViewport(&ViewportClient);
}

void FParticleSystemEditorWidget::DestroyPreviewWorld()
{
	UWorld* PreviewWorld = ViewportClient.GetPreviewWorld();
	if (PreviewWorld)
	{
		PreviewWorld->SetEditorPOVProvider(nullptr);
	}

	FSlateApplication::Get().UnregisterViewport(&ViewportClient);
	ViewportClient.Release();
	PreviewActor = nullptr;
	PreviewParticleComponent = nullptr;

	if (PreviewWorld)
	{
		FScene& PreviewScene = PreviewWorld->GetScene();
		if (GEngine)
		{
			GEngine->GetRenderer().GetResources().ReleaseShadowResourcesForScene(&PreviewScene);
		}

		if (GEngine && PreviewWorldHandle.IsValid())
		{
			GEngine->DestroyWorldContext(PreviewWorldHandle);
		}
	}
}

void FParticleSystemEditorWidget::RestartPreviewSimulation()
{
	ViewState.PreviewTime = 0.0f;

	if (!PreviewParticleComponent)
	{
		return;
	}

	PreviewParticleComponent->SetPreviewSoloEmitterIndex(ViewState.SoloEmitterIndex);
	PreviewParticleComponent->SetPreviewLODIndex(GetCurrentSystemLODIndex(GetParticleSystem()));
	PreviewParticleComponent->ResetSystem();
	PreviewParticleComponent->Activate();
	PreviewParticleComponent->MarkRenderStateDirty();
}

void FParticleSystemEditorWidget::RefreshParticleSystemComponents()
{
	ViewState.PreviewTime = 0.0f;

	UParticleSystem* ParticleSystem = GetParticleSystem();
	if (!ParticleSystem)
	{
		return;
	}

	for (TObjectIterator<UParticleSystemComponent> It; It; ++It)
	{
		UParticleSystemComponent* Component = *It;
		if (!Component || Component->GetTemplate() != ParticleSystem)
		{
			continue;
		}

		const bool bWasActive = Component->IsActive();
		if (Component == PreviewParticleComponent)
		{
			Component->SetPreviewSoloEmitterIndex(ViewState.SoloEmitterIndex);
			Component->SetPreviewLODIndex(GetCurrentSystemLODIndex(ParticleSystem));
		}
		Component->ResetSystem();
		if (bWasActive)
		{
			Component->Activate();
		}
		else
		{
			Component->Deactivate();
		}
		Component->MarkRenderStateDirty();
	}
}

void FParticleSystemEditorWidget::RenderDetailsPanel(const ImVec2& Size)
{
	ImGui::BeginChild("##ParticleDetailsPanel", Size, ImGuiChildFlags_Borders);
	if (PendingDetailsScrollRestoreFrames > 0 && PendingDetailsScrollY >= 0.0f)
	{
		ImGui::SetScrollY(PendingDetailsScrollY);
	}
	const float DetailsScrollYBeforeRender = ImGui::GetScrollY();
	if (LastStableDetailsScrollY < 0.0f)
	{
		LastStableDetailsScrollY = DetailsScrollYBeforeRender;
	}
	bool bDetailsValueChanged = false;

	DrawPanelHeader("Details");

	UParticleSystem* ParticleSystem = GetParticleSystem();
	if (!ParticleSystem)
	{
		ImGui::TextDisabled("No particle system selected.");
		ImGui::EndChild();
		return;
	}

	auto CommitLODDistanceChange = [&](int32 LODIndex, float NewDistance)
	{
		TArray<float>& LODDistances = ParticleSystem->GetMutableLODDistances();
		if (LODIndex < 0 || LODIndex >= static_cast<int32>(LODDistances.size()))
		{
			return;
		}

		LODDistances[LODIndex] = LODIndex == 0 ? 0.0f : (std::max)(0.0f, NewDistance);
		bDetailsValueChanged = true;
		MarkDirty();
		RefreshParticleSystemComponents();
	};

	auto RenderLODDistancesEditor = [&]()
	{
		if (!DrawDetailsCategoryHeader("LODDistances"))
		{
			return;
		}

		if (BeginDetailsTable("##ParticleSystemLODDistancesTable"))
		{
			TArray<float>& LODDistances = ParticleSystem->GetMutableLODDistances();
			for (int32 LODIndex = 0; LODIndex < static_cast<int32>(LODDistances.size()); ++LODIndex)
			{
				char Label[32];
				std::snprintf(Label, sizeof(Label), "Index [%d]", LODIndex);
				float EditableDistance = LODDistances[LODIndex];
				ImGui::PushID(LODIndex);
				if (DrawDetailFloatInputRow(Label, EditableDistance, "%.1f"))
				{
					CommitLODDistanceChange(LODIndex, EditableDistance);
				}
				ImGui::PopID();
			}
			EndDetailsTable();
		}
	};

	auto RenderParticleSystemDetails = [&]()
	{
		if (!DrawDetailsCategoryHeader("Particle System"))
		{
			return;
		}

		if (BeginDetailsTable("##ParticleSystemSummaryTable"))
		{
			DrawDetailRow("Asset", ParticleSystem->GetAssetPathFileName().c_str());
			DrawDetailRowF("Emitters", "%d", static_cast<int32>(ParticleSystem->GetEmitters().size()));
			DrawDetailRowF("LOD Count", "%d", ParticleSystem->GetLODCount());
			EndDetailsTable();
		}
		RenderLODDistancesEditor();
	};

	auto RenderEmitterDetails = [&]()
	{
		UParticleEmitter* SelectedEmitter = nullptr;
		TArray<UParticleEmitter*>& MutableEmitters = ParticleSystem->GetMutableEmitters();
		if (ViewState.Selection.EmitterIndex >= 0 && ViewState.Selection.EmitterIndex < static_cast<int32>(MutableEmitters.size()))
		{
			SelectedEmitter = MutableEmitters[ViewState.Selection.EmitterIndex];
		}
		if (!SelectedEmitter || !DrawDetailsCategoryHeader("Emitter"))
		{
			return;
		}

		if (BeginDetailsTable("##ParticleEmitterSummaryTable"))
		{
			int32 EditableMaxActiveParticles = SelectedEmitter->GetMaxActiveParticles();
			if (DrawDetailIntInputRow("Max Active Particles", EditableMaxActiveParticles))
			{
				EditableMaxActiveParticles = (std::max)(0, (std::min)(EditableMaxActiveParticles, 65535));
				if (EditableMaxActiveParticles != SelectedEmitter->GetMaxActiveParticles())
				{
					SelectedEmitter->SetMaxActiveParticles(EditableMaxActiveParticles);
					bDetailsValueChanged = true;
					MarkDirty();
					RefreshParticleSystemComponents();
				}
			}
			DrawDetailRowF("Emitter Duration", "%.3f", SelectedEmitter->GetEmitterDuration());
			DrawDetailRow("Looping", SelectedEmitter->IsLooping() ? "true" : "false");
			DrawDetailRowF("Emitter LOD Levels", "%d", static_cast<int32>(SelectedEmitter->GetLODLevels().size()));
			EndDetailsTable();
		}
	};

	auto RenderLODDetails = [&]()
	{
		const UParticleLODLevel* SelectedLOD = GetSelectedLODLevel(ParticleSystem);
		if (!SelectedLOD || !DrawDetailsCategoryHeader("LOD"))
		{
			return;
		}

		if (BeginDetailsTable("##ParticleLODSummaryTable"))
		{
			const UParticleModuleTypeDataBase* TypeDataModule = SelectedLOD->GetTypeDataModule();
			float EditableDistance = ParticleSystem->GetLODDistance(SelectedLOD->GetLevel());
			DrawDetailRowF("Level", "%d", SelectedLOD->GetLevel());
			ImGui::PushID("LODDistance");
			if (DrawDetailFloatInputRow("Distance", EditableDistance, "%.1f"))
			{
				CommitLODDistanceChange(SelectedLOD->GetLevel(), EditableDistance);
			}
			ImGui::PopID();
			DrawDetailRow("Enabled", SelectedLOD->IsEnabled() ? "true" : "false");
			DrawDetailRow("Render Type", GetRenderTypeLabel(GetLODRenderType(SelectedLOD)));
			DrawDetailRow("Type Data", GetTypeDataDisplayName(TypeDataModule));
			DrawDetailRowF("Payload Size", "%d", TypeDataModule ? TypeDataModule->GetParticlePayloadSize() : 0);
			DrawDetailRowF("Module Count", "%d", static_cast<int32>(SelectedLOD->GetModules().size()));
			EndDetailsTable();
		}
	};

	auto RenderModuleDetails = [&]()
	{
		const UParticleLODLevel* SelectedLOD = GetSelectedLODLevel(ParticleSystem);
		UParticleModule* SelectedModule = const_cast<UParticleModule*>(GetSelectedModule(ParticleSystem));
		if (!SelectedModule || !DrawDetailsCategoryHeader("Module"))
		{
			return;
		}

		const int32 CurrentLODIndex = GetCurrentSystemLODIndex(ParticleSystem);
		const bool bTypeDataSelection = ViewState.Selection.ModuleIndex == TypeDataModuleIndex;
		const EParticleModuleEditState ModuleEditState = (!bTypeDataSelection && SelectedLOD)
			? SelectedLOD->GetModuleEditState(ViewState.Selection.ModuleIndex)
			: (CurrentLODIndex == 0 ? EParticleModuleEditState::Duplicated : EParticleModuleEditState::InheritedLocked);
		const bool bModuleReadOnly = !CanDirectEditModulesInLOD(CurrentLODIndex)
			&& (bTypeDataSelection
				|| ModuleEditState == EParticleModuleEditState::InheritedLocked
				|| ModuleEditState == EParticleModuleEditState::Shared);
		if (BeginDetailsTable("##ParticleModuleSummaryTable"))
		{
			const UParticleModuleTypeDataBase* TypeDataModule = Cast<UParticleModuleTypeDataBase>(SelectedModule);
			DrawDetailRow("Name", TypeDataModule ? GetTypeDataDisplayName(TypeDataModule) : GetModuleDisplayName(SelectedModule));
			DrawDetailRow("Class", SelectedModule->GetClass()->GetName());
			DrawDetailRow("Spawn Module", SelectedModule->IsSpawnModule() ? "true" : "false");
			DrawDetailRow("Update Module", SelectedModule->IsUpdateModule() ? "true" : "false");
			DrawDetailRow("Source", ViewState.Selection.ModuleIndex == TypeDataModuleIndex ? "LOD TypeDataModule" : "LOD Modules[]");
			DrawDetailRow("Edit State", bTypeDataSelection ? "TypeData" : GetModuleEditStateLabel(ModuleEditState));
			DrawDetailRow("Editable", bModuleReadOnly ? "false" : "true");
			EndDetailsTable();
		}
		ImGui::Separator();
		if (RenderObjectProperties(SelectedModule, bModuleReadOnly))
		{
			bDetailsValueChanged = true;
			ApplyEditedObjectSideEffects(SelectedModule);
			MarkDirty();
			RefreshParticleSystemComponents();
		}
	};

	switch (ViewState.Selection.Kind)
	{
	case ESelectionKind::Module:
		RenderModuleDetails();
		break;
	case ESelectionKind::Emitter:
		RenderEmitterDetails();
		break;
	case ESelectionKind::LOD:
		RenderLODDetails();
		break;
	case ESelectionKind::ParticleSystem:
	default:
		RenderParticleSystemDetails();
		break;
	}

	if (bDetailsValueChanged)
	{
		PendingDetailsScrollY = LastStableDetailsScrollY >= 0.0f ? LastStableDetailsScrollY : DetailsScrollYBeforeRender;
		PendingDetailsScrollRestoreFrames = 3;
	}
	if (PendingDetailsScrollY >= 0.0f && PendingDetailsScrollRestoreFrames > 0)
	{
		ImGui::SetScrollY(PendingDetailsScrollY);
		--PendingDetailsScrollRestoreFrames;
		if (PendingDetailsScrollRestoreFrames <= 0)
		{
			LastStableDetailsScrollY = PendingDetailsScrollY;
			PendingDetailsScrollY = -1.0f;
		}
	}
	else
	{
		LastStableDetailsScrollY = ImGui::GetScrollY();
	}

	ImGui::EndChild();
}

bool FParticleSystemEditorWidget::RenderObjectProperties(UObject* Object, bool bReadOnly)
{
	if (!Object)
	{
		ImGui::TextDisabled("No object properties.");
		return false;
	}

	TArray<FPropertyValue> Properties;
	Object->GetEditableProperties(Properties);
	if (Properties.empty())
	{
		ImGui::TextDisabled("No editable properties.");
		return false;
	}

	bool bAnyChanged = false;
	TArray<FString> CategoryOrder;
	for (const FPropertyValue& PropertyValue : Properties)
	{
		const FString Category = PropertyValue.GetCategory() ? PropertyValue.GetCategory() : "";
		bool bFound = false;
		for (const FString& ExistingCategory : CategoryOrder)
		{
			if (ExistingCategory == Category)
			{
				bFound = true;
				break;
			}
		}
		if (!bFound)
		{
			CategoryOrder.push_back(Category);
		}
	}

	for (const FString& Category : CategoryOrder)
	{
		if (!Category.empty() && !DrawDetailsCategoryHeader(Category.c_str()))
		{
			continue;
		}

		ImGui::PushID(Category.c_str());
		const bool bTableOpen = BeginDetailsTable("##ParticleObjectProperties");
		if (!bTableOpen)
		{
			ImGui::PopID();
			continue;
		}
		for (int32 Index = 0; Index < static_cast<int32>(Properties.size()); ++Index)
		{
			FPropertyValue& PropertyValue = Properties[Index];
			const FString PropertyCategory = PropertyValue.GetCategory() ? PropertyValue.GetCategory() : "";
			if (PropertyCategory != Category)
			{
				continue;
			}

			ImGui::PushID(Index);
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			const bool bPropertyOpen = FEditorPropertyRenderer::DrawPropertyLabel(PropertyValue);

			ImGui::TableSetColumnIndex(1);
			const bool bCurveDistributionProperty = IsRawDistributionFloatProperty(PropertyValue) || IsRawDistributionVectorProperty(PropertyValue);
			if (bCurveDistributionProperty)
			{
				const float CurveButtonWidth = 18.0f;
				const float Width = (std::max)(1.0f, ImGui::GetContentRegionAvail().x - CurveButtonWidth - ImGui::GetStyle().ItemSpacing.x);
				ImGui::SetNextItemWidth(Width);
			}
			else
			{
				ImGui::SetNextItemWidth(-1.0f);
			}

			FEditorPropertyRenderOptions Options;
			Options.bDispatchChange = false;
			Options.bUseExternalExpansion = true;
			Options.bParentExpanded = bPropertyOpen;
			if (bReadOnly)
			{
				ImGui::BeginDisabled();
			}
			const bool bChanged = PropertyRenderer.RenderPropertyWidget(Properties, Index, Options);
			if (bCurveDistributionProperty)
			{
				ImGui::SameLine();
				if (DrawCurveIconButton("##DistributionCurve", ImVec2(18.0f, 18.0f), bReadOnly))
				{
					const FString PropertyLabel = FEditorPropertyRenderer::GetPropertyDisplayName(PropertyValue);
					if (IsRawDistributionFloatProperty(PropertyValue))
					{
						if (FRawDistributionFloat* Distribution = static_cast<FRawDistributionFloat*>(PropertyValue.GetValuePtr()))
						{
							SelectDistributionCurve(Object, Distribution, PropertyLabel, 0);
						}
					}
					else if (IsRawDistributionVectorProperty(PropertyValue))
					{
						if (FRawDistributionVector* Distribution = static_cast<FRawDistributionVector*>(PropertyValue.GetValuePtr()))
						{
							SelectDistributionCurve(Object, Distribution, PropertyLabel, -1);
						}
					}
				}
			}
			if (bReadOnly)
			{
				ImGui::EndDisabled();
			}
			if (bChanged)
			{
				bAnyChanged = true;
				if (PropertyValue.Property)
				{
					Object->PostEditProperty(PropertyValue.Property->Name);
				}
			}
			ImGui::PopID();
		}
		EndDetailsTable();
		ImGui::PopID();
	}

	return bAnyChanged;
}

void FParticleSystemEditorWidget::ApplyEditedObjectSideEffects(UObject* Object)
{
	if (!Object)
	{
		return;
	}

	if (UParticleModuleRequired* RequiredModule = Cast<UParticleModuleRequired>(Object))
	{
		RequiredModule->Material = nullptr;
	}

	if (UParticleModuleTypeDataMesh* MeshTypeData = Cast<UParticleModuleTypeDataMesh>(Object))
	{
		MeshTypeData->Mesh = nullptr;
	}
}

void FParticleSystemEditorWidget::RenderEmittersPanel(const ImVec2& Size)
{
	ImGui::BeginChild("##ParticleEmittersPanel", Size, ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);

	UParticleSystem* ParticleSystem = GetParticleSystem();
	const ImGuiStyle& Style = ImGui::GetStyle();
	const TArray<UParticleEmitter*>* EmittersPtr = ParticleSystem ? &ParticleSystem->GetEmitters() : nullptr;
	const int32 EmitterCount = EmittersPtr ? static_cast<int32>(EmittersPtr->size()) : 0;
	const float EmitterColumnsWidth = EmitterCount > 0
		? EmitterColumnWidth * static_cast<float>(EmitterCount) + Style.ItemSpacing.x * static_cast<float>((std::max)(0, EmitterCount - 1))
		: 0.0f;
	DrawPanelHeader("Emitters", EmitterColumnsWidth);

	if (!ParticleSystem)
	{
		ImGui::TextDisabled("No particle system selected.");
		ImGui::EndChild();
		return;
	}

	const TArray<UParticleEmitter*>& Emitters = ParticleSystem->GetEmitters();
	if (Emitters.empty())
	{
		ImGui::TextDisabled("No emitters.");
		ImGui::EndChild();
		return;
	}

	auto CalculateEmitterColumnHeight = [&](UParticleEmitter* Emitter)
	{
		float Height = EmitterHeaderHeight + Style.ItemSpacing.y;
		if (!Emitter)
		{
			return Height + ImGui::GetTextLineHeightWithSpacing();
		}

		UParticleLODLevel* LODLevel = Emitter->GetLODLevel(GetCurrentSystemLODIndex(ParticleSystem));
		if (LODLevel)
		{
			Height += ModuleRowHeight + Style.ItemSpacing.y;
			Height += static_cast<float>(LODLevel->GetModules().size()) * ModuleRowHeight;
			Height += ModuleDropTargetHeight;
		}

		return Height + Style.ItemSpacing.y;
	};

	float EmitterColumnHeight = 1.0f;
	for (UParticleEmitter* Emitter : Emitters)
	{
		EmitterColumnHeight = (std::max)(EmitterColumnHeight, CalculateEmitterColumnHeight(Emitter));
	}

	bool bSelectParticleSystemRequested = false;
	int32 DefaultEmitterInsertIndex = -1;
	int32 DuplicateEmitterIndex = -1;
	int32 DeleteEmitterIndex = -1;

	for (int32 EmitterIndex = 0; EmitterIndex < static_cast<int32>(Emitters.size()); ++EmitterIndex)
	{
		UParticleEmitter* Emitter = Emitters[EmitterIndex];
		const int32 DisplayLODIndex = GetCurrentSystemLODIndex(ParticleSystem);
		UParticleLODLevel* LODLevel = Emitter ? Emitter->GetLODLevel(DisplayLODIndex) : nullptr;
		if (LODLevel)
		{
			LODLevel->NormalizeModuleEditStates(DisplayLODIndex == 0 ? EParticleModuleEditState::Duplicated : EParticleModuleEditState::InheritedLocked);
		}
		const EParticleRenderType RenderType = GetLODRenderType(LODLevel);
		const bool bCanDirectEditModules = CanDirectEditModulesInLOD(DisplayLODIndex);
		const bool bModuleDirectEditLocked = !bCanDirectEditModules;
		ImGui::PushID(EmitterIndex);
		ImGui::BeginChild("##EmitterColumn", ImVec2(EmitterColumnWidth, EmitterColumnHeight), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

		auto AddModule = [&](UParticleModule* NewModule)
		{
			if (!bCanDirectEditModules || !LODLevel || !NewModule)
			{
				return;
			}

			TArray<UParticleModule*>& MutableModules = LODLevel->GetMutableModules();
			MutableModules.push_back(NewModule);
			LODLevel->GetMutableModuleEditStates().push_back(EParticleModuleEditState::Duplicated);
			LODLevel->NormalizeModuleEditStates(EParticleModuleEditState::Duplicated);
			SelectModule(EmitterIndex, DisplayLODIndex, static_cast<int32>(MutableModules.size()) - 1);
			MarkDirty();
			RefreshParticleSystemComponents();
		};

		auto SetTypeDataModule = [&](UParticleModuleTypeDataBase* NewTypeDataModule)
		{
			if (!bCanDirectEditModules || !LODLevel)
			{
				return;
			}

			if (UParticleModuleTypeDataBase* ExistingTypeDataModule = LODLevel->GetTypeDataModule())
			{
				UObjectManager::Get().DestroyObject(ExistingTypeDataModule);
			}

			LODLevel->SetTypeDataModule(NewTypeDataModule);
			if (NewTypeDataModule)
			{
				SelectModule(EmitterIndex, DisplayLODIndex, TypeDataModuleIndex);
			}
			else
			{
				SelectLOD(EmitterIndex, DisplayLODIndex);
			}
			MarkDirty();
			RefreshParticleSystemComponents();
		};

		auto RenderUnavailableCategory = [](const char* Label)
		{
			if (ImGui::BeginMenu(Label))
			{
				ImGui::BeginDisabled();
				ImGui::MenuItem("No modules available");
				ImGui::EndDisabled();
				ImGui::EndMenu();
			}
		};

		auto RenderAddModuleMenu = [&]()
		{
			if (ImGui::BeginMenu("Emitter"))
			{
				ImGui::TextDisabled("EMITTER");
				ImGui::Separator();

				ImGui::BeginDisabled();
				ImGui::MenuItem("Rename Emitter");
				ImGui::EndDisabled();

				if (ImGui::MenuItem("Duplicate Emitter"))
				{
					DuplicateEmitterIndex = EmitterIndex;
				}

				ImGui::BeginDisabled();
				ImGui::MenuItem("Duplicate and Share Emitter");
				ImGui::EndDisabled();

				if (ImGui::MenuItem("Delete Emitter"))
				{
					DeleteEmitterIndex = EmitterIndex;
				}

				ImGui::BeginDisabled();
				ImGui::MenuItem("Export Emitter");
				ImGui::MenuItem("Export All");
				ImGui::EndDisabled();

				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Particle System"))
			{
				ImGui::TextDisabled("PARTICLE SYSTEM");
				ImGui::Separator();

				if (ImGui::MenuItem("Select Particle System"))
				{
					bSelectParticleSystemRequested = true;
				}
				if (ImGui::MenuItem("Add New Emitter Before"))
				{
					DefaultEmitterInsertIndex = EmitterIndex;
				}
				if (ImGui::MenuItem("Add New Emitter After"))
				{
					DefaultEmitterInsertIndex = EmitterIndex + 1;
				}

				ImGui::BeginDisabled();
				ImGui::MenuItem("Remove Duplicate Modules");
				ImGui::EndDisabled();

				ImGui::EndMenu();
			}

			ImGui::BeginDisabled(!bCanDirectEditModules);
			if (ImGui::BeginMenu("TypeData"))
			{
				ImGui::TextDisabled("TYPEDATA");
				ImGui::Separator();

				if (ImGui::MenuItem("Sprite"))
				{
					SetTypeDataModule(nullptr);
				}
				if (ImGui::MenuItem("Mesh"))
				{
					SetTypeDataModule(UObjectManager::Get().CreateObject<UParticleModuleTypeDataMesh>());
				}
				if (ImGui::MenuItem("Ribbon"))
				{
					SetTypeDataModule(UObjectManager::Get().CreateObject<UParticleModuleTypeDataRibbon>());
				}
				if (ImGui::MenuItem("Beam"))
				{
					SetTypeDataModule(UObjectManager::Get().CreateObject<UParticleModuleTypeDataBeam>());
				}
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Acceleration"))
			{
				if (ImGui::MenuItem("Acceleration"))
				{
					AddModule(UObjectManager::Get().CreateObject<UParticleModuleAcceleration>());
				}
				ImGui::EndMenu();
			}
			RenderUnavailableCategory("Attraction");

			if (ImGui::BeginMenu("Beam"))
			{
				if (ImGui::MenuItem("Beam Source"))
				{
					AddModule(UObjectManager::Get().CreateObject<UParticleModuleBeamSource>());
				}
				if (ImGui::MenuItem("Beam Noise"))
				{
					AddModule(UObjectManager::Get().CreateObject<UParticleModuleBeamNoise>());
				}
				if (ImGui::MenuItem("Beam Target"))
				{
					AddModule(UObjectManager::Get().CreateObject<UParticleModuleBeamTarget>());
				}
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Collision"))
			{
				if (ImGui::MenuItem("Collision"))
				{
					AddModule(UObjectManager::Get().CreateObject<UParticleModuleCollision>());
				}
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Color"))
			{
				if (ImGui::MenuItem("Color Over Life"))
				{
					AddModule(UObjectManager::Get().CreateObject<UParticleModuleColor>());
				}
				ImGui::EndMenu();
			}

			RenderUnavailableCategory("Event");

			if (ImGui::BeginMenu("Lifetime"))
			{
				if (ImGui::MenuItem("Lifetime"))
				{
					AddModule(UObjectManager::Get().CreateObject<UParticleModuleLifetime>());
				}
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Location"))
			{
				if (ImGui::MenuItem("Initial Location"))
				{
					AddModule(UObjectManager::Get().CreateObject<UParticleModuleLocation>());
				}
				ImGui::EndMenu();
			}

			RenderUnavailableCategory("Rotation");
			RenderUnavailableCategory("Rotation Rate");
			RenderUnavailableCategory("Orbit");
			RenderUnavailableCategory("Orientation");

			if (ImGui::BeginMenu("Size"))
			{
				if (ImGui::MenuItem("Initial Size"))
				{
					AddModule(UObjectManager::Get().CreateObject<UParticleModuleSize>());
				}
				ImGui::EndMenu();
			}

			RenderUnavailableCategory("Spawn");

			if (ImGui::BeginMenu("SubUV"))
			{
				if (ImGui::MenuItem("Sub Image Index"))
				{
					AddModule(UObjectManager::Get().CreateObject<UParticleModuleSubImageIndex>());
				}
				ImGui::EndMenu();
			}

			RenderUnavailableCategory("Vector Field");

			if (ImGui::BeginMenu("Velocity"))
			{
				if (ImGui::MenuItem("Initial Velocity"))
				{
					AddModule(UObjectManager::Get().CreateObject<UParticleModuleVelocity>());
				}
				ImGui::EndMenu();
			}
			ImGui::EndDisabled();
		};

		auto MoveDroppedModule = [&](const FParticleModuleDropRequest& DropRequest)
		{
			if (!DropRequest.bRequested)
			{
				return false;
			}

			const FParticleModuleDragPayload& Payload = DropRequest.Payload;
			if (!Payload.Module || IsModuleOrderLocked(Payload.Module))
			{
				return false;
			}

			if (Payload.EmitterIndex < 0 || Payload.EmitterIndex >= static_cast<int32>(Emitters.size()) ||
				DropRequest.TargetEmitterIndex < 0 || DropRequest.TargetEmitterIndex >= static_cast<int32>(Emitters.size()))
			{
				return false;
			}

			UParticleEmitter* SourceEmitter = Emitters[Payload.EmitterIndex];
			UParticleEmitter* TargetEmitter = Emitters[DropRequest.TargetEmitterIndex];
			if (!SourceEmitter || !TargetEmitter)
			{
				return false;
			}

			UParticleLODLevel* SourceLODLevel = SourceEmitter->GetLODLevel(Payload.LODIndex);
			UParticleLODLevel* TargetLODLevel = TargetEmitter->GetLODLevel(DropRequest.TargetLODIndex);
			if (!SourceLODLevel || !TargetLODLevel)
			{
				return false;
			}

			TArray<UParticleModule*>& SourceModules = SourceLODLevel->GetMutableModules();
			TArray<UParticleModule*>& TargetModules = TargetLODLevel->GetMutableModules();
			SourceLODLevel->NormalizeModuleEditStates(Payload.LODIndex == 0 ? EParticleModuleEditState::Duplicated : EParticleModuleEditState::InheritedLocked);
			TargetLODLevel->NormalizeModuleEditStates(DropRequest.TargetLODIndex == 0 ? EParticleModuleEditState::Duplicated : EParticleModuleEditState::InheritedLocked);
			TArray<EParticleModuleEditState>& SourceEditStates = SourceLODLevel->GetMutableModuleEditStates();
			TArray<EParticleModuleEditState>& TargetEditStates = TargetLODLevel->GetMutableModuleEditStates();
			auto SourceIt = SourceModules.end();
			if (Payload.ModuleIndex >= 0 && Payload.ModuleIndex < static_cast<int32>(SourceModules.size()) &&
				SourceModules[Payload.ModuleIndex] == Payload.Module)
			{
				SourceIt = SourceModules.begin() + Payload.ModuleIndex;
			}
			else
			{
				SourceIt = std::find(SourceModules.begin(), SourceModules.end(), Payload.Module);
			}
			if (SourceIt == SourceModules.end() || IsModuleOrderLocked(*SourceIt))
			{
				return false;
			}

			const bool bSameLOD = SourceLODLevel == TargetLODLevel;
			const int32 SourceModuleIndex = static_cast<int32>(SourceIt - SourceModules.begin());
			int32 TargetInsertIndex = ClampModuleInsertIndex(TargetModules, DropRequest.TargetInsertIndex);
			if (bSameLOD && (TargetInsertIndex == SourceModuleIndex || TargetInsertIndex == SourceModuleIndex + 1))
			{
				return false;
			}

			UParticleModule* MovedModule = *SourceIt;
			const EParticleModuleEditState MovedEditState = SourceModuleIndex >= 0 && SourceModuleIndex < static_cast<int32>(SourceEditStates.size())
				? SourceEditStates[SourceModuleIndex]
				: EParticleModuleEditState::Duplicated;
			SourceModules.erase(SourceIt);
			if (SourceModuleIndex >= 0 && SourceModuleIndex < static_cast<int32>(SourceEditStates.size()))
			{
				SourceEditStates.erase(SourceEditStates.begin() + SourceModuleIndex);
			}
			if (bSameLOD && SourceModuleIndex < TargetInsertIndex)
			{
				--TargetInsertIndex;
			}
			TargetInsertIndex = ClampModuleInsertIndex(TargetModules, TargetInsertIndex);
			TargetModules.insert(TargetModules.begin() + TargetInsertIndex, MovedModule);
			TargetEditStates.insert(TargetEditStates.begin() + TargetInsertIndex, MovedEditState);
			SourceLODLevel->NormalizeModuleEditStates(Payload.LODIndex == 0 ? EParticleModuleEditState::Duplicated : EParticleModuleEditState::InheritedLocked);
			TargetLODLevel->NormalizeModuleEditStates(DropRequest.TargetLODIndex == 0 ? EParticleModuleEditState::Duplicated : EParticleModuleEditState::InheritedLocked);

			SelectModule(DropRequest.TargetEmitterIndex, DropRequest.TargetLODIndex, TargetInsertIndex);
			MarkDirty();
			RefreshParticleSystemComponents();
			return true;
		};

		auto GetModuleFromLOD = [&](int32 SourceLODIndex, int32 ModuleIndex) -> UParticleModule*
		{
			if (!Emitter || SourceLODIndex < 0)
			{
				return nullptr;
			}

			UParticleLODLevel* SourceLODLevel = Emitter->GetLODLevel(SourceLODIndex);
			if (!SourceLODLevel)
			{
				return nullptr;
			}

			return SourceLODLevel->ResolveModule(ModuleIndex, Emitter);
		};

		auto ReplaceModuleFromSource = [&](int32 ModuleIndex, int32 SourceLODIndex, EParticleModuleEditState NewEditState) -> bool
		{
			if (!LODLevel)
			{
				return false;
			}

			TArray<UParticleModule*>& TargetModules = LODLevel->GetMutableModules();
			if (ModuleIndex < 0 || ModuleIndex >= static_cast<int32>(TargetModules.size()))
			{
				return false;
			}

			UParticleModule* SourceModule = GetModuleFromLOD(SourceLODIndex, ModuleIndex);
			if (!SourceModule)
			{
				return false;
			}

			TArray<EParticleModuleEditState>& ModuleEditStates = LODLevel->GetMutableModuleEditStates();
			LODLevel->NormalizeModuleEditStates(DisplayLODIndex == 0 ? EParticleModuleEditState::Duplicated : EParticleModuleEditState::InheritedLocked);
			const EParticleModuleEditState OldEditState = LODLevel->GetModuleEditState(ModuleIndex);
			UParticleModule* OldModule = TargetModules[ModuleIndex];

			if (NewEditState == EParticleModuleEditState::Shared)
			{
				if (!OldModule || OldModule == SourceModule)
				{
					UParticleModule* LocalPlaceholder = DuplicateParticleModule(SourceModule);
					if (!LocalPlaceholder)
					{
						return false;
					}

					TargetModules[ModuleIndex] = LocalPlaceholder;
				}

				if (ModuleIndex >= 0 && ModuleIndex < static_cast<int32>(ModuleEditStates.size()))
				{
					ModuleEditStates[ModuleIndex] = EParticleModuleEditState::Shared;
				}

				SelectModule(EmitterIndex, DisplayLODIndex, ModuleIndex);
				ApplyEditedObjectSideEffects(SourceModule);
				MarkDirty();
				RefreshParticleSystemComponents();
				return true;
			}

			UParticleModule* ReplacementModule = DuplicateParticleModule(SourceModule);
			if (!ReplacementModule)
			{
				return false;
			}

			TargetModules[ModuleIndex] = ReplacementModule;
			if (ModuleIndex >= 0 && ModuleIndex < static_cast<int32>(ModuleEditStates.size()))
			{
				ModuleEditStates[ModuleIndex] = NewEditState;
			}

			const bool bOldModuleWasSharedSource = OldEditState == EParticleModuleEditState::Shared && OldModule == SourceModule;
			if (OldModule && OldModule != ReplacementModule && !bOldModuleWasSharedSource)
			{
				UObjectManager::Get().DestroyObject(OldModule);
			}

			SelectModule(EmitterIndex, DisplayLODIndex, ModuleIndex);
			ApplyEditedObjectSideEffects(ReplacementModule);
			MarkDirty();
			RefreshParticleSystemComponents();
			return true;
		};

		auto DeleteModuleAcrossLODs = [&](int32 ModuleIndex) -> bool
		{
			if (!Emitter || ModuleIndex < 0)
			{
				return false;
			}

			TArray<UParticleModule*> RemovedModules;
			for (int32 LODIndex = 0; LODIndex < static_cast<int32>(Emitter->GetLODLevels().size()); ++LODIndex)
			{
				UParticleLODLevel* TargetLODLevel = Emitter->GetLODLevel(LODIndex);
				if (!TargetLODLevel)
				{
					continue;
				}

				TArray<UParticleModule*>& TargetModules = TargetLODLevel->GetMutableModules();
				if (ModuleIndex >= static_cast<int32>(TargetModules.size()))
				{
					continue;
				}

				UParticleModule* RemovedModule = TargetModules[ModuleIndex];
				TargetModules.erase(TargetModules.begin() + ModuleIndex);

				TArray<EParticleModuleEditState>& TargetEditStates = TargetLODLevel->GetMutableModuleEditStates();
				if (ModuleIndex < static_cast<int32>(TargetEditStates.size()))
				{
					TargetEditStates.erase(TargetEditStates.begin() + ModuleIndex);
				}
				TargetLODLevel->NormalizeModuleEditStates(LODIndex == 0 ? EParticleModuleEditState::Duplicated : EParticleModuleEditState::InheritedLocked);

				if (RemovedModule && std::find(RemovedModules.begin(), RemovedModules.end(), RemovedModule) == RemovedModules.end())
				{
					RemovedModules.push_back(RemovedModule);
				}
			}

			for (UParticleModule* RemovedModule : RemovedModules)
			{
				UObjectManager::Get().DestroyObject(RemovedModule);
			}

			UParticleLODLevel* SelectionLODLevel = Emitter->GetLODLevel(DisplayLODIndex);
			const int32 RemainingModuleCount = SelectionLODLevel ? static_cast<int32>(SelectionLODLevel->GetModules().size()) : 0;
			if (RemainingModuleCount <= 0)
			{
				SelectLOD(EmitterIndex, DisplayLODIndex);
			}
			else
			{
				SelectModule(EmitterIndex, DisplayLODIndex, (std::min)(ModuleIndex, RemainingModuleCount - 1));
			}

			MarkDirty();
			RefreshParticleSystemComponents();
			return !RemovedModules.empty();
		};

		const ImVec2 HeaderMin = ImGui::GetCursorScreenPos();
		const float HeaderWidth = ImGui::GetContentRegionAvail().x;
		const ImVec2 HeaderMax(HeaderMin.x + HeaderWidth, HeaderMin.y + EmitterHeaderHeight);
		ImDrawList* DrawList = ImGui::GetWindowDrawList();
		DrawList->AddRectFilled(HeaderMin, HeaderMax, IsEmitterSelected(EmitterIndex) ? IM_COL32(74, 76, 83, 255) : IM_COL32(55, 56, 61, 255));
		char Header[64];
		std::snprintf(Header, sizeof(Header), "Particle Emitter");
		DrawList->AddText(ImVec2(HeaderMin.x + 8.0f, HeaderMin.y + 7.0f), IM_COL32(240, 242, 245, 255), Header);
		if (Emitter)
		{
			char CountLabel[32];
			std::snprintf(CountLabel, sizeof(CountLabel), "%d", Emitter->GetMaxActiveParticles());
			DrawList->AddText(ImVec2(HeaderMax.x - 36.0f, HeaderMin.y + 30.0f), IM_COL32(230, 234, 238, 255), CountLabel);
		}

		bool bHeaderButtonHovered = false;
		if (Emitter)
		{
			const float SmallButtonSize = 14.0f;
			const float SmallButtonGap = 4.0f;
			const float SmallButtonY = HeaderMin.y + 31.0f;
			auto DrawSmallSquareButton = [&](const char* Id, const ImVec2& Pos, ImU32 NormalColor, ImU32 HoverColor, ImU32 ActiveColor, const char* Label, ImU32 TextColor)
			{
				ImGui::SetCursorScreenPos(Pos);
				ImGui::InvisibleButton(Id, ImVec2(SmallButtonSize, SmallButtonSize));
				const bool bClicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
				const bool bHovered = ImGui::IsItemHovered();
				const bool bActive = ImGui::IsItemActive();
				const ImU32 FillColor = bActive ? ActiveColor : (bHovered ? HoverColor : NormalColor);
				const ImVec2 Max(Pos.x + SmallButtonSize, Pos.y + SmallButtonSize);
				DrawList->AddRectFilled(Pos, Max, FillColor, 1.0f);
				DrawList->AddRect(Pos, Max, IM_COL32(8, 9, 11, 255), 1.0f);
				if (Label && Label[0] != '\0')
				{
					const ImVec2 TextSize = ImGui::CalcTextSize(Label);
					DrawList->AddText(
						ImVec2(Pos.x + (SmallButtonSize - TextSize.x) * 0.5f, Pos.y + (SmallButtonSize - TextSize.y) * 0.5f - 1.0f),
						TextColor,
						Label);
				}
				return bClicked;
			};

			const ImVec2 EnableButtonPos(HeaderMin.x + 8.0f, SmallButtonY);
			ImGui::SetCursorScreenPos(EnableButtonPos);
			ImGui::InvisibleButton("##EmitterEnabled", ImVec2(SmallButtonSize, SmallButtonSize));
			const bool bEnableClicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
			const bool bEnableHovered = ImGui::IsItemHovered();
			const bool bEnableActive = ImGui::IsItemActive();
			bHeaderButtonHovered |= bEnableHovered;
			if (bEnableClicked && LODLevel)
			{
				LODLevel->SetEnabled(!LODLevel->IsEnabled());
				SelectEmitter(EmitterIndex);
				MarkDirty();
				RefreshParticleSystemComponents();
			}
			const ImVec2 EnableButtonMax(EnableButtonPos.x + SmallButtonSize, EnableButtonPos.y + SmallButtonSize);
			const ImU32 EnableFillColor = bEnableActive
				? IM_COL32(48, 52, 57, 255)
				: (bEnableHovered ? IM_COL32(62, 66, 72, 255) : IM_COL32(28, 30, 34, 255));
			DrawList->AddRectFilled(EnableButtonPos, EnableButtonMax, EnableFillColor, 1.0f);
			DrawList->AddRect(EnableButtonPos, EnableButtonMax, IM_COL32(10, 11, 13, 255), 1.0f);
			if (LODLevel && LODLevel->IsEnabled())
			{
				DrawList->AddLine(ImVec2(EnableButtonPos.x + 3.0f, EnableButtonPos.y + 7.0f), ImVec2(EnableButtonPos.x + 6.0f, EnableButtonPos.y + 10.0f), IM_COL32(232, 236, 240, 255), 1.5f);
				DrawList->AddLine(ImVec2(EnableButtonPos.x + 6.0f, EnableButtonPos.y + 10.0f), ImVec2(EnableButtonPos.x + 11.0f, EnableButtonPos.y + 4.0f), IM_COL32(232, 236, 240, 255), 1.5f);
			}

			const ImVec2 TempButtonPos(EnableButtonPos.x + SmallButtonSize + SmallButtonGap, SmallButtonY);
			DrawSmallSquareButton(
				"##EmitterTempButton",
				TempButtonPos,
				IM_COL32(196, 210, 22, 255),
				IM_COL32(226, 240, 34, 255),
				IM_COL32(156, 170, 12, 255),
				"",
				IM_COL32(0, 0, 0, 0));
			bHeaderButtonHovered |= ImGui::IsItemHovered();

			const bool bSoloActive = ViewState.SoloEmitterIndex == EmitterIndex;
			const ImVec2 SoloButtonPos(TempButtonPos.x + SmallButtonSize + SmallButtonGap, SmallButtonY);
			if (DrawSmallSquareButton(
				"##EmitterSolo",
				SoloButtonPos,
				bSoloActive ? IM_COL32(58, 101, 135, 255) : IM_COL32(86, 112, 126, 255),
				bSoloActive ? IM_COL32(70, 120, 160, 255) : IM_COL32(100, 130, 146, 255),
				bSoloActive ? IM_COL32(42, 82, 112, 255) : IM_COL32(70, 96, 112, 255),
				"S",
				IM_COL32(222, 236, 246, 255)))
			{
				ViewState.SoloEmitterIndex = bSoloActive ? -1 : EmitterIndex;
				SelectEmitter(EmitterIndex);
				RestartPreviewSimulation();
			}
			bHeaderButtonHovered |= ImGui::IsItemHovered();
		}

		if (ImGui::IsMouseHoveringRect(HeaderMin, HeaderMax) && !bHeaderButtonHovered && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
		{
			SelectEmitter(EmitterIndex);
		}
		ImGui::SetCursorScreenPos(HeaderMin);
		ImGui::Dummy(ImVec2(HeaderWidth, EmitterHeaderHeight));

		if (Emitter)
		{
			bool bModuleContextMenuOpen = false;
			if (LODLevel)
			{
				FParticleModuleDropRequest DropRequest;
				TArray<UParticleModule*>& Modules = LODLevel->GetMutableModules();
				const UParticleModuleTypeDataBase* TypeDataModule = LODLevel->GetTypeDataModule();
				const char* TypeDataLabel = TypeDataModule ? GetTypeDataDisplayName(TypeDataModule) : GetRenderTypeLabel(RenderType);
				if (TypeDataModule)
				{
					ImGui::PushID(TypeDataModuleIndex);
					FModuleRowAction TypeDataAction;
					if (SelectableModuleRow(TypeDataLabel, IsModuleSelected(EmitterIndex, DisplayLODIndex, TypeDataModuleIndex), TypeDataModule, bModuleDirectEditLocked))
					{
						SelectModule(EmitterIndex, DisplayLODIndex, TypeDataModuleIndex);
					}
					RenderModuleContextMenu(TypeDataAction, bCanDirectEditModules, false, false, ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
					bModuleContextMenuOpen |= TypeDataAction.bContextMenuOpen;
					if (TypeDataAction.bDelete && bCanDirectEditModules)
					{
						SetTypeDataModule(nullptr);
					}
					else if (TypeDataAction.bRefresh)
					{
						SelectModule(EmitterIndex, DisplayLODIndex, TypeDataModuleIndex);
						RefreshParticleSystemComponents();
					}
					if (bCanDirectEditModules)
					{
						AcceptModuleDropTarget(DropRequest, EmitterIndex, DisplayLODIndex, 0, 0);
					}
					ImGui::PopID();
				}
				else
				{
					ImGui::PushID(TypeDataModuleIndex);
					if (SelectableModuleRow(TypeDataLabel, IsLODSelected(EmitterIndex, DisplayLODIndex), nullptr, bModuleDirectEditLocked))
					{
						SelectLOD(EmitterIndex, DisplayLODIndex);
					}
					if (bCanDirectEditModules)
					{
						AcceptModuleDropTarget(DropRequest, EmitterIndex, DisplayLODIndex, 0, 0);
					}
					ImGui::PopID();
				}

				bool bModuleListMutated = false;
				for (int32 ModuleIndex = 0; ModuleIndex < static_cast<int32>(Modules.size()); ++ModuleIndex)
				{
					UParticleModule* Module = LODLevel->ResolveModule(ModuleIndex, Emitter);
					const EParticleModuleEditState ModuleEditState = LODLevel->GetModuleEditState(ModuleIndex);
					const bool bModuleInheritedLocked = !bCanDirectEditModules
						&& (ModuleEditState == EParticleModuleEditState::InheritedLocked || ModuleEditState == EParticleModuleEditState::Shared);
					const bool bHasHigherModule = DisplayLODIndex > 0 && GetModuleFromLOD(DisplayLODIndex - 1, ModuleIndex) != nullptr;
					const bool bHasHighestModule = DisplayLODIndex > 0 && GetModuleFromLOD(0, ModuleIndex) != nullptr;
					const bool bDeleteLocked = IsModuleDeleteLocked(Module);
					const bool bOrderLocked = IsModuleOrderLocked(Module);
					ImGui::PushID(ModuleIndex);
					FParticleModuleDragPayload DragPayload{ EmitterIndex, DisplayLODIndex, ModuleIndex, Modules[ModuleIndex] };
					const FModuleRowAction Action = EditableModuleRow(
						GetModuleDisplayName(Module),
						Module,
						IsModuleSelected(EmitterIndex, DisplayLODIndex, ModuleIndex),
						!bDeleteLocked,
						bModuleInheritedLocked,
						bHasHigherModule,
						bHasHighestModule,
						(bCanDirectEditModules && !bOrderLocked) ? &DragPayload : nullptr,
						bCanDirectEditModules ? &DropRequest : nullptr,
						EmitterIndex,
						DisplayLODIndex,
						ModuleIndex,
						ModuleIndex + 1);
					if (Action.bSelect)
					{
						SelectModule(EmitterIndex, DisplayLODIndex, ModuleIndex);
					}
					if (Action.bShowCurves && Module)
					{
						SelectModule(EmitterIndex, DisplayLODIndex, ModuleIndex);
						SelectModuleDistributionCurves(Module);
					}
					bModuleContextMenuOpen |= Action.bContextMenuOpen;
					ImGui::PopID();

					if (Action.bToggleEnabled && Module && !bModuleInheritedLocked)
					{
						Module->SetEnabled(!Module->IsEnabled());
						SelectModule(EmitterIndex, DisplayLODIndex, ModuleIndex);
						ApplyEditedObjectSideEffects(Module);
						MarkDirty();
						RefreshParticleSystemComponents();
						bModuleListMutated = true;
					}
					else if (Action.bDelete && !bDeleteLocked)
					{
						bModuleListMutated = DeleteModuleAcrossLODs(ModuleIndex);
					}
					else if (Action.bRefresh)
					{
						SelectModule(EmitterIndex, DisplayLODIndex, ModuleIndex);
						RefreshParticleSystemComponents();
					}
					else if (Action.bDuplicateFromHigher)
					{
						bModuleListMutated = ReplaceModuleFromSource(ModuleIndex, DisplayLODIndex - 1, EParticleModuleEditState::Duplicated);
					}
					else if (Action.bShareFromHigher)
					{
						bModuleListMutated = ReplaceModuleFromSource(ModuleIndex, DisplayLODIndex - 1, EParticleModuleEditState::Shared);
					}
					else if (Action.bDuplicateFromHighest)
					{
						bModuleListMutated = ReplaceModuleFromSource(ModuleIndex, 0, EParticleModuleEditState::Duplicated);
					}

					if (bModuleListMutated)
					{
						break;
					}
				}

				ImGui::InvisibleButton("##ModuleDropTail", ImVec2(ImGui::GetContentRegionAvail().x, (std::max)(ModuleDropTargetHeight, ImGui::GetContentRegionAvail().y)));
				if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
				{
					SelectLOD(EmitterIndex, DisplayLODIndex);
				}
				if (bCanDirectEditModules)
				{
					AcceptModuleDropTarget(DropRequest, EmitterIndex, DisplayLODIndex, static_cast<int32>(Modules.size()), static_cast<int32>(Modules.size()));
					MoveDroppedModule(DropRequest);
				}
			}

			if (LODLevel && bCanDirectEditModules && ImGui::IsWindowHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Right) && !bModuleContextMenuOpen)
			{
				ImGui::OpenPopup("##ParticleEmitterContextMenu");
			}
			if (LODLevel && ImGui::BeginPopup("##ParticleEmitterContextMenu"))
			{
				RenderAddModuleMenu();
				ImGui::EndPopup();
			}
		}

		if (IsEmitterSelected(EmitterIndex))
		{
			const ImVec2 ColumnMin = ImGui::GetWindowPos();
			const ImVec2 ColumnMax(ColumnMin.x + ImGui::GetWindowSize().x, ColumnMin.y + ImGui::GetWindowSize().y);
			ImDrawList* ColumnDrawList = ImGui::GetWindowDrawList();
			ColumnDrawList->AddRect(ColumnMin, ColumnMax, IM_COL32(12, 14, 18, 235), 2.0f, 0, 3.0f);
			ColumnDrawList->AddRect(ImVec2(ColumnMin.x + 1.0f, ColumnMin.y + 1.0f), ImVec2(ColumnMax.x - 1.0f, ColumnMax.y - 1.0f), IM_COL32(105, 164, 232, 255), 1.0f, 0, 2.0f);
		}

		ImGui::EndChild();
		ImGui::PopID();
		if (EmitterIndex + 1 < static_cast<int32>(Emitters.size()))
		{
			ImGui::SameLine();
		}
	}

	if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemHovered())
	{
		bSelectParticleSystemRequested = true;
	}

	if (DuplicateEmitterIndex >= 0)
	{
		TArray<UParticleEmitter*>& MutableEmitters = ParticleSystem->GetMutableEmitters();
		if (DuplicateEmitterIndex < static_cast<int32>(MutableEmitters.size()))
		{
			UParticleEmitter* DuplicatedEmitter = DuplicateParticleEmitter(MutableEmitters[DuplicateEmitterIndex]);
			if (DuplicatedEmitter)
			{
				const int32 TargetIndex = DuplicateEmitterIndex + 1;
				MutableEmitters.insert(MutableEmitters.begin() + TargetIndex, DuplicatedEmitter);
				if (ViewState.SoloEmitterIndex >= TargetIndex)
				{
					++ViewState.SoloEmitterIndex;
				}
				ParticleSystem->NormalizeLODLevels();
				SelectEmitter(TargetIndex);
				MarkDirty();
				RefreshParticleSystemComponents();
			}
		}
	}
	else if (DeleteEmitterIndex >= 0)
	{
		TArray<UParticleEmitter*>& MutableEmitters = ParticleSystem->GetMutableEmitters();
		if (DeleteEmitterIndex < static_cast<int32>(MutableEmitters.size()))
		{
			UParticleEmitter* RemovedEmitter = MutableEmitters[DeleteEmitterIndex];
			MutableEmitters.erase(MutableEmitters.begin() + DeleteEmitterIndex);
			UObjectManager::Get().DestroyObject(RemovedEmitter);
			if (ViewState.SoloEmitterIndex == DeleteEmitterIndex)
			{
				ViewState.SoloEmitterIndex = -1;
			}
			else if (ViewState.SoloEmitterIndex > DeleteEmitterIndex)
			{
				--ViewState.SoloEmitterIndex;
			}
			ParticleSystem->NormalizeLODLevels();

			if (MutableEmitters.empty())
			{
				SelectParticleSystem();
			}
			else
			{
				const int32 NewEmitterIndex = (std::min)(DeleteEmitterIndex, static_cast<int32>(MutableEmitters.size()) - 1);
				SelectEmitter(NewEmitterIndex);
			}

			MarkDirty();
			RefreshParticleSystemComponents();
		}
	}
	else if (DefaultEmitterInsertIndex >= 0)
	{
		TArray<UParticleEmitter*>& MutableEmitters = ParticleSystem->GetMutableEmitters();
		const int32 OldEmitterCount = static_cast<int32>(MutableEmitters.size());
		UParticleEmitter* NewEmitter = ParticleSystem->AddDefaultEmitter();
		if (NewEmitter && !MutableEmitters.empty())
		{
			const int32 AppendedIndex = static_cast<int32>(MutableEmitters.size()) - 1;
			const int32 TargetIndex = (std::max)(0, (std::min)(DefaultEmitterInsertIndex, OldEmitterCount));
			if (TargetIndex != AppendedIndex)
			{
				MutableEmitters.erase(MutableEmitters.begin() + AppendedIndex);
				MutableEmitters.insert(MutableEmitters.begin() + TargetIndex, NewEmitter);
				if (ViewState.SoloEmitterIndex >= TargetIndex && ViewState.SoloEmitterIndex < AppendedIndex)
				{
					++ViewState.SoloEmitterIndex;
				}
			}

			ParticleSystem->NormalizeLODLevels();
			SelectLOD(TargetIndex, ClampSystemLODIndex(ParticleSystem, ViewState.Selection.LODIndex));
			MarkDirty();
			RefreshParticleSystemComponents();
		}
	}
	else if (bSelectParticleSystemRequested)
	{
		SelectParticleSystem();
	}

	ImGui::EndChild();
}

void FParticleSystemEditorWidget::ClearSelectedCurve()
{
	CurveSelection = FCurveEditorSelection{};
	CurveEditorState = FCurveEditorState{};
}

bool FParticleSystemEditorWidget::AppendDistributionCurve(UObject* OwnerObject, FRawDistributionFloat* Distribution, const FString& Label)
{
	if (!OwnerObject || !Distribution)
	{
		return false;
	}

	const bool bChanged = EnsureFloatDistributionHasCurve(*Distribution);
	CurveSelection.OwnerObject = OwnerObject;
	auto AddCurve = [&](FFloatCurve* Curve, const FString& TrackLabel)
	{
		if (!Curve || CurveSelection.CurveCount >= MaxCurveEditorTracks)
		{
			return;
		}

		CurveSelection.Curves[CurveSelection.CurveCount] = Curve;
		CurveSelection.CurveLabels[CurveSelection.CurveCount] = TrackLabel;
		++CurveSelection.CurveCount;
	};

	if (Distribution->Mode == EDistributionValueMode::ConstantCurve)
	{
		AddCurve(&Distribution->ConstantCurve, Label);
	}
	else if (Distribution->Mode == EDistributionValueMode::UniformCurve)
	{
		AddCurve(&Distribution->MinCurve, Label + ".Min");
		AddCurve(&Distribution->MaxCurve, Label + ".Max");
	}

	return bChanged;
}

bool FParticleSystemEditorWidget::AppendDistributionCurve(UObject* OwnerObject, FRawDistributionVector* Distribution, const FString& Label)
{
	if (!OwnerObject || !Distribution)
	{
		return false;
	}

	const bool bChanged = EnsureVectorDistributionHasCurve(*Distribution);
	CurveSelection.OwnerObject = OwnerObject;
	auto AddCurve = [&](FFloatCurve* Curve, const FString& TrackLabel)
	{
		if (!Curve || CurveSelection.CurveCount >= MaxCurveEditorTracks)
		{
			return;
		}

		CurveSelection.Curves[CurveSelection.CurveCount] = Curve;
		CurveSelection.CurveLabels[CurveSelection.CurveCount] = TrackLabel;
		++CurveSelection.CurveCount;
	};

	if (Distribution->Mode == EDistributionValueMode::ConstantCurve)
	{
		AddCurve(&Distribution->ConstantCurve.X, Label + ".X");
		AddCurve(&Distribution->ConstantCurve.Y, Label + ".Y");
		AddCurve(&Distribution->ConstantCurve.Z, Label + ".Z");
	}
	else if (Distribution->Mode == EDistributionValueMode::UniformCurve)
	{
		AddCurve(&Distribution->MinCurve.X, Label + ".Min X");
		AddCurve(&Distribution->MinCurve.Y, Label + ".Min Y");
		AddCurve(&Distribution->MinCurve.Z, Label + ".Min Z");
		AddCurve(&Distribution->MaxCurve.X, Label + ".Max X");
		AddCurve(&Distribution->MaxCurve.Y, Label + ".Max Y");
		AddCurve(&Distribution->MaxCurve.Z, Label + ".Max Z");
	}

	return bChanged;
}

void FParticleSystemEditorWidget::SelectDistributionCurve(UObject* OwnerObject, FRawDistributionFloat* Distribution, const FString& Label, int32 CurveIndex)
{
	if (!OwnerObject || !Distribution)
	{
		ClearSelectedCurve();
		return;
	}

	ClearSelectedCurve();
	CurveSelection.Label = Label;
	const bool bChanged = AppendDistributionCurve(OwnerObject, Distribution, Label);
	CurveSelection.ActiveCurveIndex = (std::max)(0, (std::min)(CurveIndex, CurveSelection.CurveCount > 0 ? CurveSelection.CurveCount - 1 : 0));
	CurveEditorState.SelectedKeyIndex = -1;
	CurveEditorState.bDraggingSelectedKey = false;
	CurveEditorState.DraggingTangentHandle = ETangentHandle::None;
	CurveEditorState.bPanningView = false;
	CurveEditorState.bSuppressNextCanvasContextMenu = false;
	ViewState.bShowCurveEditor = true;
	FitCurveViewToSelectedCurves();
	if (bChanged)
	{
		OwnerObject->PostEditProperty(nullptr);
		MarkDirty();
		RefreshParticleSystemComponents();
	}
}

void FParticleSystemEditorWidget::SelectDistributionCurve(UObject* OwnerObject, FRawDistributionVector* Distribution, const FString& Label, int32 CurveIndex)
{
	if (!OwnerObject || !Distribution)
	{
		ClearSelectedCurve();
		return;
	}

	ClearSelectedCurve();
	CurveSelection.Label = Label;
	const bool bChanged = AppendDistributionCurve(OwnerObject, Distribution, Label);
	CurveSelection.ActiveCurveIndex = (CurveIndex < 0 && CurveSelection.CurveCount > 1)
		? -1
		: (std::max)(0, (std::min)(CurveIndex, CurveSelection.CurveCount > 0 ? CurveSelection.CurveCount - 1 : 0));
	CurveEditorState.SelectedKeyIndex = -1;
	CurveEditorState.bDraggingSelectedKey = false;
	CurveEditorState.DraggingTangentHandle = ETangentHandle::None;
	CurveEditorState.bPanningView = false;
	CurveEditorState.bSuppressNextCanvasContextMenu = false;
	ViewState.bShowCurveEditor = true;
	FitCurveViewToSelectedCurves();
	if (bChanged)
	{
		OwnerObject->PostEditProperty(nullptr);
		MarkDirty();
		RefreshParticleSystemComponents();
	}
}

bool FParticleSystemEditorWidget::SelectModuleDistributionCurves(UParticleModule* Module)
{
	if (!Module)
	{
		return false;
	}

	ClearSelectedCurve();
	CurveSelection.Label = GetModuleDisplayName(Module);
	bool bChanged = false;
	if (UParticleModuleLifetime* LifetimeModule = Cast<UParticleModuleLifetime>(Module))
	{
		bChanged |= AppendDistributionCurve(Module, &LifetimeModule->Lifetime, "Lifetime");
	}
	else if (UParticleModuleLocation* LocationModule = Cast<UParticleModuleLocation>(Module))
	{
		bChanged |= AppendDistributionCurve(Module, &LocationModule->StartLocation, "StartLocation");
	}
	else if (UParticleModuleVelocity* VelocityModule = Cast<UParticleModuleVelocity>(Module))
	{
		bChanged |= AppendDistributionCurve(Module, &VelocityModule->StartVelocity, "StartVelocity");
	}
	else if (UParticleModuleSize* SizeModule = Cast<UParticleModuleSize>(Module))
	{
		bChanged |= AppendDistributionCurve(Module, &SizeModule->StartSize, "StartSize");
	}

	if (CurveSelection.CurveCount <= 0)
	{
		ClearSelectedCurve();
		return false;
	}

	CurveSelection.ActiveCurveIndex = CurveSelection.CurveCount > 1 ? -1 : 0;
	CurveEditorState.SelectedKeyIndex = -1;
	CurveEditorState.bDraggingSelectedKey = false;
	CurveEditorState.DraggingTangentHandle = ETangentHandle::None;
	CurveEditorState.bPanningView = false;
	CurveEditorState.bSuppressNextCanvasContextMenu = false;
	ViewState.bShowCurveEditor = true;
	FitCurveViewToSelectedCurves();
	if (bChanged)
	{
		Module->PostEditProperty(nullptr);
		MarkDirty();
		RefreshParticleSystemComponents();
	}
	return true;
}

void FParticleSystemEditorWidget::RemoveSelectedCurve(int32 CurveIndex)
{
	if (CurveIndex < 0 || CurveIndex >= CurveSelection.CurveCount)
	{
		return;
	}

	for (int32 Index = CurveIndex; Index + 1 < CurveSelection.CurveCount; ++Index)
	{
		CurveSelection.Curves[Index] = CurveSelection.Curves[Index + 1];
		CurveSelection.CurveLabels[Index] = CurveSelection.CurveLabels[Index + 1];
	}
	--CurveSelection.CurveCount;
	CurveSelection.Curves[CurveSelection.CurveCount] = nullptr;
	CurveSelection.CurveLabels[CurveSelection.CurveCount].clear();

	if (CurveSelection.CurveCount <= 0)
	{
		ClearSelectedCurve();
		return;
	}

	if (CurveSelection.ActiveCurveIndex >= CurveSelection.CurveCount)
	{
		CurveSelection.ActiveCurveIndex = CurveSelection.CurveCount - 1;
	}
	if (CurveSelection.ActiveCurveIndex == CurveIndex)
	{
		CurveEditorState.SelectedKeyIndex = -1;
	}
	FitCurveViewToSelectedCurves();
}

int32 FParticleSystemEditorWidget::GetSelectedCurveCount() const
{
	return CurveSelection.CurveCount;
}

FFloatCurve* FParticleSystemEditorWidget::GetSelectedCurve(int32 CurveIndex) const
{
	if (CurveIndex < 0 || CurveIndex >= CurveSelection.CurveCount)
	{
		return nullptr;
	}
	return CurveSelection.Curves[CurveIndex];
}

FFloatCurve* FParticleSystemEditorWidget::GetActiveSelectedCurve() const
{
	if (CurveSelection.ActiveCurveIndex < 0)
	{
		return GetSelectedCurve(0);
	}
	return GetSelectedCurve(CurveSelection.ActiveCurveIndex);
}

void FParticleSystemEditorWidget::FitCurveViewToSelectedCurves()
{
	bool bHasKey = false;
	for (int32 CurveIndex = 0; CurveIndex < GetSelectedCurveCount(); ++CurveIndex)
	{
		const FFloatCurve* Curve = GetSelectedCurve(CurveIndex);
		if (!Curve)
		{
			continue;
		}
		for (const FCurveKey& Key : Curve->Keys)
		{
			if (!bHasKey)
			{
				CurveEditorState.ViewMinTime = Key.Time;
				CurveEditorState.ViewMaxTime = Key.Time;
				CurveEditorState.ViewMinValue = Key.Value;
				CurveEditorState.ViewMaxValue = Key.Value;
				bHasKey = true;
			}
			else
			{
				CurveEditorState.ViewMinTime = (std::min)(CurveEditorState.ViewMinTime, Key.Time);
				CurveEditorState.ViewMaxTime = (std::max)(CurveEditorState.ViewMaxTime, Key.Time);
				CurveEditorState.ViewMinValue = (std::min)(CurveEditorState.ViewMinValue, Key.Value);
				CurveEditorState.ViewMaxValue = (std::max)(CurveEditorState.ViewMaxValue, Key.Value);
			}
		}
	}

	if (!bHasKey)
	{
		CurveEditorState.ViewMinTime = 0.0f;
		CurveEditorState.ViewMaxTime = 1.0f;
		CurveEditorState.ViewMinValue = -1.0f;
		CurveEditorState.ViewMaxValue = 1.0f;
		return;
	}

	if (CurveEditorState.ViewMaxTime <= CurveEditorState.ViewMinTime + CurveTimeEpsilon)
	{
		CurveEditorState.ViewMinTime -= 0.5f;
		CurveEditorState.ViewMaxTime += 0.5f;
	}
	else
	{
		const float TimePadding = (CurveEditorState.ViewMaxTime - CurveEditorState.ViewMinTime) * 0.08f;
		CurveEditorState.ViewMinTime -= TimePadding;
		CurveEditorState.ViewMaxTime += TimePadding;
	}
	if (CurveEditorState.ViewMaxValue <= CurveEditorState.ViewMinValue + CurveTimeEpsilon)
	{
		const float CenterValue = CurveEditorState.ViewMinValue;
		const float HalfRange = (std::max)(0.05f, std::fabs(CenterValue) * 0.1f);
		CurveEditorState.ViewMinValue = CenterValue - HalfRange;
		CurveEditorState.ViewMaxValue = CenterValue + HalfRange;
	}
	else
	{
		const float ValuePadding = (std::max)(0.01f, (CurveEditorState.ViewMaxValue - CurveEditorState.ViewMinValue) * 0.15f);
		CurveEditorState.ViewMinValue -= ValuePadding;
		CurveEditorState.ViewMaxValue += ValuePadding;
	}
}

bool FParticleSystemEditorWidget::RenderSelectedCurveEditor(const ImVec2& Size)
{
	FFloatCurve* ActiveCurve = GetActiveSelectedCurve();
	if (!ActiveCurve)
	{
		return false;
	}

	bool bChanged = false;
	const ImVec2 CanvasSize((std::max)(160.0f, Size.x), (std::max)(100.0f, Size.y));
	const ImVec2 CanvasMin = ImGui::GetCursorScreenPos();
	const ImVec2 CanvasMax(CanvasMin.x + CanvasSize.x, CanvasMin.y + CanvasSize.y);

	ImGui::InvisibleButton("##ParticleCurveCanvas", CanvasSize);
	const bool bCanvasHovered = ImGui::IsItemHovered();
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	const ImGuiIO& IO = ImGui::GetIO();
	const bool bEditAllCurves = CurveSelection.ActiveCurveIndex < 0 && GetSelectedCurveCount() > 1;
	auto ForEditableCurves = [&](auto&& Function)
	{
		if (bEditAllCurves)
		{
			for (int32 CurveIndex = 0; CurveIndex < GetSelectedCurveCount(); ++CurveIndex)
			{
				if (FFloatCurve* Curve = GetSelectedCurve(CurveIndex))
				{
					Function(*Curve);
				}
			}
		}
		else
		{
			Function(*ActiveCurve);
		}
	};

	DrawList->AddRectFilled(CanvasMin, CanvasMax, IM_COL32(57, 57, 57, 255));
	DrawList->AddRect(CanvasMin, CanvasMax, IM_COL32(20, 20, 20, 255));
	DrawList->PushClipRect(CanvasMin, CanvasMax, true);
	auto FormatGridValue = [](float Value, char* Buffer, size_t BufferSize)
	{
		std::snprintf(Buffer, BufferSize, "%.2f", Value);
	};
	auto GetNiceGridStep = [](float Range, float DesiredLineCount)
	{
		if (Range <= 0.0f)
		{
			return 1.0f;
		}
		const float TargetStep = Range / (std::max)(1.0f, DesiredLineCount);
		const float Power = std::pow(10.0f, std::floor(std::log10(TargetStep)));
		const float Normalized = TargetStep / Power;
		float Nice = 10.0f;
		if (Normalized <= 1.0f)
		{
			Nice = 1.0f;
		}
		else if (Normalized <= 2.0f)
		{
			Nice = 2.0f;
		}
		else if (Normalized <= 5.0f)
		{
			Nice = 5.0f;
		}
		return Nice * Power;
	};
	const float TimeGridStep = GetNiceGridStep(CurveEditorState.ViewMaxTime - CurveEditorState.ViewMinTime, CanvasSize.x / 72.0f);
	const float FirstTimeTick = std::ceil(CurveEditorState.ViewMinTime / TimeGridStep) * TimeGridStep;
	for (float Time = FirstTimeTick; Time <= CurveEditorState.ViewMaxTime + 0.001f; Time += TimeGridStep)
	{
		const float T = (Time - CurveEditorState.ViewMinTime) / (CurveEditorState.ViewMaxTime - CurveEditorState.ViewMinTime);
		const ImU32 GridColor = std::fabs(Time) < 0.001f ? IM_COL32(178, 178, 178, 230) : IM_COL32(150, 150, 150, 210);
		const float GridX = CanvasMin.x + T * CanvasSize.x;
		DrawList->AddLine(ImVec2(GridX, CanvasMin.y), ImVec2(GridX, CanvasMax.y), GridColor, 1.0f);

		char TimeLabel[32] = {};
		FormatGridValue(Time, TimeLabel, sizeof(TimeLabel));
		const float LabelWidth = ImGui::CalcTextSize(TimeLabel).x;
		const float LabelX = (std::min)((std::max)(GridX + 3.0f, CanvasMin.x + 3.0f), CanvasMax.x - LabelWidth - 3.0f);
		DrawList->AddText(ImVec2(LabelX, CanvasMax.y - 14.0f), IM_COL32(232, 232, 232, 245), TimeLabel);
	}
	const float ValueGridStep = GetNiceGridStep(CurveEditorState.ViewMaxValue - CurveEditorState.ViewMinValue, CanvasSize.y / 38.0f);
	const float FirstValueTick = std::ceil(CurveEditorState.ViewMinValue / ValueGridStep) * ValueGridStep;
	for (float Value = FirstValueTick; Value <= CurveEditorState.ViewMaxValue + 0.001f; Value += ValueGridStep)
	{
		const float T = (Value - CurveEditorState.ViewMinValue) / (CurveEditorState.ViewMaxValue - CurveEditorState.ViewMinValue);
		const float GridY = CanvasMax.y - T * CanvasSize.y;
		const ImU32 GridColor = std::fabs(Value) < 0.001f ? IM_COL32(178, 178, 178, 230) : IM_COL32(150, 150, 150, 210);
		DrawList->AddLine(ImVec2(CanvasMin.x, GridY), ImVec2(CanvasMax.x, GridY), GridColor, 1.0f);

		char ValueLabel[32] = {};
		FormatGridValue(Value, ValueLabel, sizeof(ValueLabel));
		const float LabelY = (std::min)((std::max)(GridY + 2.0f, CanvasMin.y + 2.0f), CanvasMax.y - ImGui::GetTextLineHeight() - 2.0f);
		DrawList->AddText(ImVec2(CanvasMin.x + 4.0f, LabelY), IM_COL32(232, 232, 232, 245), ValueLabel);
	}
	DrawList->PopClipRect();

	auto DrawCurveLine = [&](const FFloatCurve& Curve, ImU32 Color)
	{
		if (Curve.IsEmpty())
		{
			return;
		}

		const int32 SampleCount = 128;
		ImVec2 PreviousPoint = CurveToScreen(
			CurveEditorState.ViewMinTime,
			Curve.Evaluate(CurveEditorState.ViewMinTime),
			CurveEditorState.ViewMinTime,
			CurveEditorState.ViewMaxTime,
			CurveEditorState.ViewMinValue,
			CurveEditorState.ViewMaxValue,
			CanvasMin,
			CanvasMax);

		for (int32 SampleIndex = 1; SampleIndex < SampleCount; ++SampleIndex)
		{
			const float Alpha = static_cast<float>(SampleIndex) / static_cast<float>(SampleCount - 1);
			const float SampleTime = CurveEditorState.ViewMinTime + (CurveEditorState.ViewMaxTime - CurveEditorState.ViewMinTime) * Alpha;
			const ImVec2 CurrentPoint = CurveToScreen(
				SampleTime,
				Curve.Evaluate(SampleTime),
				CurveEditorState.ViewMinTime,
				CurveEditorState.ViewMaxTime,
				CurveEditorState.ViewMinValue,
				CurveEditorState.ViewMaxValue,
				CanvasMin,
				CanvasMax);
			if (IsFinitePoint(PreviousPoint) && IsFinitePoint(CurrentPoint))
			{
				DrawList->AddLine(PreviousPoint, CurrentPoint, Color, 2.0f);
			}
			PreviousPoint = CurrentPoint;
		}
	};

	if (GetSelectedCurveCount() > 1)
	{
		for (int32 CurveIndex = 0; CurveIndex < GetSelectedCurveCount(); ++CurveIndex)
		{
			if (FFloatCurve* Curve = GetSelectedCurve(CurveIndex))
			{
				DrawCurveLine(*Curve, GetCurveTrackColor(CurveIndex, !(CurveSelection.ActiveCurveIndex < 0 || CurveSelection.ActiveCurveIndex == CurveIndex)));
			}
		}
	}
	else
	{
		DrawCurveLine(*ActiveCurve, IM_COL32(80, 220, 120, 255));
	}

	ETangentHandle HoveredTangentHandle = ETangentHandle::None;
	const bool bHasSelectedKey = CurveEditorState.SelectedKeyIndex >= 0 && CurveEditorState.SelectedKeyIndex < static_cast<int32>(ActiveCurve->Keys.size());
	const bool bCanShowArriveHandle =
		bHasSelectedKey &&
		CurveEditorState.SelectedKeyIndex > 0 &&
		ActiveCurve->Keys[CurveEditorState.SelectedKeyIndex - 1].InterpMode == ECurveInterpMode::Cubic;
	const bool bCanShowLeaveHandle =
		bHasSelectedKey &&
		CurveEditorState.SelectedKeyIndex + 1 < static_cast<int32>(ActiveCurve->Keys.size()) &&
		ActiveCurve->Keys[CurveEditorState.SelectedKeyIndex].InterpMode == ECurveInterpMode::Cubic;

	if (bHasSelectedKey && (bCanShowArriveHandle || bCanShowLeaveHandle))
	{
		const FCurveKey& Key = ActiveCurve->Keys[CurveEditorState.SelectedKeyIndex];
		const ImVec2 KeyPos = CurveToScreen(Key.Time, Key.Value, CurveEditorState.ViewMinTime, CurveEditorState.ViewMaxTime, CurveEditorState.ViewMinValue, CurveEditorState.ViewMaxValue, CanvasMin, CanvasMax);
		if (bCanShowArriveHandle)
		{
			const ImVec2 HandlePos = GetTangentHandlePosition(Key, true, CurveEditorState.ViewMinTime, CurveEditorState.ViewMaxTime, CurveEditorState.ViewMinValue, CurveEditorState.ViewMaxValue, CanvasMin, CanvasMax);
			if (IsPointNear(IO.MousePos, HandlePos, CurveTangentHandleHitRadius))
			{
				HoveredTangentHandle = ETangentHandle::Arrive;
			}
			DrawList->AddLine(KeyPos, HandlePos, IM_COL32(95, 150, 255, 180), 1.5f);
			DrawList->AddCircleFilled(HandlePos, 4.5f, IM_COL32(95, 150, 255, 255));
		}
		if (bCanShowLeaveHandle)
		{
			const ImVec2 HandlePos = GetTangentHandlePosition(Key, false, CurveEditorState.ViewMinTime, CurveEditorState.ViewMaxTime, CurveEditorState.ViewMinValue, CurveEditorState.ViewMaxValue, CanvasMin, CanvasMax);
			if (IsPointNear(IO.MousePos, HandlePos, CurveTangentHandleHitRadius))
			{
				HoveredTangentHandle = ETangentHandle::Leave;
			}
			DrawList->AddLine(KeyPos, HandlePos, IM_COL32(95, 150, 255, 180), 1.5f);
			DrawList->AddCircleFilled(HandlePos, 4.5f, IM_COL32(95, 150, 255, 255));
		}
	}

	int32 HoveredKeyIndex = -1;
	for (int32 KeyIndex = 0; KeyIndex < static_cast<int32>(ActiveCurve->Keys.size()); ++KeyIndex)
	{
		const FCurveKey& Key = ActiveCurve->Keys[KeyIndex];
		const ImVec2 KeyPos = CurveToScreen(Key.Time, Key.Value, CurveEditorState.ViewMinTime, CurveEditorState.ViewMaxTime, CurveEditorState.ViewMinValue, CurveEditorState.ViewMaxValue, CanvasMin, CanvasMax);
		if (IsPointNear(IO.MousePos, KeyPos, CurveKeyHitRadius))
		{
			HoveredKeyIndex = KeyIndex;
		}
		const ImU32 KeyColor =
			(CurveEditorState.SelectedKeyIndex == KeyIndex) ? IM_COL32(255, 245, 110, 255) :
			(HoveredKeyIndex == KeyIndex) ? IM_COL32(255, 205, 90, 255) :
			IM_COL32(255, 165, 60, 255);
		DrawList->AddCircleFilled(KeyPos, 5.0f, KeyColor);
		DrawList->AddCircle(KeyPos, 5.0f, IM_COL32(20, 20, 22, 220), 12, 1.0f);
	}

	if (bCanvasHovered && IO.MouseWheel != 0.0f)
	{
		float MouseTime = 0.0f;
		float MouseValue = 0.0f;
		ScreenToCurve(IO.MousePos, CurveEditorState.ViewMinTime, CurveEditorState.ViewMaxTime, CurveEditorState.ViewMinValue, CurveEditorState.ViewMaxValue, CanvasMin, CanvasMax, MouseTime, MouseValue);
		const float Zoom = (IO.MouseWheel > 0.0f) ? 0.85f : 1.1764706f;
		if (!IO.KeyCtrl)
		{
			CurveEditorState.ViewMinTime = MouseTime + (CurveEditorState.ViewMinTime - MouseTime) * Zoom;
			CurveEditorState.ViewMaxTime = MouseTime + (CurveEditorState.ViewMaxTime - MouseTime) * Zoom;
			CurveEditorState.ViewMaxTime = (std::max)(CurveEditorState.ViewMaxTime, CurveEditorState.ViewMinTime + CurveTimeEpsilon);
		}
		if (!IO.KeyShift)
		{
			CurveEditorState.ViewMinValue = MouseValue + (CurveEditorState.ViewMinValue - MouseValue) * Zoom;
			CurveEditorState.ViewMaxValue = MouseValue + (CurveEditorState.ViewMaxValue - MouseValue) * Zoom;
			CurveEditorState.ViewMaxValue = (std::max)(CurveEditorState.ViewMaxValue, CurveEditorState.ViewMinValue + CurveTimeEpsilon);
		}
	}

	if (HoveredTangentHandle != ETangentHandle::None && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		CurveEditorState.DraggingTangentHandle = HoveredTangentHandle;
		CurveEditorState.bDraggingSelectedKey = false;
	}
	else if (HoveredKeyIndex != -1 && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		CurveEditorState.SelectedKeyIndex = HoveredKeyIndex;
		CurveEditorState.bDraggingSelectedKey = true;
	}
	else if (bCanvasHovered && HoveredKeyIndex == -1 && HoveredTangentHandle == ETangentHandle::None && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		CurveEditorState.SelectedKeyIndex = -1;
	}

	if (CurveEditorState.DraggingTangentHandle != ETangentHandle::None && bHasSelectedKey && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
	{
		FCurveKey& Key = ActiveCurve->Keys[CurveEditorState.SelectedKeyIndex];
		float MouseTime = 0.0f;
		float MouseValue = 0.0f;
		ScreenToCurve(IO.MousePos, CurveEditorState.ViewMinTime, CurveEditorState.ViewMaxTime, CurveEditorState.ViewMinValue, CurveEditorState.ViewMaxValue, CanvasMin, CanvasMax, MouseTime, MouseValue);

		const bool bArrive = CurveEditorState.DraggingTangentHandle == ETangentHandle::Arrive;
		const float DeltaTime = bArrive ? Key.Time - MouseTime : MouseTime - Key.Time;
		const float NewTangent = std::fabs(DeltaTime) > CurveTimeEpsilon ? (bArrive ? (Key.Value - MouseValue) / DeltaTime : (MouseValue - Key.Value) / DeltaTime) : (bArrive ? Key.ArriveTangent : Key.LeaveTangent);
		if (Key.TangentMode == ECurveTangentMode::Auto)
		{
			Key.TangentMode = ECurveTangentMode::User;
		}
		if (Key.TangentMode == ECurveTangentMode::Break)
		{
			if (bArrive)
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

	if (CurveEditorState.bDraggingSelectedKey && CurveEditorState.SelectedKeyIndex >= 0 && CurveEditorState.SelectedKeyIndex < static_cast<int32>(ActiveCurve->Keys.size()) && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
	{
		const float TimeDelta = (IO.MouseDelta.x / CanvasSize.x) * (CurveEditorState.ViewMaxTime - CurveEditorState.ViewMinTime);
		const float ValueDelta = -(IO.MouseDelta.y / CanvasSize.y) * (CurveEditorState.ViewMaxValue - CurveEditorState.ViewMinValue);
		ForEditableCurves([&](FFloatCurve& Curve)
		{
			if (CurveEditorState.SelectedKeyIndex < 0 || CurveEditorState.SelectedKeyIndex >= static_cast<int32>(Curve.Keys.size()))
			{
				return;
			}
			FCurveKey& Key = Curve.Keys[CurveEditorState.SelectedKeyIndex];
			Key.Time += TimeDelta;
			Key.Value += ValueDelta;
			if (CurveEditorState.SelectedKeyIndex > 0)
			{
				Key.Time = (std::max)(Key.Time, Curve.Keys[CurveEditorState.SelectedKeyIndex - 1].Time + CurveTimeEpsilon);
			}
			if (CurveEditorState.SelectedKeyIndex + 1 < static_cast<int32>(Curve.Keys.size()))
			{
				Key.Time = (std::min)(Key.Time, Curve.Keys[CurveEditorState.SelectedKeyIndex + 1].Time - CurveTimeEpsilon);
			}
		});
		bChanged = true;
	}

	if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
	{
		if (CurveEditorState.bDraggingSelectedKey)
		{
			ForEditableCurves([](FFloatCurve& Curve)
			{
				Curve.SortKeys();
				Curve.AutoSetTangents();
			});
		}
		CurveEditorState.bDraggingSelectedKey = false;
		CurveEditorState.DraggingTangentHandle = ETangentHandle::None;
	}

	if (bCanvasHovered && HoveredKeyIndex == -1 && HoveredTangentHandle == ETangentHandle::None && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
	{
		CurveEditorState.bPanningView = true;
		CurveEditorState.bSuppressNextCanvasContextMenu = false;
	}
	if (CurveEditorState.bPanningView && ImGui::IsMouseDragging(ImGuiMouseButton_Right, 3.0f))
	{
		const float TimeDelta = (IO.MouseDelta.x / CanvasSize.x) * (CurveEditorState.ViewMaxTime - CurveEditorState.ViewMinTime);
		const float ValueDelta = (IO.MouseDelta.y / CanvasSize.y) * (CurveEditorState.ViewMaxValue - CurveEditorState.ViewMinValue);
		CurveEditorState.ViewMinTime -= TimeDelta;
		CurveEditorState.ViewMaxTime -= TimeDelta;
		CurveEditorState.ViewMinValue += ValueDelta;
		CurveEditorState.ViewMaxValue += ValueDelta;
		CurveEditorState.bSuppressNextCanvasContextMenu = true;
	}
	if (ImGui::IsMouseReleased(ImGuiMouseButton_Right))
	{
		CurveEditorState.bPanningView = false;
	}

	if (HoveredKeyIndex != -1 && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
	{
		CurveEditorState.SelectedKeyIndex = HoveredKeyIndex;
		ImGui::OpenPopup("ParticleCurveKeyContext");
	}
	else if (bCanvasHovered && HoveredKeyIndex == -1 && HoveredTangentHandle == ETangentHandle::None && ImGui::IsMouseReleased(ImGuiMouseButton_Right) && !CurveEditorState.bSuppressNextCanvasContextMenu)
	{
		ScreenToCurve(IO.MousePos, CurveEditorState.ViewMinTime, CurveEditorState.ViewMaxTime, CurveEditorState.ViewMinValue, CurveEditorState.ViewMaxValue, CanvasMin, CanvasMax, CurveEditorState.PendingContextTime, CurveEditorState.PendingContextValue);
		ImGui::OpenPopup("ParticleCurveCanvasContext");
	}

	if (ImGui::BeginPopup("ParticleCurveKeyContext"))
	{
		if (CurveEditorState.SelectedKeyIndex >= 0 && CurveEditorState.SelectedKeyIndex < static_cast<int32>(ActiveCurve->Keys.size()))
		{
			if (ImGui::MenuItem("Delete Key"))
			{
				ForEditableCurves([&](FFloatCurve& Curve)
				{
					if (CurveEditorState.SelectedKeyIndex >= 0 && CurveEditorState.SelectedKeyIndex < static_cast<int32>(Curve.Keys.size()))
					{
						Curve.Keys.erase(Curve.Keys.begin() + CurveEditorState.SelectedKeyIndex);
						Curve.AutoSetTangents();
					}
				});
				CurveEditorState.SelectedKeyIndex = -1;
				bChanged = true;
			}
			ImGui::Separator();
			if (CurveEditorState.SelectedKeyIndex >= 0 && CurveEditorState.SelectedKeyIndex < static_cast<int32>(ActiveCurve->Keys.size()))
			{
				if (ImGui::MenuItem("Constant", nullptr, ActiveCurve->Keys[CurveEditorState.SelectedKeyIndex].InterpMode == ECurveInterpMode::Constant))
				{
					ForEditableCurves([&](FFloatCurve& Curve)
					{
						if (CurveEditorState.SelectedKeyIndex >= 0 && CurveEditorState.SelectedKeyIndex < static_cast<int32>(Curve.Keys.size()))
						{
							Curve.Keys[CurveEditorState.SelectedKeyIndex].InterpMode = ECurveInterpMode::Constant;
						}
					});
					bChanged = true;
				}
				if (ImGui::MenuItem("Linear", nullptr, ActiveCurve->Keys[CurveEditorState.SelectedKeyIndex].InterpMode == ECurveInterpMode::Linear))
				{
					ForEditableCurves([&](FFloatCurve& Curve)
					{
						if (CurveEditorState.SelectedKeyIndex >= 0 && CurveEditorState.SelectedKeyIndex < static_cast<int32>(Curve.Keys.size()))
						{
							Curve.Keys[CurveEditorState.SelectedKeyIndex].InterpMode = ECurveInterpMode::Linear;
						}
					});
					bChanged = true;
				}
				if (ImGui::MenuItem("Cubic", nullptr, ActiveCurve->Keys[CurveEditorState.SelectedKeyIndex].InterpMode == ECurveInterpMode::Cubic))
				{
					ForEditableCurves([&](FFloatCurve& Curve)
					{
						if (CurveEditorState.SelectedKeyIndex >= 0 && CurveEditorState.SelectedKeyIndex < static_cast<int32>(Curve.Keys.size()))
						{
							Curve.Keys[CurveEditorState.SelectedKeyIndex].InterpMode = ECurveInterpMode::Cubic;
							Curve.Keys[CurveEditorState.SelectedKeyIndex].TangentMode = ECurveTangentMode::Auto;
							Curve.AutoSetTangents();
						}
					});
					bChanged = true;
				}
			}
		}
		ImGui::EndPopup();
	}

	if (ImGui::BeginPopup("ParticleCurveCanvasContext"))
	{
		if (ImGui::MenuItem("Add Key"))
		{
			FCurveKey NewKey{};
			NewKey.Time = CurveEditorState.PendingContextTime;
			NewKey.Value = CurveEditorState.PendingContextValue;
			NewKey.InterpMode = ECurveInterpMode::Linear;
			ForEditableCurves([&](FFloatCurve& Curve)
			{
				Curve.Keys.push_back(NewKey);
				Curve.SortKeys();
				Curve.AutoSetTangents();
			});
			for (int32 KeyIndex = 0; KeyIndex < static_cast<int32>(ActiveCurve->Keys.size()); ++KeyIndex)
			{
				if (std::fabs(ActiveCurve->Keys[KeyIndex].Time - NewKey.Time) <= CurveTimeEpsilon)
				{
					CurveEditorState.SelectedKeyIndex = KeyIndex;
					break;
				}
			}
			bChanged = true;
		}
		if (ImGui::MenuItem("Fit To Keys"))
		{
			FitCurveViewToSelectedCurves();
		}
		ImGui::EndPopup();
	}

	return bChanged;
}

void FParticleSystemEditorWidget::RenderCurveEditorPanel(const ImVec2& Size)
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::BeginChild("##ParticleCurveEditorPanel", Size, ImGuiChildFlags_Borders);
	ImGui::PopStyleVar();

	if (GetSelectedCurveCount() <= 0)
	{
		const ImVec2 CanvasSize = ImGui::GetContentRegionAvail();
		const ImVec2 CanvasMin = ImGui::GetCursorScreenPos();
		const ImVec2 CanvasMax(CanvasMin.x + CanvasSize.x, CanvasMin.y + CanvasSize.y);
		ImGui::InvisibleButton("##ParticleCurveEditorCanvasEmpty", CanvasSize);
		ImDrawList* DrawList = ImGui::GetWindowDrawList();
		DrawList->AddRectFilled(CanvasMin, CanvasMax, IM_COL32(48, 48, 48, 255));
		for (int32 i = 0; i <= 10; ++i)
		{
			const float X = CanvasMin.x + CanvasSize.x * (static_cast<float>(i) / 10.0f);
			DrawList->AddLine(ImVec2(X, CanvasMin.y), ImVec2(X, CanvasMax.y), IM_COL32(126, 126, 126, 150));
		}
		for (int32 i = 0; i <= 6; ++i)
		{
			const float Y = CanvasMin.y + CanvasSize.y * (static_cast<float>(i) / 6.0f);
			DrawList->AddLine(ImVec2(CanvasMin.x, Y), ImVec2(CanvasMax.x, Y), IM_COL32(126, 126, 126, 150));
		}
		DrawList->AddText(ImVec2(CanvasMin.x + 10.0f, CanvasMin.y + 8.0f), IM_COL32(225, 230, 235, 255), "Click a module curve button or a distribution curve icon.");
		ImGui::EndChild();
		return;
	}

	const ImVec2 ContentSize = ImGui::GetContentRegionAvail();
	const float TrackListWidth = (std::min)(200.0f, (std::max)(120.0f, ContentSize.x * 0.35f));
	int32 RemoveCurveIndex = -1;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::BeginChild("##ParticleCurveTrackList", ImVec2(TrackListWidth, 0.0f), false);
	ImGui::PopStyleVar();
	{
		const ImVec2 ListMin = ImGui::GetWindowPos();
		const ImVec2 ListMax(ListMin.x + ImGui::GetWindowSize().x, ListMin.y + ImGui::GetWindowSize().y);
		ImDrawList* DrawList = ImGui::GetWindowDrawList();
		DrawList->AddRectFilled(ListMin, ListMax, IM_COL32(150, 150, 150, 255));
		DrawList->AddLine(ImVec2(ListMax.x - 1.0f, ListMin.y), ImVec2(ListMax.x - 1.0f, ListMax.y), IM_COL32(16, 16, 16, 255), 1.0f);
	}

	auto DrawTrackRow = [&](const char* RowId, const char* Label, int32 CurveIndex)
	{
		ImGui::PushID(RowId);
		const ImVec2 RowMin = ImGui::GetCursorScreenPos();
		const float RowWidth = ImGui::GetContentRegionAvail().x;
		const float RowHeight = 36.0f;
		ImGui::InvisibleButton("##CurveTrackRow", ImVec2(RowWidth, RowHeight));
		const bool bHovered = ImGui::IsItemHovered();
		const bool bActive = CurveSelection.ActiveCurveIndex == CurveIndex;
		if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
		{
			CurveSelection.ActiveCurveIndex = CurveIndex;
			CurveEditorState.SelectedKeyIndex = -1;
		}
		if (ImGui::BeginPopupContextItem("##CurveTrackContext"))
		{
			if (ImGui::MenuItem("Remove Curve"))
			{
				RemoveCurveIndex = CurveIndex;
			}
			ImGui::EndPopup();
		}

		ImDrawList* DrawList = ImGui::GetWindowDrawList();
		const ImVec2 RowMax(RowMin.x + RowWidth, RowMin.y + RowHeight);
		const ImU32 RowColor =
			bActive ? IM_COL32(168, 168, 168, 255) :
			bHovered ? IM_COL32(162, 162, 162, 255) :
			IM_COL32(150, 150, 150, 255);
		DrawList->AddRectFilled(RowMin, RowMax, RowColor);
		DrawList->AddLine(ImVec2(RowMin.x, RowMax.y - 1.0f), ImVec2(RowMax.x, RowMax.y - 1.0f), IM_COL32(38, 38, 38, 210), 1.0f);

		const FString LabelString = Label ? Label : "";
		const bool bVectorLike = LabelString.find(".X") != FString::npos
			|| LabelString.find(".Y") != FString::npos
			|| LabelString.find(".Z") != FString::npos
			|| LabelString.find("Color") != FString::npos;
		if (bVectorLike)
		{
			const ImU32 SwatchColors[] =
			{
				IM_COL32(255, 28, 28, 255),
				IM_COL32(0, 235, 76, 255),
				IM_COL32(48, 88, 255, 255),
			};
			for (int32 SwatchIndex = 0; SwatchIndex < 3; ++SwatchIndex)
			{
				const ImVec2 SwatchMin(RowMin.x + 8.0f + static_cast<float>(SwatchIndex) * 10.0f, RowMin.y + RowHeight - 12.0f);
				DrawList->AddRectFilled(SwatchMin, ImVec2(SwatchMin.x + 7.0f, SwatchMin.y + 7.0f), SwatchColors[SwatchIndex], 1.0f);
				DrawList->AddRect(SwatchMin, ImVec2(SwatchMin.x + 7.0f, SwatchMin.y + 7.0f), IM_COL32(20, 20, 20, 240), 1.0f);
			}
		}
		else
		{
			const ImVec2 SwatchMin(RowMin.x + 8.0f, RowMin.y + RowHeight - 12.0f);
			DrawList->AddRectFilled(SwatchMin, ImVec2(SwatchMin.x + 7.0f, SwatchMin.y + 7.0f), GetCurveTrackColor(CurveIndex), 1.0f);
			DrawList->AddRect(SwatchMin, ImVec2(SwatchMin.x + 7.0f, SwatchMin.y + 7.0f), IM_COL32(20, 20, 20, 240), 1.0f);
		}

		const ImVec2 VisibilityMin(RowMax.x - 11.0f, RowMin.y + RowHeight - 13.0f);
		DrawList->AddRectFilled(VisibilityMin, ImVec2(VisibilityMin.x + 7.0f, VisibilityMin.y + 7.0f), IM_COL32(94, 94, 94, 255), 1.0f);
		DrawList->AddRect(VisibilityMin, ImVec2(VisibilityMin.x + 7.0f, VisibilityMin.y + 7.0f), IM_COL32(12, 12, 12, 255), 1.0f);
		DrawList->AddText(ImVec2(RowMin.x + 8.0f, RowMin.y + 4.0f), IM_COL32(4, 4, 4, 255), Label);
		ImGui::PopID();
	};

	for (int32 CurveIndex = 0; CurveIndex < GetSelectedCurveCount(); ++CurveIndex)
	{
		const FString& Label = CurveSelection.CurveLabels[CurveIndex];
		char RowId[32] = {};
		std::snprintf(RowId, sizeof(RowId), "Curve%d", CurveIndex);
		DrawTrackRow(RowId, Label.empty() ? "Curve" : Label.c_str(), CurveIndex);
	}
	ImGui::EndChild();

	if (RemoveCurveIndex >= 0)
	{
		RemoveSelectedCurve(RemoveCurveIndex);
	}

	ImGui::SameLine(0.0f, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::BeginChild("##ParticleCurveGraphArea", ImVec2(0.0f, 0.0f), false);
	ImGui::PopStyleVar();
	if (RenderSelectedCurveEditor(ImGui::GetContentRegionAvail()))
	{
		if (CurveSelection.OwnerObject)
		{
			CurveSelection.OwnerObject->PostEditProperty(nullptr);
		}
		MarkDirty();
		RefreshParticleSystemComponents();
	}
	ImGui::EndChild();

	ImGui::EndChild();
}
