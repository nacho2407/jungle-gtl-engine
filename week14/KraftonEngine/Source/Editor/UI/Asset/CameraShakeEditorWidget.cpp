#include "CameraShakeEditorWidget.h"

#include "Asset/AssetPackage.h"
#include "CameraShake/CameraShakeAsset.h"
#include "CameraShake/CameraShakeManager.h"
#include "Editor/EditorEngine.h"
#include "Editor/UI/ContentBrowser/ContentItem.h"
#include "FloatCurve/FloatCurveAsset.h"
#include "FloatCurve/FloatCurveManager.h"
#include "Object/Object.h"
#include "Platform/Paths.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <imgui.h>

namespace
{
	constexpr float TwoPi = 6.28318530717958f;

	bool InputFString(const char* Label, FString& Value)
	{
		char Buffer[512];
		std::snprintf(Buffer, sizeof(Buffer), "%s", Value.c_str());
		if (ImGui::InputText(Label, Buffer, sizeof(Buffer)))
		{
			Value = Buffer;
			return true;
		}
		return false;
	}

	bool AcceptFloatCurveDrop(FString& OutPath)
	{
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload("FloatCurveContentItem"))
			{
				const FContentItem* Item = static_cast<const FContentItem*>(Payload->Data);
				FString PackagePath = FPaths::ToUtf8(Item->Path.lexically_relative(FPaths::RootDir()).generic_wstring());

				EAssetPackageType Type = EAssetPackageType::Unknown;
				if (FAssetPackage::GetPackageType(PackagePath, Type) && Type == EAssetPackageType::FloatCurve)
				{
					OutPath = PackagePath;
					return true;
				}
			}
			ImGui::EndDragDropTarget();
		}
		return false;
	}

	void OpenCurveEditor(UEditorEngine* EditorEngine, const FString& Path)
	{
		if (!EditorEngine || Path.empty())
		{
			return;
		}

		if (UFloatCurveAsset* CurveAsset = FFloatCurveManager::Get().Load(Path))
		{
			EditorEngine->OpenAssetEditorForObject(CurveAsset);
		}
	}

	bool CurvePathField(const char* Label, FString& Path, UEditorEngine* EditorEngine)
	{
		bool bChanged = InputFString(Label, Path);
		if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
		{
			OpenCurveEditor(EditorEngine, Path);
		}
		if (AcceptFloatCurveDrop(Path))
		{
			bChanged = true;
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Double-click to open curve. Drop a .curve asset here.");
		}
		return bChanged;
	}

	bool DragVector(const char* Label, FVector& Value)
	{
		float Values[3] = { Value.X, Value.Y, Value.Z };
		if (ImGui::DragFloat3(Label, Values, 0.01f))
		{
			Value = FVector(Values[0], Values[1], Values[2]);
			return true;
		}
		return false;
	}

	bool DragRotator(const char* Label, FRotator& Value)
	{
		float Values[3] = { Value.Pitch, Value.Yaw, Value.Roll };
		if (ImGui::DragFloat3(Label, Values, 0.01f))
		{
			Value = FRotator(Values[0], Values[1], Values[2]);
			return true;
		}
		return false;
	}

	float EvalCurvePath(const FString& Path, float Time)
	{
		if (Path.empty())
		{
			return 0.0f;
		}

		UFloatCurveAsset* CurveAsset = FFloatCurveManager::Get().Load(Path);
		return CurveAsset ? CurveAsset->GetCurve().Evaluate(Time) : 0.0f;
	}

	float EvalWave(float Time, float Amplitude, float Frequency)
	{
		return std::sin(Time * Frequency * TwoPi) * Amplitude;
	}

	struct FPreviewSample
	{
		float Location = 0.0f;
		float Rotation = 0.0f;
		float FOV = 0.0f;
	};

	struct FPreviewPose
	{
		FVector Location = FVector(0.0f, 0.0f, 0.0f);
		FRotator Rotation = FRotator(0.0f, 0.0f, 0.0f);
		float FOV = 0.0f;
	};

	FPreviewPose EvaluatePreviewPose(const UCameraShakeAsset* Asset, float Time)
	{
		FPreviewPose Pose;
		if (!Asset)
		{
			return Pose;
		}

		if (Asset->ShakeType == ECameraShakeType::Sequence)
		{
			Pose.Location = FVector(
				EvalCurvePath(Asset->Sequence.LocXCurvePath, Time),
				EvalCurvePath(Asset->Sequence.LocYCurvePath, Time),
				EvalCurvePath(Asset->Sequence.LocZCurvePath, Time));
			Pose.Rotation = FRotator(
				EvalCurvePath(Asset->Sequence.PitchCurvePath, Time),
				EvalCurvePath(Asset->Sequence.YawCurvePath, Time),
				EvalCurvePath(Asset->Sequence.RollCurvePath, Time));
			Pose.FOV = EvalCurvePath(Asset->Sequence.FOVCurvePath, Time);
		}
		else if (Asset->ShakeType == ECameraShakeType::WaveOscillator)
		{
			const FWaveOscillatorCameraShakeAssetData& Wave = Asset->WaveOscillator;
			Pose.Location = FVector(
				EvalWave(Time, Wave.LocationAmplitude.X, Wave.LocationFrequency.X),
				EvalWave(Time, Wave.LocationAmplitude.Y, Wave.LocationFrequency.Y),
				EvalWave(Time, Wave.LocationAmplitude.Z, Wave.LocationFrequency.Z));
			Pose.Rotation = FRotator(
				EvalWave(Time, Wave.RotationAmplitude.Pitch, Wave.RotationFrequency.Pitch),
				EvalWave(Time, Wave.RotationAmplitude.Yaw, Wave.RotationFrequency.Yaw),
				EvalWave(Time, Wave.RotationAmplitude.Roll, Wave.RotationFrequency.Roll));
			Pose.FOV = EvalWave(Time, Wave.FOVAmplitude, Wave.FOVFrequency);
		}

		return Pose;
	}

	FPreviewSample EvaluatePreviewSample(const UCameraShakeAsset* Asset, float Time)
	{
		FPreviewSample Sample;
		if (!Asset)
		{
			return Sample;
		}

		if (Asset->ShakeType == ECameraShakeType::Sequence)
		{
			Sample.Location =
				EvalCurvePath(Asset->Sequence.LocXCurvePath, Time) +
				EvalCurvePath(Asset->Sequence.LocYCurvePath, Time) +
				EvalCurvePath(Asset->Sequence.LocZCurvePath, Time);
			Sample.Rotation =
				EvalCurvePath(Asset->Sequence.PitchCurvePath, Time) +
				EvalCurvePath(Asset->Sequence.YawCurvePath, Time) +
				EvalCurvePath(Asset->Sequence.RollCurvePath, Time);
			Sample.FOV = EvalCurvePath(Asset->Sequence.FOVCurvePath, Time);
		}
		else if (Asset->ShakeType == ECameraShakeType::WaveOscillator)
		{
			const FWaveOscillatorCameraShakeAssetData& Wave = Asset->WaveOscillator;
			Sample.Location =
				EvalWave(Time, Wave.LocationAmplitude.X, Wave.LocationFrequency.X) +
				EvalWave(Time, Wave.LocationAmplitude.Y, Wave.LocationFrequency.Y) +
				EvalWave(Time, Wave.LocationAmplitude.Z, Wave.LocationFrequency.Z);
			Sample.Rotation =
				EvalWave(Time, Wave.RotationAmplitude.Pitch, Wave.RotationFrequency.Pitch) +
				EvalWave(Time, Wave.RotationAmplitude.Yaw, Wave.RotationFrequency.Yaw) +
				EvalWave(Time, Wave.RotationAmplitude.Roll, Wave.RotationFrequency.Roll);
			Sample.FOV = EvalWave(Time, Wave.FOVAmplitude, Wave.FOVFrequency);
		}

		return Sample;
	}

	void DrawPreviewGraph(const UCameraShakeAsset* Asset)
	{
		const ImVec2 CanvasSize(ImGui::GetContentRegionAvail().x, 180.0f);
		const ImVec2 CanvasMin = ImGui::GetCursorScreenPos();
		const ImVec2 CanvasMax(CanvasMin.x + CanvasSize.x, CanvasMin.y + CanvasSize.y);
		ImGui::InvisibleButton("##CameraShakePreview", CanvasSize);

		ImDrawList* DrawList = ImGui::GetWindowDrawList();
		DrawList->AddRectFilled(CanvasMin, CanvasMax, IM_COL32(24, 24, 28, 255), 4.0f);
		DrawList->AddRect(CanvasMin, CanvasMax, IM_COL32(90, 90, 100, 255), 4.0f);

		const int32 SampleCount = 96;
		const float Duration = Asset ? (std::max)(Asset->Duration, 0.001f) : 1.0f;
		TArray<FPreviewSample> Samples;
		Samples.reserve(SampleCount);

		float MaxAbs = 0.001f;
		for (int32 i = 0; i < SampleCount; ++i)
		{
			const float Alpha = static_cast<float>(i) / static_cast<float>(SampleCount - 1);
			const FPreviewSample Sample = EvaluatePreviewSample(Asset, Alpha * Duration);
			Samples.push_back(Sample);
			MaxAbs = (std::max)(MaxAbs, std::fabs(Sample.Location));
			MaxAbs = (std::max)(MaxAbs, std::fabs(Sample.Rotation));
			MaxAbs = (std::max)(MaxAbs, std::fabs(Sample.FOV));
		}

		for (int32 GridIndex = 1; GridIndex < 4; ++GridIndex)
		{
			const float T = static_cast<float>(GridIndex) / 4.0f;
			DrawList->AddLine(
				ImVec2(CanvasMin.x + CanvasSize.x * T, CanvasMin.y),
				ImVec2(CanvasMin.x + CanvasSize.x * T, CanvasMax.y),
				IM_COL32(55, 55, 64, 255));
		}
		const float MidY = (CanvasMin.y + CanvasMax.y) * 0.5f;
		DrawList->AddLine(ImVec2(CanvasMin.x, MidY), ImVec2(CanvasMax.x, MidY), IM_COL32(75, 75, 84, 255));

		auto ToPoint = [&](int32 Index, float Value)
		{
			const float Alpha = static_cast<float>(Index) / static_cast<float>(SampleCount - 1);
			const float Normalized = Value / MaxAbs;
			return ImVec2(
				CanvasMin.x + Alpha * CanvasSize.x,
				MidY - Normalized * CanvasSize.y * 0.45f);
		};

		auto DrawChannel = [&](auto ValueGetter, ImU32 Color)
		{
			for (int32 i = 1; i < SampleCount; ++i)
			{
				DrawList->AddLine(
					ToPoint(i - 1, ValueGetter(Samples[i - 1])),
					ToPoint(i, ValueGetter(Samples[i])),
					Color,
					2.0f);
			}
		};

		DrawChannel([](const FPreviewSample& Sample) { return Sample.Location; }, IM_COL32(88, 170, 255, 255));
		DrawChannel([](const FPreviewSample& Sample) { return Sample.Rotation; }, IM_COL32(255, 185, 80, 255));
		DrawChannel([](const FPreviewSample& Sample) { return Sample.FOV; }, IM_COL32(120, 220, 120, 255));

		ImGui::TextColored(ImVec4(0.35f, 0.67f, 1.0f, 1.0f), "Location");
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(1.0f, 0.73f, 0.31f, 1.0f), "Rotation");
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(0.47f, 0.86f, 0.47f, 1.0f), "FOV");
	}

	void DrawCameraPreview(const UCameraShakeAsset* Asset, float PreviewTime)
	{
		const ImVec2 CanvasSize(ImGui::GetContentRegionAvail().x, 220.0f);
		const ImVec2 CanvasMin = ImGui::GetCursorScreenPos();
		const ImVec2 CanvasMax(CanvasMin.x + CanvasSize.x, CanvasMin.y + CanvasSize.y);
		ImGui::InvisibleButton("##CameraShakeViewportPreview", CanvasSize);

		ImDrawList* DrawList = ImGui::GetWindowDrawList();
		DrawList->AddRectFilled(CanvasMin, CanvasMax, IM_COL32(18, 19, 23, 255), 5.0f);
		DrawList->AddRect(CanvasMin, CanvasMax, IM_COL32(86, 90, 102, 255), 5.0f);

		const FPreviewPose Pose = EvaluatePreviewPose(Asset, PreviewTime);
		const ImVec2 Center((CanvasMin.x + CanvasMax.x) * 0.5f, (CanvasMin.y + CanvasMax.y) * 0.5f);
		const float OffsetScale = 8.0f;
		const ImVec2 ShakeOffset(Pose.Location.X * OffsetScale, -Pose.Location.Z * OffsetScale);

		float FovScale = 1.0f - Pose.FOV * 1.5f;
		FovScale = (std::max)(0.75f, (std::min)(1.25f, FovScale));

		const float Width = CanvasSize.x * 0.52f * FovScale;
		const float Height = CanvasSize.y * 0.48f * FovScale;
		const float RollRadians = Pose.Rotation.Roll * 0.01745329252f;
		const float CosR = std::cos(RollRadians);
		const float SinR = std::sin(RollRadians);
		const ImVec2 FrameCenter(Center.x + ShakeOffset.x, Center.y + ShakeOffset.y);

		auto RotatePoint = [&](float X, float Y)
		{
			return ImVec2(
				FrameCenter.x + X * CosR - Y * SinR,
				FrameCenter.y + X * SinR + Y * CosR);
		};

		const ImVec2 P0 = RotatePoint(-Width * 0.5f, -Height * 0.5f);
		const ImVec2 P1 = RotatePoint( Width * 0.5f, -Height * 0.5f);
		const ImVec2 P2 = RotatePoint( Width * 0.5f,  Height * 0.5f);
		const ImVec2 P3 = RotatePoint(-Width * 0.5f,  Height * 0.5f);

		DrawList->AddLine(ImVec2(CanvasMin.x + CanvasSize.x * 0.5f, CanvasMin.y), ImVec2(CanvasMin.x + CanvasSize.x * 0.5f, CanvasMax.y), IM_COL32(40, 43, 50, 255));
		DrawList->AddLine(ImVec2(CanvasMin.x, CanvasMin.y + CanvasSize.y * 0.5f), ImVec2(CanvasMax.x, CanvasMin.y + CanvasSize.y * 0.5f), IM_COL32(40, 43, 50, 255));
		DrawList->AddQuadFilled(P0, P1, P2, P3, IM_COL32(44, 54, 68, 220));
		DrawList->AddQuad(P0, P1, P2, P3, IM_COL32(105, 190, 255, 255), 2.5f);
		DrawList->AddCircleFilled(FrameCenter, 3.5f, IM_COL32(255, 210, 90, 255));

		char Readout[256];
		std::snprintf(
			Readout,
			sizeof(Readout),
			"t %.2f | loc %.2f %.2f %.2f | rot %.2f %.2f %.2f | fov %.3f",
			PreviewTime,
			Pose.Location.X,
			Pose.Location.Y,
			Pose.Location.Z,
			Pose.Rotation.Pitch,
			Pose.Rotation.Yaw,
			Pose.Rotation.Roll,
			Pose.FOV);
		DrawList->AddText(ImVec2(CanvasMin.x + 10.0f, CanvasMax.y - 24.0f), IM_COL32(220, 224, 232, 255), Readout);
	}
}

bool FCameraShakeEditorWidget::CanEdit(UObject* Object) const
{
	return Object && Object->IsA<UCameraShakeAsset>();
}

void FCameraShakeEditorWidget::Open(UObject* Object)
{
	if (!CanEdit(Object))
	{
		return;
	}

	EditedObject = Object;
	bOpen = true;
	bPreviewPlaying = false;
	PreviewTime = 0.0f;
	ClearDirty();
}

void FCameraShakeEditorWidget::Render(float DeltaTime)
{
	if (!IsOpen() || !EditedObject)
	{
		return;
	}

	UCameraShakeAsset* ShakeAsset = static_cast<UCameraShakeAsset*>(EditedObject);
	const float PreviewDuration = (std::max)(ShakeAsset->Duration, 0.001f);
	if (bPreviewPlaying)
	{
		PreviewTime += DeltaTime;
		if (PreviewTime >= PreviewDuration)
		{
			if (bPreviewLoop)
			{
				PreviewTime = std::fmod(PreviewTime, PreviewDuration);
			}
			else
			{
				PreviewTime = PreviewDuration;
				bPreviewPlaying = false;
			}
		}
	}

	bool bWindowOpen = true;
	FString VisibleTitle = "Camera Shake Editor";
	if (!ShakeAsset->GetSourcePath().empty())
	{
		VisibleTitle += " - ";
		VisibleTitle += ShakeAsset->GetSourcePath();
	}
	if (IsDirty())
	{
		VisibleTitle += " *";
	}

	ImGui::SetNextWindowSize(ImVec2(560.0f, 520.0f), ImGuiCond_Once);

	FString WindowTitle = VisibleTitle + "###CameraShakeEditor";
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
		FCameraShakeManager::Get().Save(ShakeAsset);
		ClearDirty();
	}
	ImGui::SameLine();
	ImGui::TextDisabled("%s", ShakeAsset->GetSourcePath().empty() ? "Unsaved asset" : ShakeAsset->GetSourcePath().c_str());
	ImGui::Separator();

	bool bChanged = false;

	ImGui::TextUnformatted("Common");
	const char* ShakeTypeLabels[] = { "Sequence", "Wave Oscillator" };
	int32 CurrentShakeType = static_cast<int32>(ShakeAsset->ShakeType);
	if (ImGui::Combo("Shake Type", &CurrentShakeType, ShakeTypeLabels, IM_ARRAYSIZE(ShakeTypeLabels)))
	{
		ShakeAsset->ShakeType = static_cast<ECameraShakeType>(CurrentShakeType);
		bChanged = true;
	}

	if (ImGui::DragFloat("Duration", &ShakeAsset->Duration, 0.01f, 0.0f, 60.0f))
	{
		bChanged = true;
	}
	if (ImGui::DragFloat("Blend In Time", &ShakeAsset->BlendInTime, 0.01f, 0.0f, 60.0f))
	{
		bChanged = true;
	}
	if (ImGui::DragFloat("Blend Out Time", &ShakeAsset->BlendOutTime, 0.01f, 0.0f, 60.0f))
	{
		bChanged = true;
	}
	if (ImGui::Checkbox("Single Instance", &ShakeAsset->bSingleInstance))
	{
		bChanged = true;
	}

	ImGui::Separator();

	ImGui::TextUnformatted("Preview");
	if (ImGui::Button(bPreviewPlaying ? "Stop Preview" : "Play Preview"))
	{
		bPreviewPlaying = !bPreviewPlaying;
		if (bPreviewPlaying && PreviewTime >= PreviewDuration)
		{
			PreviewTime = 0.0f;
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Reset"))
	{
		bPreviewPlaying = false;
		PreviewTime = 0.0f;
	}
	ImGui::SameLine();
	ImGui::Checkbox("Loop", &bPreviewLoop);
	if (ImGui::SliderFloat("Time", &PreviewTime, 0.0f, PreviewDuration))
	{
		bPreviewPlaying = false;
	}
	DrawCameraPreview(ShakeAsset, PreviewTime);
	if (ImGui::CollapsingHeader("Debug Graph"))
	{
		DrawPreviewGraph(ShakeAsset);
	}
	ImGui::Separator();

	if (ShakeAsset->ShakeType == ECameraShakeType::Sequence)
	{
		ImGui::TextUnformatted("Sequence Curves");
		bChanged |= CurvePathField("Location X", ShakeAsset->Sequence.LocXCurvePath, EditorEngine);
		bChanged |= CurvePathField("Location Y", ShakeAsset->Sequence.LocYCurvePath, EditorEngine);
		bChanged |= CurvePathField("Location Z", ShakeAsset->Sequence.LocZCurvePath, EditorEngine);
		bChanged |= CurvePathField("Pitch", ShakeAsset->Sequence.PitchCurvePath, EditorEngine);
		bChanged |= CurvePathField("Yaw", ShakeAsset->Sequence.YawCurvePath, EditorEngine);
		bChanged |= CurvePathField("Roll", ShakeAsset->Sequence.RollCurvePath, EditorEngine);
		bChanged |= CurvePathField("FOV", ShakeAsset->Sequence.FOVCurvePath, EditorEngine);
	}
	else if (ShakeAsset->ShakeType == ECameraShakeType::WaveOscillator)
	{
		ImGui::TextUnformatted("Wave Oscillator");
		bChanged |= DragVector("Location Amplitude", ShakeAsset->WaveOscillator.LocationAmplitude);
		bChanged |= DragVector("Location Frequency", ShakeAsset->WaveOscillator.LocationFrequency);
		bChanged |= DragRotator("Rotation Amplitude", ShakeAsset->WaveOscillator.RotationAmplitude);
		bChanged |= DragRotator("Rotation Frequency", ShakeAsset->WaveOscillator.RotationFrequency);
		if (ImGui::DragFloat("FOV Amplitude", &ShakeAsset->WaveOscillator.FOVAmplitude, 0.001f))
		{
			bChanged = true;
		}
		if (ImGui::DragFloat("FOV Frequency", &ShakeAsset->WaveOscillator.FOVFrequency, 0.01f))
		{
			bChanged = true;
		}
	}

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
