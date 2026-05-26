#include "ParticleEditorInternal.h"

#include <functional>

using namespace ParticleEditorInternal;

namespace
{
	enum class EParticleCurveAssetKind
	{
		Float,
		Vector,
		Color,
	};

	struct FParticleCurveBinding
	{
		FString Label;
		FString Path;
		EParticleCurveAssetKind Kind = EParticleCurveAssetKind::Float;
		void* SoftPtr = nullptr;
		UObject* Owner = nullptr;
		const char* NotifyPropertyName = nullptr;
		UObject* Asset = nullptr;
		FFloatCurve* Curve = nullptr;
	};

	FString SanitizeCurveAssetName(FString Name)
	{
		if (Name.empty())
		{
			Name = "Curve";
		}
		for (char& Ch : Name)
		{
			const bool bValid =
				(Ch >= 'a' && Ch <= 'z') ||
				(Ch >= 'A' && Ch <= 'Z') ||
				(Ch >= '0' && Ch <= '9') ||
				Ch == '_' || Ch == '-';
			if (!bValid)
			{
				Ch = '_';
			}
		}
		return Name;
	}

	FString GetBaseFileNameWithoutExtension(const FString& Path, const FString& Fallback)
	{
		if (Path.empty())
		{
			return Fallback;
		}

		std::filesystem::path FsPath(FPaths::ToWide(FPaths::Normalize(Path)));
		FString Stem = FPaths::ToUtf8(FsPath.stem().wstring());
		return Stem.empty() ? Fallback : Stem;
	}

	FString GetParticleCurveParticleName(const FParticleEditorViewer* Viewer)
	{
		if (Viewer)
		{
			const FString FileName = FPaths::Normalize(Viewer->GetFileName());
			if (!FileName.empty())
			{
				return SanitizeCurveAssetName(GetBaseFileNameWithoutExtension(FileName, "Particle"));
			}

			if (const UParticleSystem* ParticleSystem = Viewer->GetParticleSystem())
			{
				return SanitizeCurveAssetName(GetBaseFileNameWithoutExtension(ParticleSystem->GetAssetPath(), "Particle"));
			}
		}
		return "Particle";
	}

	FString GetParticleCurveModuleName(const UObject* Module)
	{
		FString ModuleName = Module ? Module->GetClassName() : FString("ParticleModule");
		constexpr const char* ParticleModulePrefix = "UParticleModule";
		if (ModuleName.rfind(ParticleModulePrefix, 0) == 0 && ModuleName.size() > std::strlen(ParticleModulePrefix))
		{
			ModuleName = ModuleName.substr(std::strlen(ParticleModulePrefix));
		}
		else if (!ModuleName.empty() && ModuleName[0] == 'U' && ModuleName.size() > 1)
		{
			ModuleName = ModuleName.substr(1);
		}
		return SanitizeCurveAssetName(ModuleName.empty() ? FString("Module") : ModuleName);
	}

	FString MakeNewParticleCurvePath(const FParticleEditorViewer* Viewer, const UObject* Module, const FString& SlotLabel)
	{
		const FString ParticleName = GetParticleCurveParticleName(Viewer);
		const FString ModuleName = GetParticleCurveModuleName(Module);
		const FString CurveName = SanitizeCurveAssetName(SlotLabel.empty() ? FString("Curve") : SlotLabel);
		const FString BaseName = ParticleName + "_" + ModuleName + "_" + CurveName;
		FString Candidate = "Assets/Curves/Particle/" + BaseName + ".curve";
		int32 Suffix = 1;
		while (std::filesystem::exists(FPaths::ToWide(Candidate)))
		{
			Candidate = "Assets/Curves/Particle/" + BaseName + "_" + std::to_string(Suffix++) + ".curve";
		}
		return Candidate;
	}

	void AddDefaultKeys(FFloatCurve& Curve, float StartValue = 0.0f, float EndValue = 1.0f)
	{
		Curve.Keys.clear();
		FCurveKey A;
		A.Time = 0.0f;
		A.Value = StartValue;
		A.InterpMode = ECurveInterpMode::Cubic;
		A.TangentMode = ECurveTangentMode::Auto;
		FCurveKey B;
		B.Time = 1.0f;
		B.Value = EndValue;
		B.InterpMode = ECurveInterpMode::Cubic;
		B.TangentMode = ECurveTangentMode::Auto;
		Curve.Keys.push_back(A);
		Curve.Keys.push_back(B);
		Curve.SortKeys();
	}

	void SetBindingPath(FParticleCurveBinding& Binding, const FString& Path)
	{
		switch (Binding.Kind)
		{
		case EParticleCurveAssetKind::Float:
			static_cast<TSoftObjectPtr<UCurveFloatAsset>*>(Binding.SoftPtr)->SetPath(Path);
			break;
		case EParticleCurveAssetKind::Vector:
			static_cast<TSoftObjectPtr<UCurveVectorAsset>*>(Binding.SoftPtr)->SetPath(Path);
			break;
		case EParticleCurveAssetKind::Color:
			static_cast<TSoftObjectPtr<UCurveColorAsset>*>(Binding.SoftPtr)->SetPath(Path);
			break;
		}
		Binding.Path = Path;
	}

	void SaveBindingCurve(const FParticleCurveBinding& Binding)
	{
		if (!Binding.Asset || Binding.Path.empty())
		{
			return;
		}

		switch (Binding.Kind)
		{
		case EParticleCurveAssetKind::Float:
			FResourceManager::Get().SaveCurve(Binding.Path, Cast<UCurveFloatAsset>(Binding.Asset));
			break;
		case EParticleCurveAssetKind::Vector:
			FResourceManager::Get().SaveCurve(Binding.Path, Cast<UCurveVectorAsset>(Binding.Asset));
			break;
		case EParticleCurveAssetKind::Color:
			FResourceManager::Get().SaveCurve(Binding.Path, Cast<UCurveColorAsset>(Binding.Asset));
			break;
		}
	}

	void NotifyCurveEdited(FParticleEditorViewer* Viewer, const FParticleCurveBinding& Binding)
	{
		SaveBindingCurve(Binding);
		if (Binding.Owner && Binding.NotifyPropertyName)
		{
			Binding.Owner->PostEditProperty(Binding.NotifyPropertyName);
		}
		if (Viewer)
		{
			if (UParticleEmitter* Emitter = Viewer->GetSelectedEmitter())
			{
				Emitter->CacheEmitterModuleInfo();
			}
			Viewer->MarkDirty();
			Viewer->RestartSimulation();
		}
	}

	void AddFloatBinding(
		TArray<FParticleCurveBinding>& OutBindings,
		const FString& Label,
		TSoftObjectPtr<UCurveFloatAsset>& SoftCurve,
		UObject* Owner,
		const char* NotifyPropertyName)
	{
		FParticleCurveBinding Binding;
		Binding.Label = Label;
		Binding.Path = SoftCurve.GetPath();
		Binding.Kind = EParticleCurveAssetKind::Float;
		Binding.SoftPtr = &SoftCurve;
		Binding.Owner = Owner;
		Binding.NotifyPropertyName = NotifyPropertyName;
		if (!Binding.Path.empty())
		{
			if (UCurveFloatAsset* Asset = FResourceManager::Get().LoadFloatCurve(Binding.Path))
			{
				Binding.Asset = Asset;
				Binding.Curve = &Asset->GetMutableCurve();
			}
		}
		OutBindings.push_back(Binding);
	}

	void AddVectorChannelBinding(
		TArray<FParticleCurveBinding>& OutBindings,
		const FString& Label,
		TSoftObjectPtr<UCurveVectorAsset>& SoftCurve,
		UObject* Owner,
		const char* NotifyPropertyName,
		char Channel,
		bool bUniformXYZ)
	{
		FParticleCurveBinding Binding;
		Binding.Label = bUniformXYZ ? Label + ".XYZ" : Label + "." + FString(1, Channel);
		Binding.Path = SoftCurve.GetPath();
		Binding.Kind = EParticleCurveAssetKind::Vector;
		Binding.SoftPtr = &SoftCurve;
		Binding.Owner = Owner;
		Binding.NotifyPropertyName = NotifyPropertyName;
		if (!Binding.Path.empty())
		{
			if (UCurveVectorAsset* Asset = FResourceManager::Get().LoadVectorCurve(Binding.Path))
			{
				Binding.Asset = Asset;
				FVectorCurve& Curve = Asset->GetMutableCurve();
				switch (Channel)
				{
				case 'X': Binding.Curve = &Curve.XCurve; break;
				case 'Y': Binding.Curve = &Curve.YCurve; break;
				case 'Z': Binding.Curve = &Curve.ZCurve; break;
				default: break;
				}
			}
		}
		OutBindings.push_back(Binding);
	}

	void AddVectorBinding(
		TArray<FParticleCurveBinding>& OutBindings,
		const FString& Label,
		TSoftObjectPtr<UCurveVectorAsset>& SoftCurve,
		UObject* Owner,
		const char* NotifyPropertyName)
	{
		FParticleCurveBinding Binding;
		Binding.Label = Label;
		Binding.Path = SoftCurve.GetPath();
		Binding.Kind = EParticleCurveAssetKind::Vector;
		Binding.SoftPtr = &SoftCurve;
		Binding.Owner = Owner;
		Binding.NotifyPropertyName = NotifyPropertyName;
		if (!Binding.Path.empty())
		{
			if (UCurveVectorAsset* Asset = FResourceManager::Get().LoadVectorCurve(Binding.Path))
			{
				Binding.Asset = Asset;
				Binding.Curve = &Asset->GetMutableCurve().XCurve;
			}
		}
		OutBindings.push_back(Binding);
	}

	void AddColorChannelBinding(
		TArray<FParticleCurveBinding>& OutBindings,
		const FString& Label,
		TSoftObjectPtr<UCurveColorAsset>& SoftCurve,
		UObject* Owner,
		const char* NotifyPropertyName,
		char Channel)
	{
		FParticleCurveBinding Binding;
		Binding.Label = Label + "." + FString(1, Channel);
		Binding.Path = SoftCurve.GetPath();
		Binding.Kind = EParticleCurveAssetKind::Color;
		Binding.SoftPtr = &SoftCurve;
		Binding.Owner = Owner;
		Binding.NotifyPropertyName = NotifyPropertyName;
		if (!Binding.Path.empty())
		{
			if (UCurveColorAsset* Asset = FResourceManager::Get().LoadColorCurve(Binding.Path))
			{
				Binding.Asset = Asset;
				FColorCurve& Curve = Asset->GetMutableCurve();
				switch (Channel)
				{
				case 'R': Binding.Curve = &Curve.RCurve; break;
				case 'G': Binding.Curve = &Curve.GCurve; break;
				case 'B': Binding.Curve = &Curve.BCurve; break;
				case 'A': Binding.Curve = &Curve.ACurve; break;
				default: break;
				}
			}
		}
		OutBindings.push_back(Binding);
	}

	void AddColorBinding(
		TArray<FParticleCurveBinding>& OutBindings,
		const FString& Label,
		TSoftObjectPtr<UCurveColorAsset>& SoftCurve,
		UObject* Owner,
		const char* NotifyPropertyName)
	{
		FParticleCurveBinding Binding;
		Binding.Label = Label;
		Binding.Path = SoftCurve.GetPath();
		Binding.Kind = EParticleCurveAssetKind::Color;
		Binding.SoftPtr = &SoftCurve;
		Binding.Owner = Owner;
		Binding.NotifyPropertyName = NotifyPropertyName;
		if (!Binding.Path.empty())
		{
			if (UCurveColorAsset* Asset = FResourceManager::Get().LoadColorCurve(Binding.Path))
			{
				Binding.Asset = Asset;
				Binding.Curve = &Asset->GetMutableCurve().RCurve;
			}
		}
		OutBindings.push_back(Binding);
	}

	void CollectDistributionCurveBindings(UObject* Module, TArray<FParticleCurveBinding>& OutBindings)
	{
		OutBindings.clear();
		if (!Module || !Module->GetClass())
		{
			return;
		}

		TArray<const FProperty*> Properties;
		Module->GetClass()->GetAllProperties(Properties);
		for (const FProperty* Property : Properties)
		{
			if (!Property || Property->Type != EPropertyType::Struct || !Property->ScriptStruct || !Property->Name)
			{
				continue;
			}

			void* ValuePtr = Property->GetValuePtr(Module);
			if (!ValuePtr)
			{
				continue;
			}

			const FString BaseLabel = GetPropertyDisplayName(*Property);
			const char* StructName = Property->ScriptStruct->GetName();
			if (std::strcmp(StructName, "FParticleFloatDistribution") == 0)
			{
				FParticleFloatDistribution& Distribution = *static_cast<FParticleFloatDistribution*>(ValuePtr);
				if (Distribution.Mode == EParticleDistributionMode::Curve)
				{
					AddFloatBinding(OutBindings, BaseLabel + ".Curve", Distribution.Curve, Module, Property->Name);
				}
				else if (Distribution.Mode == EParticleDistributionMode::RandomRangeCurve)
				{
					AddFloatBinding(OutBindings, BaseLabel + ".MinCurve", Distribution.MinCurve, Module, Property->Name);
					AddFloatBinding(OutBindings, BaseLabel + ".MaxCurve", Distribution.MaxCurve, Module, Property->Name);
				}
			}
			else if (std::strcmp(StructName, "FParticleVectorDistribution") == 0)
			{
				FParticleVectorDistribution& Distribution = *static_cast<FParticleVectorDistribution*>(ValuePtr);
				auto AddVectorSlots = [&](const FString& Label, TSoftObjectPtr<UCurveVectorAsset>& SoftCurve)
				{
					AddVectorBinding(OutBindings, Label, SoftCurve, Module, Property->Name);
				};
				if (Distribution.Mode == EParticleDistributionMode::Curve)
				{
					AddVectorSlots(BaseLabel + ".Curve", Distribution.Curve);
				}
				else if (Distribution.Mode == EParticleDistributionMode::RandomRangeCurve)
				{
					AddVectorSlots(BaseLabel + ".MinCurve", Distribution.MinCurve);
					AddVectorSlots(BaseLabel + ".MaxCurve", Distribution.MaxCurve);
				}
			}
			else if (std::strcmp(StructName, "FParticleColorDistribution") == 0)
			{
				FParticleColorDistribution& Distribution = *static_cast<FParticleColorDistribution*>(ValuePtr);
				auto AddColorSlots = [&](const FString& Label, TSoftObjectPtr<UCurveColorAsset>& SoftCurve)
				{
					AddColorBinding(OutBindings, Label, SoftCurve, Module, Property->Name);
				};
				if (Distribution.Mode == EParticleDistributionMode::Curve)
				{
					AddColorSlots(BaseLabel + ".Curve", Distribution.Curve);
				}
				else if (Distribution.Mode == EParticleDistributionMode::RandomRangeCurve)
				{
					AddColorSlots(BaseLabel + ".MinCurve", Distribution.MinCurve);
					AddColorSlots(BaseLabel + ".MaxCurve", Distribution.MaxCurve);
				}
			}
		}
	}

	bool CreateCurveForBinding(FParticleEditorViewer* Viewer, FParticleCurveBinding& Binding)
	{
		if (!Binding.SoftPtr)
		{
			return false;
		}

		const FString NewPath = MakeNewParticleCurvePath(Viewer, Binding.Owner, Binding.Label);
		switch (Binding.Kind)
		{
		case EParticleCurveAssetKind::Float:
		{
			UCurveFloatAsset* Asset = UObjectManager::Get().CreateObject<UCurveFloatAsset>();
			Asset->SetAssetPath(NewPath);
			AddDefaultKeys(Asset->GetMutableCurve());
			FResourceManager::Get().SaveCurve(NewPath, Asset);
			Binding.Asset = Asset;
			Binding.Curve = &Asset->GetMutableCurve();
			break;
		}
		case EParticleCurveAssetKind::Vector:
		{
			UCurveVectorAsset* Asset = UObjectManager::Get().CreateObject<UCurveVectorAsset>();
			Asset->SetAssetPath(NewPath);
			FVectorCurve& Curve = Asset->GetMutableCurve();
			AddDefaultKeys(Curve.XCurve);
			AddDefaultKeys(Curve.YCurve);
			AddDefaultKeys(Curve.ZCurve);
			FResourceManager::Get().SaveCurve(NewPath, Asset);
			Binding.Asset = Asset;
			if (Binding.Label.find(".Y") != FString::npos)
			{
				Binding.Curve = &Curve.YCurve;
			}
			else if (Binding.Label.find(".Z") != FString::npos)
			{
				Binding.Curve = &Curve.ZCurve;
			}
			else
			{
				Binding.Curve = &Curve.XCurve;
			}
			break;
		}
		case EParticleCurveAssetKind::Color:
		{
			UCurveColorAsset* Asset = UObjectManager::Get().CreateObject<UCurveColorAsset>();
			Asset->SetAssetPath(NewPath);
			FColorCurve& Curve = Asset->GetMutableCurve();
			AddDefaultKeys(Curve.RCurve);
			AddDefaultKeys(Curve.GCurve);
			AddDefaultKeys(Curve.BCurve);
			AddDefaultKeys(Curve.ACurve, 1.0f, 1.0f);
			FResourceManager::Get().SaveCurve(NewPath, Asset);
			Binding.Asset = Asset;
			if (Binding.Label.find(".G") != FString::npos)
			{
				Binding.Curve = &Curve.GCurve;
			}
			else if (Binding.Label.find(".B") != FString::npos)
			{
				Binding.Curve = &Curve.BCurve;
			}
			else if (Binding.Label.find(".A") != FString::npos)
			{
				Binding.Curve = &Curve.ACurve;
			}
			else
			{
				Binding.Curve = &Curve.RCurve;
			}
			break;
		}
		}

		SetBindingPath(Binding, NewPath);
		NotifyCurveEdited(Viewer, Binding);
		return true;
	}

	bool DeleteCurveForBinding(FParticleEditorViewer* Viewer, FParticleCurveBinding& Binding)
	{
		if (!Binding.SoftPtr || Binding.Path.empty())
		{
			return false;
		}
		SetBindingPath(Binding, "");
		Binding.Asset = nullptr;
		Binding.Curve = nullptr;
		if (Binding.Owner && Binding.NotifyPropertyName)
		{
			Binding.Owner->PostEditProperty(Binding.NotifyPropertyName);
		}
		if (Viewer)
		{
			Viewer->MarkDirty();
			Viewer->RestartSimulation();
		}
		return true;
	}

	float ComputeAutoTangent(const FFloatCurve& Curve, int32 KeyIndex)
	{
		if (KeyIndex < 0 || KeyIndex >= static_cast<int32>(Curve.Keys.size()))
		{
			return 0.0f;
		}
		const int32 PrevIndex = KeyIndex - 1;
		const int32 NextIndex = KeyIndex + 1;
		if (PrevIndex >= 0 && NextIndex < static_cast<int32>(Curve.Keys.size()))
		{
			const float TimeDelta = Curve.Keys[NextIndex].Time - Curve.Keys[PrevIndex].Time;
			if (std::fabs(TimeDelta) > 0.0001f)
			{
				return (Curve.Keys[NextIndex].Value - Curve.Keys[PrevIndex].Value) / TimeDelta;
			}
		}
		const int32 NeighborIndex = NextIndex < static_cast<int32>(Curve.Keys.size()) ? NextIndex : PrevIndex;
		if (NeighborIndex >= 0 && NeighborIndex < static_cast<int32>(Curve.Keys.size()))
		{
			const float TimeDelta = Curve.Keys[NeighborIndex].Time - Curve.Keys[KeyIndex].Time;
			if (std::fabs(TimeDelta) > 0.0001f)
			{
				return (Curve.Keys[NeighborIndex].Value - Curve.Keys[KeyIndex].Value) / TimeDelta;
			}
		}
		return 0.0f;
	}

	float ComputeIncomingTangent(const FFloatCurve& Curve, int32 KeyIndex)
	{
		if (KeyIndex <= 0 || KeyIndex >= static_cast<int32>(Curve.Keys.size()))
		{
			return 0.0f;
		}

		const FCurveKey& PrevKey = Curve.Keys[KeyIndex - 1];
		const FCurveKey& Key = Curve.Keys[KeyIndex];
		const float TimeDelta = Key.Time - PrevKey.Time;
		if (std::fabs(TimeDelta) <= 0.0001f)
		{
			return 0.0f;
		}
		return (Key.Value - PrevKey.Value) / TimeDelta;
	}

	float ComputeOutgoingTangent(const FFloatCurve& Curve, int32 KeyIndex)
	{
		if (KeyIndex < 0 || KeyIndex + 1 >= static_cast<int32>(Curve.Keys.size()))
		{
			return 0.0f;
		}

		const FCurveKey& Key = Curve.Keys[KeyIndex];
		const FCurveKey& NextKey = Curve.Keys[KeyIndex + 1];
		const float TimeDelta = NextKey.Time - Key.Time;
		if (std::fabs(TimeDelta) <= 0.0001f)
		{
			return 0.0f;
		}
		return (NextKey.Value - Key.Value) / TimeDelta;
	}

	float ClampParticleCurveTangent(float Tangent)
	{
		return std::clamp(Tangent, -100.0f, 100.0f);
	}

	float ComputeClampedAutoTangent(const FFloatCurve& Curve, int32 KeyIndex)
	{
		float Tangent = ComputeAutoTangent(Curve, KeyIndex);
		if (KeyIndex > 0 && KeyIndex + 1 < static_cast<int32>(Curve.Keys.size()))
		{
			const float PrevDelta = Curve.Keys[KeyIndex].Value - Curve.Keys[KeyIndex - 1].Value;
			const float NextDelta = Curve.Keys[KeyIndex + 1].Value - Curve.Keys[KeyIndex].Value;
			if ((PrevDelta > 0.0f && NextDelta < 0.0f) || (PrevDelta < 0.0f && NextDelta > 0.0f))
			{
				Tangent = 0.0f;
			}
		}
		return ClampParticleCurveTangent(Tangent);
	}

	enum class EParticleCurveKeyCommand
	{
		Auto,
		AutoClamped,
		User,
		Break,
		Linear,
		Constant,
		Flatten,
		Straighten,
	};

	bool ApplyKeyCommand(FFloatCurve& Curve, int32 KeyIndex, EParticleCurveKeyCommand Command)
	{
		if (KeyIndex < 0 || KeyIndex >= static_cast<int32>(Curve.Keys.size()))
		{
			return false;
		}

		FCurveKey& Key = Curve.Keys[KeyIndex];
		const float AutoTangent = ClampParticleCurveTangent(ComputeAutoTangent(Curve, KeyIndex));
		const float ArriveTangent = ClampParticleCurveTangent(ComputeIncomingTangent(Curve, KeyIndex));
		const float LeaveTangent = ClampParticleCurveTangent(ComputeOutgoingTangent(Curve, KeyIndex));

		switch (Command)
		{
		case EParticleCurveKeyCommand::Auto:
			Key.InterpMode = ECurveInterpMode::Cubic;
			Key.TangentMode = ECurveTangentMode::Auto;
			Key.ArriveTangent = AutoTangent;
			Key.LeaveTangent = AutoTangent;
			break;
		case EParticleCurveKeyCommand::AutoClamped:
		{
			const float Tangent = ComputeClampedAutoTangent(Curve, KeyIndex);
			Key.InterpMode = ECurveInterpMode::Cubic;
			// FCurveKey has no dedicated Auto/Clamp enum, so store the clamped auto result as user tangents.
			Key.TangentMode = ECurveTangentMode::User;
			Key.ArriveTangent = Tangent;
			Key.LeaveTangent = Tangent;
			break;
		}
		case EParticleCurveKeyCommand::User:
			Key.InterpMode = ECurveInterpMode::Cubic;
			Key.TangentMode = ECurveTangentMode::User;
			Key.ArriveTangent = AutoTangent;
			Key.LeaveTangent = AutoTangent;
			break;
		case EParticleCurveKeyCommand::Break:
			Key.InterpMode = ECurveInterpMode::Cubic;
			Key.TangentMode = ECurveTangentMode::Break;
			Key.ArriveTangent = ArriveTangent;
			Key.LeaveTangent = LeaveTangent;
			break;
		case EParticleCurveKeyCommand::Linear:
			Key.InterpMode = ECurveInterpMode::Linear;
			Key.TangentMode = ECurveTangentMode::User;
			Key.ArriveTangent = ArriveTangent;
			Key.LeaveTangent = LeaveTangent;
			break;
		case EParticleCurveKeyCommand::Constant:
			Key.InterpMode = ECurveInterpMode::Constant;
			Key.TangentMode = ECurveTangentMode::User;
			Key.ArriveTangent = 0.0f;
			Key.LeaveTangent = 0.0f;
			break;
		case EParticleCurveKeyCommand::Flatten:
			Key.InterpMode = ECurveInterpMode::Cubic;
			Key.TangentMode = ECurveTangentMode::User;
			Key.ArriveTangent = 0.0f;
			Key.LeaveTangent = 0.0f;
			break;
		case EParticleCurveKeyCommand::Straighten:
			Key.InterpMode = ECurveInterpMode::Cubic;
			Key.TangentMode = ECurveTangentMode::User;
			Key.ArriveTangent = AutoTangent;
			Key.LeaveTangent = AutoTangent;
			break;
		}
		return true;
	}

	void FitParticleCurveView(FParticleCurveEditorState& State, const FFloatCurve* Curve, const ImVec2& GraphSize, bool bFitX, bool bFitY)
	{
		if (!Curve || Curve->Keys.empty())
		{
			if (bFitX)
			{
				State.CanvasPanTime = 0.0f;
				State.CanvasZoomX = 1.0f;
			}
			if (bFitY)
			{
				State.CanvasPanValue = 0.0f;
				State.CanvasZoomY = 1.0f;
			}
			return;
		}

		float MinTime = FLT_MAX;
		float MaxTime = -FLT_MAX;
		float MinValue = FLT_MAX;
		float MaxValue = -FLT_MAX;
		for (const FCurveKey& Key : Curve->Keys)
		{
			MinTime = std::min(MinTime, Key.Time);
			MaxTime = std::max(MaxTime, Key.Time);
			MinValue = std::min(MinValue, Key.Value);
			MaxValue = std::max(MaxValue, Key.Value);
		}

		constexpr float BasePixelsPerTime = 43.0f;
		constexpr float BasePixelsPerValue = 252.0f;
		constexpr float BaseFirstTime = -13.5f;
		constexpr float BaseValueMax = 0.5f;
		if (bFitX)
		{
			const float Range = std::max(0.1f, MaxTime - MinTime);
			const float Padding = Range * 0.12f + 0.05f;
			const float VisibleRange = Range + Padding * 2.0f;
			State.CanvasZoomX = std::clamp(GraphSize.x / std::max(1.0f, VisibleRange * BasePixelsPerTime), 0.25f, 8.0f);
			const float FirstTime = MinTime - Padding;
			State.CanvasPanTime = FirstTime - BaseFirstTime;
		}
		if (bFitY)
		{
			const float Range = std::max(0.1f, MaxValue - MinValue);
			const float Padding = Range * 0.18f + 0.05f;
			const float VisibleRange = Range + Padding * 2.0f;
			State.CanvasZoomY = std::clamp(GraphSize.y / std::max(1.0f, VisibleRange * BasePixelsPerValue), 0.20f, 64.0f);
			const float ValueMax = MaxValue + Padding;
			State.CanvasPanValue = ValueMax - BaseValueMax;
		}
	}

	bool IsParticleCurveBindingVisibleWithActive(const FParticleCurveBinding& Binding, const FParticleCurveBinding* ActiveBinding)
	{
		if (!Binding.Asset && (!Binding.Curve || Binding.Curve->Keys.empty()))
		{
			return false;
		}
		if (!ActiveBinding)
		{
			return true;
		}
		if (!ActiveBinding->Path.empty())
		{
			return Binding.Kind == ActiveBinding->Kind && Binding.Path == ActiveBinding->Path;
		}
		return &Binding == ActiveBinding;
	}

	char GetDefaultParticleCurveChannel(const FParticleCurveBinding& Binding)
	{
		switch (Binding.Kind)
		{
		case EParticleCurveAssetKind::Vector:
			return 'X';
		case EParticleCurveAssetKind::Color:
			return 'R';
		case EParticleCurveAssetKind::Float:
		default:
			return '\0';
		}
	}

	bool IsValidParticleCurveChannel(const FParticleCurveBinding& Binding, char Channel)
	{
		switch (Binding.Kind)
		{
		case EParticleCurveAssetKind::Float:
			return Channel == '\0';
		case EParticleCurveAssetKind::Vector:
			return Channel == 'X' || Channel == 'Y' || Channel == 'Z';
		case EParticleCurveAssetKind::Color:
			return Channel == 'R' || Channel == 'G' || Channel == 'B' || Channel == 'A';
		}
		return false;
	}

	char NormalizeParticleCurveChannel(const FParticleCurveBinding& Binding, char Channel)
	{
		return IsValidParticleCurveChannel(Binding, Channel) ? Channel : GetDefaultParticleCurveChannel(Binding);
	}

	FFloatCurve* GetParticleCurveChannelCurve(const FParticleCurveBinding& Binding, char Channel)
	{
		const char NormalizedChannel = NormalizeParticleCurveChannel(Binding, Channel);
		switch (Binding.Kind)
		{
		case EParticleCurveAssetKind::Float:
			return Binding.Curve;
		case EParticleCurveAssetKind::Vector:
			if (UCurveVectorAsset* Asset = Cast<UCurveVectorAsset>(Binding.Asset))
			{
				FVectorCurve& Curve = Asset->GetMutableCurve();
				switch (NormalizedChannel)
				{
				case 'Y': return &Curve.YCurve;
				case 'Z': return &Curve.ZCurve;
				case 'X':
				default: return &Curve.XCurve;
				}
			}
			break;
		case EParticleCurveAssetKind::Color:
			if (UCurveColorAsset* Asset = Cast<UCurveColorAsset>(Binding.Asset))
			{
				FColorCurve& Curve = Asset->GetMutableCurve();
				switch (NormalizedChannel)
				{
				case 'G': return &Curve.GCurve;
				case 'B': return &Curve.BCurve;
				case 'A': return &Curve.ACurve;
				case 'R':
				default: return &Curve.RCurve;
				}
			}
			break;
		}
		return nullptr;
	}

	FString GetParticleCurveChannelLabel(const FParticleCurveBinding& Binding, char Channel)
	{
		const char NormalizedChannel = NormalizeParticleCurveChannel(Binding, Channel);
		if (NormalizedChannel == '\0')
		{
			return Binding.Label;
		}
		return Binding.Label + "." + FString(1, NormalizedChannel);
	}

	FString GetParticleCurveBindingDisplayLabel(const FParticleCurveBinding& Binding)
	{
		return Binding.Label;
	}

	void ForEachParticleCurveChannel(
		const FParticleCurveBinding& Binding,
		const std::function<void(const FString& ChannelLabel, FFloatCurve* Curve)>& Func)
	{
		if (!Func)
		{
			return;
		}

		switch (Binding.Kind)
		{
		case EParticleCurveAssetKind::Float:
			Func(Binding.Label, Binding.Curve);
			break;
		case EParticleCurveAssetKind::Vector:
			Func(GetParticleCurveChannelLabel(Binding, 'X'), GetParticleCurveChannelCurve(Binding, 'X'));
			Func(GetParticleCurveChannelLabel(Binding, 'Y'), GetParticleCurveChannelCurve(Binding, 'Y'));
			Func(GetParticleCurveChannelLabel(Binding, 'Z'), GetParticleCurveChannelCurve(Binding, 'Z'));
			break;
		case EParticleCurveAssetKind::Color:
			Func(GetParticleCurveChannelLabel(Binding, 'R'), GetParticleCurveChannelCurve(Binding, 'R'));
			Func(GetParticleCurveChannelLabel(Binding, 'G'), GetParticleCurveChannelCurve(Binding, 'G'));
			Func(GetParticleCurveChannelLabel(Binding, 'B'), GetParticleCurveChannelCurve(Binding, 'B'));
			Func(GetParticleCurveChannelLabel(Binding, 'A'), GetParticleCurveChannelCurve(Binding, 'A'));
			break;
		}
	}

	void FitParticleCurveView(FParticleCurveEditorState& State, const TArray<FParticleCurveBinding>& Bindings, const FParticleCurveBinding* ActiveBinding, const ImVec2& GraphSize, bool bFitX, bool bFitY)
	{
		float MinTime = FLT_MAX;
		float MaxTime = -FLT_MAX;
		float MinValue = FLT_MAX;
		float MaxValue = -FLT_MAX;
		bool bHasKeys = false;

		for (const FParticleCurveBinding& Binding : Bindings)
		{
			if (!IsParticleCurveBindingVisibleWithActive(Binding, ActiveBinding))
			{
				continue;
			}
			ForEachParticleCurveChannel(
				Binding,
				[&](const FString&, FFloatCurve* Curve)
				{
					if (!Curve || Curve->Keys.empty())
					{
						return;
					}
					for (const FCurveKey& Key : Curve->Keys)
					{
						MinTime = std::min(MinTime, Key.Time);
						MaxTime = std::max(MaxTime, Key.Time);
						MinValue = std::min(MinValue, Key.Value);
						MaxValue = std::max(MaxValue, Key.Value);
						bHasKeys = true;
					}
				});
		}

		if (!bHasKeys)
		{
			FitParticleCurveView(State, ActiveBinding ? ActiveBinding->Curve : nullptr, GraphSize, bFitX, bFitY);
			return;
		}

		constexpr float BasePixelsPerTime = 43.0f;
		constexpr float BasePixelsPerValue = 252.0f;
		constexpr float BaseFirstTime = -13.5f;
		constexpr float BaseValueMax = 0.5f;
		if (bFitX)
		{
			const float Range = std::max(0.1f, MaxTime - MinTime);
			const float Padding = Range * 0.12f + 0.05f;
			const float VisibleRange = Range + Padding * 2.0f;
			State.CanvasZoomX = std::clamp(GraphSize.x / std::max(1.0f, VisibleRange * BasePixelsPerTime), 0.25f, 8.0f);
			const float FirstTime = MinTime - Padding;
			State.CanvasPanTime = FirstTime - BaseFirstTime;
		}
		if (bFitY)
		{
			const float Range = std::max(0.1f, MaxValue - MinValue);
			const float Padding = Range * 0.18f + 0.05f;
			const float VisibleRange = Range + Padding * 2.0f;
			State.CanvasZoomY = std::clamp(GraphSize.y / std::max(1.0f, VisibleRange * BasePixelsPerValue), 0.20f, 64.0f);
			const float ValueMax = MaxValue + Padding;
			State.CanvasPanValue = ValueMax - BaseValueMax;
		}
	}

	bool ParticleCurveLabelHasSuffix(const FString& Label, const char* Suffix)
	{
		const size_t LabelLen = Label.size();
		const size_t SuffixLen = std::strlen(Suffix);
		return LabelLen >= SuffixLen && Label.compare(LabelLen - SuffixLen, SuffixLen, Suffix) == 0;
	}

	ImU32 GetParticleCurveDisplayColor(const FString& Label, bool bActive)
	{
		const int32 Alpha = bActive ? 255 : 150;
		if (ParticleCurveLabelHasSuffix(Label, ".R") || ParticleCurveLabelHasSuffix(Label, ".X"))
		{
			return IM_COL32(255, 88, 88, Alpha);
		}
		if (ParticleCurveLabelHasSuffix(Label, ".G") || ParticleCurveLabelHasSuffix(Label, ".Y"))
		{
			return IM_COL32(92, 230, 120, Alpha);
		}
		if (ParticleCurveLabelHasSuffix(Label, ".B") || ParticleCurveLabelHasSuffix(Label, ".Z"))
		{
			return IM_COL32(98, 156, 255, Alpha);
		}
		if (ParticleCurveLabelHasSuffix(Label, ".A"))
		{
			return IM_COL32(238, 238, 238, Alpha);
		}
		return IM_COL32(255, 206, 64, Alpha);
	}

	int32 FindNearestKey(const FFloatCurve& Curve, const ImVec2& Mouse, const ImVec2& GraphStart, float FirstTime, float ValueMax, float PixelsPerTime, float PixelsPerValue)
	{
		int32 BestIndex = -1;
		float BestDistanceSq = 64.0f;
		for (int32 Index = 0; Index < static_cast<int32>(Curve.Keys.size()); ++Index)
		{
			const FCurveKey& Key = Curve.Keys[Index];
			const float X = GraphStart.x + (Key.Time - FirstTime) * PixelsPerTime;
			const float Y = GraphStart.y + (ValueMax - Key.Value) * PixelsPerValue;
			const float Dx = Mouse.x - X;
			const float Dy = Mouse.y - Y;
			const float DistSq = Dx * Dx + Dy * Dy;
			if (DistSq < BestDistanceSq)
			{
				BestDistanceSq = DistSq;
				BestIndex = Index;
			}
		}
		return BestIndex;
	}

	int32 FindClosestKeyByValue(const FFloatCurve& Curve, float Time, float Value)
	{
		int32 BestIndex = -1;
		float BestScore = FLT_MAX;
		for (int32 Index = 0; Index < static_cast<int32>(Curve.Keys.size()); ++Index)
		{
			const FCurveKey& Key = Curve.Keys[Index];
			const float Score = std::fabs(Key.Time - Time) + std::fabs(Key.Value - Value);
			if (Score < BestScore)
			{
				BestScore = Score;
				BestIndex = Index;
			}
		}
		return BestIndex;
	}

	bool AddKeyToCurve(FFloatCurve& Curve, float Time, float Value, int32& OutIndex)
	{
		FCurveKey Key;
		Key.Time = std::max(0.0f, Time);
		Key.Value = Value;
		Key.InterpMode = ECurveInterpMode::Cubic;
		Key.TangentMode = ECurveTangentMode::Auto;
		Curve.Keys.push_back(Key);
		Curve.SortKeys();
		OutIndex = FindClosestKeyByValue(Curve, Key.Time, Key.Value);
		return true;
	}

	bool MoveKeyInCurve(FFloatCurve& Curve, int32 KeyIndex, float Time, float Value, int32& OutNewIndex)
	{
		if (KeyIndex < 0 || KeyIndex >= static_cast<int32>(Curve.Keys.size()))
		{
			return false;
		}
		Curve.Keys[KeyIndex].Time = std::max(0.0f, Time);
		Curve.Keys[KeyIndex].Value = Value;
		const float NewTime = Curve.Keys[KeyIndex].Time;
		const float NewValue = Curve.Keys[KeyIndex].Value;
		Curve.SortKeys();
		OutNewIndex = FindClosestKeyByValue(Curve, NewTime, NewValue);
		return true;
	}
}

void FParticleEditorViewerWidget::RenderCurveEditor(FParticleEditorViewer* Viewer)
{
	LoadCascadeToolbarIcons();
	CurveState.bWantsDeleteKeyFocus = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) ||
		ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);

	UParticleModule* Module = ResolveParticleModule(
		Viewer,
		CurveState.Type,
		CurveState.EmitterIndex,
		CurveState.LODIndex,
		CurveState.ModuleIndex);

	TArray<FParticleCurveBinding> CurveBindings;
	CollectDistributionCurveBindings(Module, CurveBindings);
	if (CurveBindings.empty())
	{
		CurveState.SelectedCurveIndex = 0;
		CurveState.SelectedCurveChannel = '\0';
		CurveState.SelectedKeyIndex = -1;
		CurveState.ActiveKeyIndex = -1;
	}
	else
	{
		CurveState.SelectedCurveIndex = std::clamp(CurveState.SelectedCurveIndex, 0, static_cast<int32>(CurveBindings.size()) - 1);
		CurveState.SelectedCurveChannel = NormalizeParticleCurveChannel(CurveBindings[CurveState.SelectedCurveIndex], CurveState.SelectedCurveChannel);
	}

	FParticleCurveBinding* ActiveBinding = CurveBindings.empty() ? nullptr : &CurveBindings[CurveState.SelectedCurveIndex];
	FFloatCurve* ActiveCurve = ActiveBinding ? GetParticleCurveChannelCurve(*ActiveBinding, CurveState.SelectedCurveChannel) : nullptr;
	const bool bHasCurveSlots = !CurveBindings.empty();
	const bool bHasEditableCurve = ActiveCurve != nullptr;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4.0f, 4.0f));
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.13f, 0.13f, 0.13f, 1.0f));
	ImGui::BeginChild(
		"ParticleCurveToolbar",
		ImVec2(0.0f, 66.0f),
		false,
		ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

	const auto RunKeyCommand = [&](EParticleCurveKeyCommand Command)
	{
		if (!ActiveBinding || !ActiveCurve || CurveState.SelectedKeyIndex < 0)
		{
			return;
		}
		if (Viewer)
			{
				Viewer->CaptureUndoSnapshot("EditParticleCurveKey");
			}
		if (ApplyKeyCommand(*ActiveCurve, CurveState.SelectedKeyIndex, Command))
		{
			NotifyCurveEdited(Viewer, *ActiveBinding);
		}
	};

	constexpr float CurveToolbarItemGap = 2.0f;
	if (DrawParticleCurveToolbarButton("HorizontalFit", ToolbarIcons.CurveHorizontalIcon.Get(), "Horizontal", false, bHasCurveSlots))
	{
		CurveState.bPendingHorizontalFit = true;
	}
	ImGui::SameLine(0.0f, CurveToolbarItemGap);
	if (DrawParticleCurveToolbarButton("VerticalFit", ToolbarIcons.CurveVerticalIcon.Get(), "Vertical", false, bHasCurveSlots))
	{
		CurveState.bPendingVerticalFit = true;
	}
	ImGui::SameLine(0.0f, CurveToolbarItemGap);
	if (DrawParticleCurveToolbarButton("FitCurve", ToolbarIcons.CurveFitIcon.Get(), "Fit", false, bHasCurveSlots))
	{
		CurveState.bPendingHorizontalFit = true;
		CurveState.bPendingVerticalFit = true;
	}
	ImGui::SameLine(0.0f, CurveToolbarItemGap);
	if (DrawParticleCurveToolbarButton("PanCurve", ToolbarIcons.CurvePanIcon.Get(), "Pan", CurveState.ActiveTool == EParticleCurveEditorTool::Pan, true))
	{
		CurveState.ActiveTool = EParticleCurveEditorTool::Pan;
	}
	ImGui::SameLine(0.0f, CurveToolbarItemGap);
	if (DrawParticleCurveToolbarButton("ZoomCurve", ToolbarIcons.CurveZoomIcon.Get(), "Zoom", CurveState.ActiveTool == EParticleCurveEditorTool::Zoom, true))
	{
		CurveState.ActiveTool = EParticleCurveEditorTool::Zoom;
	}
	ImGui::SameLine(0.0f, CurveToolbarItemGap);
	DrawParticleCurveToolbarSeparator("CurveEditTangents");
	ImGui::SameLine(0.0f, CurveToolbarItemGap);
	if (DrawParticleCurveToolbarButton("AutoCurve", ToolbarIcons.CurveAutoIcon.Get(), "Auto", false, bHasEditableCurve))
	{
		RunKeyCommand(EParticleCurveKeyCommand::Auto);
	}
	ImGui::SameLine(0.0f, CurveToolbarItemGap);
	if (DrawParticleCurveToolbarButton("AutoClampedCurve", ToolbarIcons.CurveAutoClampedIcon.Get(), "Auto/Clamp", false, bHasEditableCurve))
	{
		RunKeyCommand(EParticleCurveKeyCommand::AutoClamped);
	}
	ImGui::SameLine(0.0f, CurveToolbarItemGap);
	if (DrawParticleCurveToolbarButton("UserCurve", ToolbarIcons.CurveUserIcon.Get(), "User", false, bHasEditableCurve))
	{
		RunKeyCommand(EParticleCurveKeyCommand::User);
	}
	ImGui::SameLine(0.0f, CurveToolbarItemGap);
	if (DrawParticleCurveToolbarButton("BreakCurve", ToolbarIcons.CurveBreakIcon.Get(), "Break", false, bHasEditableCurve))
	{
		RunKeyCommand(EParticleCurveKeyCommand::Break);
	}
	ImGui::SameLine(0.0f, CurveToolbarItemGap);
	if (DrawParticleCurveToolbarButton("LinearCurve", ToolbarIcons.CurveLinearIcon.Get(), "Linear", false, bHasEditableCurve))
	{
		RunKeyCommand(EParticleCurveKeyCommand::Linear);
	}
	ImGui::SameLine(0.0f, CurveToolbarItemGap);
	if (DrawParticleCurveToolbarButton("ConstantCurve", ToolbarIcons.CurveConstantIcon.Get(), "Constant", false, bHasEditableCurve))
	{
		RunKeyCommand(EParticleCurveKeyCommand::Constant);
	}
	ImGui::SameLine(0.0f, CurveToolbarItemGap);
	DrawParticleCurveToolbarSeparator("CurveEditInterpolation");
	ImGui::SameLine(0.0f, CurveToolbarItemGap);
	if (DrawParticleCurveToolbarButton("FlattenCurve", ToolbarIcons.CurveFlattenIcon.Get(), "Flatten", false, bHasEditableCurve))
	{
		RunKeyCommand(EParticleCurveKeyCommand::Flatten);
	}
	ImGui::SameLine(0.0f, CurveToolbarItemGap);
	if (DrawParticleCurveToolbarButton("StraightenCurve", ToolbarIcons.CurveStraightenIcon.Get(), "Straighten", false, bHasEditableCurve))
	{
		RunKeyCommand(EParticleCurveKeyCommand::Straighten);
	}
	ImGui::SameLine(0.0f, CurveToolbarItemGap);
	if (DrawParticleCurveToolbarButton("ShowAllCurve", ToolbarIcons.CurveShowAllIcon.Get(), "Show All", false, bHasCurveSlots))
	{
		CurveState.bPendingHorizontalFit = true;
		CurveState.bPendingVerticalFit = true;
	}
	ImGui::SameLine(0.0f, CurveToolbarItemGap);
	DrawParticleCurveToolbarSeparator("CurveEditDisplay");
	ImGui::SameLine(0.0f, CurveToolbarItemGap);
	if (DrawParticleCurveToolbarButton("CreateCurve", ToolbarIcons.CurveCreateIcon.Get(), "Create", false, ActiveBinding != nullptr && !bHasEditableCurve))
	{
		if (Viewer)
			{
				Viewer->CaptureUndoSnapshot("CreateParticleCurveAsset");
			}
		CreateCurveForBinding(Viewer, *ActiveBinding);
		CurveState.bPendingHorizontalFit = true;
		CurveState.bPendingVerticalFit = true;
	}
	ImGui::SameLine(0.0f, CurveToolbarItemGap);
	if (DrawParticleCurveToolbarButton("DeleteCurve", ToolbarIcons.CurveDeleteIcon.Get(), "Delete", false, ActiveBinding != nullptr && !ActiveBinding->Path.empty()))
	{
		if (Viewer)
			{
				Viewer->CaptureUndoSnapshot("DeleteParticleCurveReference");
			}
		DeleteCurveForBinding(Viewer, *ActiveBinding);
		CurveState.SelectedKeyIndex = -1;
		CurveState.ActiveKeyIndex = -1;
	}

	ImGui::SameLine(0.0f, 16.0f);
	ImGui::BeginGroup();
	ImGui::TextDisabled("Current Tab:");
	ImGui::SetNextItemWidth(220.0f);
	const char* PreviewLabel = ActiveBinding ? ActiveBinding->Label.c_str() : "No Curve";
	if (ImGui::BeginCombo("##ParticleCurveCurrentTab", PreviewLabel))
	{
		for (int32 Index = 0; Index < static_cast<int32>(CurveBindings.size()); ++Index)
		{
			const bool bSelected = Index == CurveState.SelectedCurveIndex;
			FString Label = CurveBindings[Index].Label;
			if (CurveBindings[Index].Path.empty())
			{
				Label += " (empty)";
			}
			if (ImGui::Selectable(Label.c_str(), bSelected))
			{
				CurveState.SelectedCurveIndex = Index;
				CurveState.SelectedCurveChannel = GetDefaultParticleCurveChannel(CurveBindings[Index]);
				CurveState.SelectedKeyIndex = -1;
				CurveState.ActiveKeyIndex = -1;
				CurveState.bPendingHorizontalFit = true;
				CurveState.bPendingVerticalFit = true;
			}
			if (bSelected)
			{
				ImGui::SetItemDefaultFocus();
			}
		}
		if (CurveBindings.empty())
		{
			ImGui::TextDisabled("Selected module has no distribution curves.");
		}
		ImGui::EndCombo();
	}
	ImGui::EndGroup();

	ImGui::EndChild();
	ImGui::PopStyleColor();
	ImGui::PopStyleVar();

	ActiveBinding = CurveBindings.empty() ? nullptr : &CurveBindings[CurveState.SelectedCurveIndex];
	if (ActiveBinding)
	{
		CurveState.SelectedCurveChannel = NormalizeParticleCurveChannel(*ActiveBinding, CurveState.SelectedCurveChannel);
	}
	ActiveCurve = ActiveBinding ? GetParticleCurveChannelCurve(*ActiveBinding, CurveState.SelectedCurveChannel) : nullptr;

	const ImVec2 Available = ImGui::GetContentRegionAvail();
	if (Available.x <= 1.0f || Available.y <= 1.0f)
	{
		return;
	}

	const float ChannelWidth = std::min(224.0f, std::max(150.0f, Available.x * 0.25f));
	const ImVec2 CanvasStart = ImGui::GetCursorScreenPos();
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	const ImVec2 CanvasEnd(CanvasStart.x + Available.x, CanvasStart.y + Available.y);
	const ImVec2 ChannelEnd(CanvasStart.x + ChannelWidth, CanvasEnd.y);
	const ImVec2 GraphStart(ChannelEnd.x, CanvasStart.y);
	const ImVec2 GraphSize(std::max(1.0f, CanvasEnd.x - GraphStart.x), std::max(1.0f, CanvasEnd.y - GraphStart.y));

	if (CurveState.bPendingHorizontalFit || CurveState.bPendingVerticalFit)
	{
		FitParticleCurveView(CurveState, CurveBindings, ActiveBinding, GraphSize, CurveState.bPendingHorizontalFit, CurveState.bPendingVerticalFit);
		CurveState.bPendingHorizontalFit = false;
		CurveState.bPendingVerticalFit = false;
	}

	CurveState.CanvasZoomX = std::clamp(CurveState.CanvasZoomX, 0.25f, 8.0f);
	CurveState.CanvasZoomY = std::clamp(CurveState.CanvasZoomY, 0.20f, 64.0f);

	constexpr float BasePixelsPerTime = 43.0f;
	constexpr float BasePixelsPerValue = 252.0f;
	constexpr float BaseFirstTime = -13.5f;
	constexpr float BaseValueMax = 0.5f;
	constexpr float ValueMin = -0.6f;

	float PixelsPerTime = BasePixelsPerTime * CurveState.CanvasZoomX;
	float PixelsPerValue = BasePixelsPerValue * CurveState.CanvasZoomY;
	float FirstTime = BaseFirstTime + CurveState.CanvasPanTime;
	float ValueMax = BaseValueMax + CurveState.CanvasPanValue;

	ImGui::SetCursorScreenPos(GraphStart);
	ImGui::InvisibleButton(
		"##ParticleCurveEditorGraphCanvas",
		GraphSize,
		ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle | ImGuiButtonFlags_MouseButtonRight);
	const bool bGraphHovered = ImGui::IsItemHovered();
	const bool bGraphActive = ImGui::IsItemActive();
	const bool bGraphDragging =
		bGraphActive &&
		(ImGui::IsMouseDragging(ImGuiMouseButton_Left) ||
		 ImGui::IsMouseDragging(ImGuiMouseButton_Middle) ||
		 ImGui::IsMouseDragging(ImGuiMouseButton_Right));
	const ImGuiIO& IO = ImGui::GetIO();

	if (ActiveCurve && CurveState.SelectedKeyIndex >= static_cast<int32>(ActiveCurve->Keys.size()))
	{
		CurveState.SelectedKeyIndex = -1;
		CurveState.ActiveKeyIndex = -1;
	}

	const auto ScreenToTime = [&](float X)
	{
		return FirstTime + (X - GraphStart.x) / std::max(1.0f, PixelsPerTime);
	};
	const auto ScreenToValue = [&](float Y)
	{
		return ValueMax - (Y - GraphStart.y) / std::max(1.0f, PixelsPerValue);
	};

	if (bGraphHovered && ActiveCurve && !IO.WantTextInput)
	{
		const int32 HoveredKeyIndex = FindNearestKey(*ActiveCurve, IO.MousePos, GraphStart, FirstTime, ValueMax, PixelsPerTime, PixelsPerValue);
		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
		{
			if (IO.KeyCtrl && HoveredKeyIndex < 0)
			{
				if (Viewer)
					{
						Viewer->CaptureUndoSnapshot("AddParticleCurveKey");
					}
				int32 NewIndex = -1;
				AddKeyToCurve(*ActiveCurve, ScreenToTime(IO.MousePos.x), ScreenToValue(IO.MousePos.y), NewIndex);
				CurveState.SelectedKeyIndex = NewIndex;
				CurveState.ActiveKeyIndex = NewIndex;
				NotifyCurveEdited(Viewer, *ActiveBinding);
			}
			else if (HoveredKeyIndex >= 0)
			{
				CurveState.SelectedKeyIndex = HoveredKeyIndex;
				CurveState.ActiveKeyIndex = HoveredKeyIndex;
			}
			else
			{
				CurveState.SelectedKeyIndex = -1;
				CurveState.ActiveKeyIndex = -1;
			}
		}
	}

	if (ActiveCurve && CurveState.ActiveKeyIndex >= 0 && ImGui::IsMouseDown(ImGuiMouseButton_Left) && !IO.KeyCtrl)
	{
		if (ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f))
		{
			int32 NewIndex = CurveState.ActiveKeyIndex;
			if (MoveKeyInCurve(*ActiveCurve, CurveState.ActiveKeyIndex, ScreenToTime(IO.MousePos.x), ScreenToValue(IO.MousePos.y), NewIndex))
			{
				CurveState.ActiveKeyIndex = NewIndex;
				CurveState.SelectedKeyIndex = NewIndex;
				NotifyCurveEdited(Viewer, *ActiveBinding);
			}
		}
	}
	else if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
	{
		CurveState.ActiveKeyIndex = -1;
	}

	if (ActiveCurve && CurveState.SelectedKeyIndex >= 0 && !IO.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Delete, false))
	{
		if (Viewer)
			{
				Viewer->CaptureUndoSnapshot("DeleteParticleCurveKey");
			}
		ActiveCurve->Keys.erase(ActiveCurve->Keys.begin() + CurveState.SelectedKeyIndex);
		CurveState.SelectedKeyIndex = -1;
		CurveState.ActiveKeyIndex = -1;
		NotifyCurveEdited(Viewer, *ActiveBinding);
	}

	if (bGraphDragging && CurveState.ActiveKeyIndex < 0)
	{
		if (CurveState.ActiveTool == EParticleCurveEditorTool::Zoom)
		{
			const ImVec2 Mouse = IO.MousePos;
			const float AnchorTime = FirstTime + (Mouse.x - GraphStart.x) / std::max(1.0f, PixelsPerTime);
			const float AnchorValue = ValueMax - (Mouse.y - GraphStart.y) / std::max(1.0f, PixelsPerValue);
			const float ZoomFactorX = std::pow(1.01f, IO.MouseDelta.x);
			const float ZoomFactorY = std::pow(1.01f, -IO.MouseDelta.y);
			CurveState.CanvasZoomX = std::clamp(CurveState.CanvasZoomX * ZoomFactorX, 0.25f, 8.0f);
			CurveState.CanvasZoomY = std::clamp(CurveState.CanvasZoomY * ZoomFactorY, 0.20f, 64.0f);
			PixelsPerTime = BasePixelsPerTime * CurveState.CanvasZoomX;
			PixelsPerValue = BasePixelsPerValue * CurveState.CanvasZoomY;
			FirstTime = AnchorTime - (Mouse.x - GraphStart.x) / std::max(1.0f, PixelsPerTime);
			ValueMax = AnchorValue + (Mouse.y - GraphStart.y) / std::max(1.0f, PixelsPerValue);
			CurveState.CanvasPanTime = FirstTime - BaseFirstTime;
			CurveState.CanvasPanValue = ValueMax - BaseValueMax;
		}
		else
		{
			CurveState.CanvasPanTime -= IO.MouseDelta.x / std::max(1.0f, PixelsPerTime);
			CurveState.CanvasPanValue += IO.MouseDelta.y / std::max(1.0f, PixelsPerValue);
			FirstTime = BaseFirstTime + CurveState.CanvasPanTime;
			ValueMax = BaseValueMax + CurveState.CanvasPanValue;
		}
	}
	if (bGraphHovered && std::fabs(IO.MouseWheel) > 0.0f)
	{
		const ImVec2 Mouse = IO.MousePos;
		const float AnchorTime = FirstTime + (Mouse.x - GraphStart.x) / std::max(1.0f, PixelsPerTime);
		const float AnchorValue = ValueMax - (Mouse.y - GraphStart.y) / std::max(1.0f, PixelsPerValue);
		const float ZoomFactor = IO.MouseWheel > 0.0f ? 1.12f : 1.0f / 1.12f;

		if (IO.KeyShift)
		{
			CurveState.CanvasZoomX = std::clamp(CurveState.CanvasZoomX * ZoomFactor, 0.25f, 8.0f);
		}
		else if (IO.KeyCtrl)
		{
			CurveState.CanvasZoomY = std::clamp(CurveState.CanvasZoomY * ZoomFactor, 0.20f, 64.0f);
		}
		else
		{
			CurveState.CanvasZoomX = std::clamp(CurveState.CanvasZoomX * ZoomFactor, 0.25f, 8.0f);
			CurveState.CanvasZoomY = std::clamp(CurveState.CanvasZoomY * ZoomFactor, 0.20f, 64.0f);
		}

		PixelsPerTime = BasePixelsPerTime * CurveState.CanvasZoomX;
		PixelsPerValue = BasePixelsPerValue * CurveState.CanvasZoomY;
		FirstTime = AnchorTime - (Mouse.x - GraphStart.x) / std::max(1.0f, PixelsPerTime);
		ValueMax = AnchorValue + (Mouse.y - GraphStart.y) / std::max(1.0f, PixelsPerValue);
		CurveState.CanvasPanTime = FirstTime - BaseFirstTime;
		CurveState.CanvasPanValue = ValueMax - BaseValueMax;
	}

	DrawList->AddRectFilled(CanvasStart, ChannelEnd, IM_COL32(33, 34, 38, 255), 0.0f);
	DrawList->AddRectFilled(CanvasStart, ImVec2(ChannelEnd.x, CanvasStart.y + 32.0f), IM_COL32(42, 44, 51, 255), 0.0f);
	DrawList->AddRect(CanvasStart, ChannelEnd, IM_COL32(75, 75, 82, 255), 0.0f);
	DrawList->AddRectFilled(GraphStart, CanvasEnd, IM_COL32(48, 48, 48, 255), 0.0f);
	DrawList->AddLine(ImVec2(ChannelEnd.x, CanvasStart.y), ImVec2(ChannelEnd.x, CanvasEnd.y), IM_COL32(78, 80, 88, 255), 1.0f);

	const float HeaderHeight = 32.0f;
	DrawList->AddText(
		ImVec2(CanvasStart.x + 6.0f, CanvasStart.y + 7.0f),
		ActiveBinding ? IM_COL32(236, 236, 238, 255) : IM_COL32(166, 170, 180, 255),
		"Curve Source");

	const auto SelectCurveSource = [&](int32 BindingIndex, char Channel)
	{
		if (BindingIndex < 0 || BindingIndex >= static_cast<int32>(CurveBindings.size()))
		{
			return;
		}

		CurveState.SelectedCurveIndex = BindingIndex;
		CurveState.SelectedCurveChannel = NormalizeParticleCurveChannel(CurveBindings[BindingIndex], Channel);
		CurveState.SelectedKeyIndex = -1;
		CurveState.ActiveKeyIndex = -1;
		CurveState.bPendingHorizontalFit = true;
		CurveState.bPendingVerticalFit = true;
	};

	DrawList->PushClipRect(CanvasStart, ChannelEnd, true);
	float SourceRowY = CanvasStart.y + HeaderHeight + 4.0f;
	constexpr float SourceRowHeight = 20.0f;
	constexpr float SourceRowGap = 2.0f;

	ImGui::PushID("ParticleCurveSourceList");
	const auto DrawSourceRow = [&](int32 BindingIndex, char Channel, const FString& Label, float Indent, bool bChannelRow)
	{
		if (SourceRowY + SourceRowHeight > ChannelEnd.y - 2.0f)
		{
			return;
		}

		const FParticleCurveBinding& Binding = CurveBindings[BindingIndex];
		const char NormalizedChannel = NormalizeParticleCurveChannel(Binding, Channel);
		const bool bSelected = BindingIndex == CurveState.SelectedCurveIndex &&
			NormalizeParticleCurveChannel(Binding, CurveState.SelectedCurveChannel) == NormalizedChannel;
		const ImVec2 RowMin(CanvasStart.x + 4.0f, SourceRowY);
		const ImVec2 RowMax(ChannelEnd.x - 4.0f, SourceRowY + SourceRowHeight);

		ImGui::PushID(BindingIndex);
		ImGui::PushID(bChannelRow ? 1 : 0);
		ImGui::PushID(static_cast<int>(NormalizedChannel));
		ImGui::SetCursorScreenPos(RowMin);
		ImGui::InvisibleButton("##CurveSourceRow", ImVec2(std::max(1.0f, RowMax.x - RowMin.x), SourceRowHeight));
		const bool bHovered = ImGui::IsItemHovered();
		if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
		{
			SelectCurveSource(BindingIndex, NormalizedChannel);
		}
		ImGui::PopID();
		ImGui::PopID();
		ImGui::PopID();

		if (bSelected || bHovered)
		{
			DrawList->AddRectFilled(RowMin, RowMax, bSelected ? IM_COL32(58, 90, 130, 220) : IM_COL32(68, 70, 78, 150), 3.0f);
		}

		const FString ColorLabel = GetParticleCurveChannelLabel(Binding, NormalizedChannel);
		const ImU32 SwatchColor = GetParticleCurveDisplayColor(ColorLabel, bSelected);
		const float SwatchX = RowMin.x + 6.0f + Indent;
		const float SwatchY = RowMin.y + 6.0f;
		DrawList->AddRectFilled(ImVec2(SwatchX, SwatchY), ImVec2(SwatchX + 8.0f, SwatchY + 8.0f), SwatchColor, 2.0f);
		DrawList->AddRect(ImVec2(SwatchX, SwatchY), ImVec2(SwatchX + 8.0f, SwatchY + 8.0f), IM_COL32(0, 0, 0, 180), 2.0f);

		const bool bEmpty = Binding.Path.empty();
		const ImU32 TextColor = bSelected
			? IM_COL32(245, 248, 255, 255)
			: (bEmpty ? IM_COL32(145, 148, 158, 255) : IM_COL32(218, 220, 226, 255));
		FString RowText = Label;
		if (!bChannelRow && bEmpty)
		{
			RowText += " (empty)";
		}
		DrawList->AddText(ImVec2(SwatchX + 13.0f, RowMin.y + 3.0f), TextColor, RowText.c_str());

		SourceRowY += SourceRowHeight + SourceRowGap;
	};

	for (int32 BindingIndex = 0; BindingIndex < static_cast<int32>(CurveBindings.size()); ++BindingIndex)
	{
		const FParticleCurveBinding& Binding = CurveBindings[BindingIndex];
		DrawSourceRow(BindingIndex, GetDefaultParticleCurveChannel(Binding), GetParticleCurveBindingDisplayLabel(Binding), 0.0f, false);

		switch (Binding.Kind)
		{
		case EParticleCurveAssetKind::Vector:
			DrawSourceRow(BindingIndex, 'X', "X", 16.0f, true);
			DrawSourceRow(BindingIndex, 'Y', "Y", 16.0f, true);
			DrawSourceRow(BindingIndex, 'Z', "Z", 16.0f, true);
			break;
		case EParticleCurveAssetKind::Color:
			DrawSourceRow(BindingIndex, 'R', "R", 16.0f, true);
			DrawSourceRow(BindingIndex, 'G', "G", 16.0f, true);
			DrawSourceRow(BindingIndex, 'B', "B", 16.0f, true);
			DrawSourceRow(BindingIndex, 'A', "A", 16.0f, true);
			break;
		case EParticleCurveAssetKind::Float:
		default:
			break;
		}
	}
	ImGui::PopID();
	DrawList->PopClipRect();

	if (ActiveBinding)
	{
		CurveState.SelectedCurveChannel = NormalizeParticleCurveChannel(*ActiveBinding, CurveState.SelectedCurveChannel);
	}
	ActiveCurve = ActiveBinding ? GetParticleCurveChannelCurve(*ActiveBinding, CurveState.SelectedCurveChannel) : nullptr;

	const float GraphHeight = GraphSize.y;

	const float TimeGridStep = ChooseParticleCurveGridStep(PixelsPerTime, 56.0f);
	const float ValueGridStep = ChooseParticleCurveGridStep(PixelsPerValue, 30.0f);
	const float FirstGridTime = std::ceil(FirstTime / TimeGridStep) * TimeGridStep;
	for (float Time = FirstGridTime; ; Time += TimeGridStep)
	{
		const float X = GraphStart.x + (Time - FirstTime) * PixelsPerTime;
		if (X > CanvasEnd.x + 0.5f)
		{
			break;
		}
		DrawList->AddLine(ImVec2(X, GraphStart.y), ImVec2(X, CanvasEnd.y), IM_COL32(158, 158, 158, 90), 1.0f);
		if (GraphHeight > 34.0f)
		{
			char Label[32];
			snprintf(Label, sizeof(Label), "%.2f", Time);
			DrawList->AddText(ImVec2(X + 3.0f, CanvasEnd.y - 18.0f), IM_COL32(224, 224, 224, 255), Label);
		}
	}

	const float VisibleValueMin = ValueMax - GraphSize.y / std::max(1.0f, PixelsPerValue);
	const float FirstGridValue = std::floor(ValueMax / ValueGridStep) * ValueGridStep;
	for (float Value = FirstGridValue; Value >= std::min(ValueMin, VisibleValueMin) - 0.0001f; Value -= ValueGridStep)
	{
		const float Y = GraphStart.y + (ValueMax - Value) * PixelsPerValue;
		if (Y > CanvasEnd.y + 0.5f)
		{
			break;
		}
		DrawList->AddLine(ImVec2(GraphStart.x, Y), ImVec2(CanvasEnd.x, Y), IM_COL32(158, 158, 158, 90), 1.0f);
		char Label[32];
		snprintf(Label, sizeof(Label), "%.2f", Value);
		DrawList->AddText(ImVec2(GraphStart.x + 3.0f, Y - 8.0f), IM_COL32(232, 232, 232, 255), Label);
	}

	const float ZeroY = GraphStart.y + (ValueMax - 0.0f) * PixelsPerValue;
	if (ZeroY >= GraphStart.y && ZeroY <= CanvasEnd.y)
	{
		DrawList->AddLine(ImVec2(GraphStart.x, ZeroY), ImVec2(CanvasEnd.x, ZeroY), IM_COL32(255, 0, 178, 255), 1.0f);
	}

	DrawList->PushClipRect(GraphStart, CanvasEnd, true);
	const int32 SampleCount = std::max(8, static_cast<int32>(GraphSize.x / 8.0f));
	for (int32 Pass = 0; Pass < 2; ++Pass)
	{
		for (const FParticleCurveBinding& Binding : CurveBindings)
		{
			const bool bActiveBinding = ActiveBinding && (&Binding == ActiveBinding);
			if ((Pass == 0 && bActiveBinding) || (Pass == 1 && !bActiveBinding))
			{
				continue;
			}
			if (!IsParticleCurveBindingVisibleWithActive(Binding, ActiveBinding))
			{
				continue;
			}

			ForEachParticleCurveChannel(
				Binding,
				[&](const FString& ChannelLabel, FFloatCurve* Curve)
				{
					if (!Curve || Curve->Keys.empty())
					{
						return;
					}

					const bool bPrimaryEditableCurve = bActiveBinding && Curve == ActiveCurve;
					const ImU32 CurveColor = GetParticleCurveDisplayColor(ChannelLabel, bPrimaryEditableCurve);
					ImVec2 PrevPoint;
					bool bHasPrev = false;
					for (int32 SampleIndex = 0; SampleIndex <= SampleCount; ++SampleIndex)
					{
						const float X = GraphStart.x + GraphSize.x * static_cast<float>(SampleIndex) / static_cast<float>(SampleCount);
						const float Time = ScreenToTime(X);
						const float Value = Curve->Evaluate(Time);
						const float Y = GraphStart.y + (ValueMax - Value) * PixelsPerValue;
						const ImVec2 Point(X, Y);
						if (bHasPrev)
						{
							DrawList->AddLine(PrevPoint, Point, CurveColor, bPrimaryEditableCurve ? 2.4f : 1.5f);
						}
						PrevPoint = Point;
						bHasPrev = true;
					}

					for (int32 KeyIndex = 0; KeyIndex < static_cast<int32>(Curve->Keys.size()); ++KeyIndex)
					{
						const FCurveKey& Key = Curve->Keys[KeyIndex];
						const ImVec2 Point(
							GraphStart.x + (Key.Time - FirstTime) * PixelsPerTime,
							GraphStart.y + (ValueMax - Key.Value) * PixelsPerValue);
						const bool bSelected = bPrimaryEditableCurve && KeyIndex == CurveState.SelectedKeyIndex;
						const ImU32 FillColor = bSelected ? IM_COL32(118, 214, 255, 255) : GetParticleCurveDisplayColor(ChannelLabel, bPrimaryEditableCurve);
						DrawList->AddCircleFilled(Point, bSelected ? 5.5f : (bPrimaryEditableCurve ? 4.0f : 3.0f), FillColor, 16);
						DrawList->AddCircle(Point, bSelected ? 6.5f : (bPrimaryEditableCurve ? 5.0f : 4.0f), IM_COL32(20, 20, 20, 230), 16, 1.2f);
					}
				});
		}
	}
	DrawList->PopClipRect();

	DrawList->AddRect(CanvasStart, CanvasEnd, IM_COL32(82, 82, 82, 255), 0.0f);
	if (bGraphHovered)
	{
		ImGui::SetMouseCursor(
			CurveState.ActiveTool == EParticleCurveEditorTool::Zoom
				? ImGuiMouseCursor_ResizeAll
				: (bGraphDragging ? ImGuiMouseCursor_ResizeAll : ImGuiMouseCursor_Hand));
	}
}
