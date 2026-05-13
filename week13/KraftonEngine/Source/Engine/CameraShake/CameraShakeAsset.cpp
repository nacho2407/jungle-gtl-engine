#include "CameraShakeAsset.h"
#include "Object/ObjectFactory.h"
#include "Platform/Paths.h"
#include "Serialization/Archive.h"

#include <fstream>
#include <sstream>

#include <SimpleJSON/json.hpp>

IMPLEMENT_CLASS(UCameraShakeAsset, UObject)

static FVector ReadVector(json::JSON& Object, const FString& Key, const FVector& DefaultValue)
{
	if (!Object.hasKey(Key))
	{
		return DefaultValue;
	}

	json::JSON& Array = Object[Key];
	if (Array.length() < 3)
	{
		return DefaultValue;
	}

	return FVector(
		Array[0].ToFloat(),
		Array[1].ToFloat(),
		Array[2].ToFloat());
}

static FRotator ReadRotator(json::JSON& Object, const FString& Key, const FRotator& DefaultValue)
{
	if (!Object.hasKey(Key))
	{
		return DefaultValue;
	}
	json::JSON& Array = Object[Key];
	if (Array.length() < 3)
	{
		return DefaultValue;
	}
	return FRotator(
		Array[0].ToFloat(),
		Array[1].ToFloat(),
		Array[2].ToFloat());
}

UCameraShakeAsset::~UCameraShakeAsset()
{
}

static void SerializeVector(FArchive& Ar, FVector& Value)
{
	Ar << Value.X;
	Ar << Value.Y;
	Ar << Value.Z;
}

static void SerializeRotator(FArchive& Ar, FRotator& Value)
{
	Ar << Value.Pitch;
	Ar << Value.Yaw;
	Ar << Value.Roll;
}

void UCameraShakeAsset::Serialize(FArchive& Ar)
{
	Ar << Version;

	int32 Type = static_cast<int32>(ShakeType);
	Ar << Type;
	if (Ar.IsLoading())
	{
		ShakeType = static_cast<ECameraShakeType>(Type);
	}

	Ar << Duration;
	Ar << BlendInTime;
	Ar << BlendOutTime;
	Ar << bSingleInstance;

	Ar << Sequence.LocXCurvePath;
	Ar << Sequence.LocYCurvePath;
	Ar << Sequence.LocZCurvePath;

	Ar << Sequence.PitchCurvePath;
	Ar << Sequence.YawCurvePath;
	Ar << Sequence.RollCurvePath;

	Ar << Sequence.FOVCurvePath;

	SerializeVector(Ar, WaveOscillator.LocationAmplitude);
	SerializeVector(Ar, WaveOscillator.LocationFrequency);

	SerializeRotator(Ar, WaveOscillator.RotationAmplitude);
	SerializeRotator(Ar, WaveOscillator.RotationFrequency);

	Ar << WaveOscillator.FOVAmplitude;
	Ar << WaveOscillator.FOVFrequency;
}

bool UCameraShakeAsset::LoadFromFile(const FString& Path)
{
	using namespace json;

	std::ifstream File(FPaths::ToWide(Path));
	if (!File.is_open())
	{
		return false;
	}

	std::stringstream Buffer;
	Buffer << File.rdbuf();

	JSON Root = JSON::Load(Buffer.str());
	if (Root.IsNull()) return false;

	if (Root.hasKey("Version"))
	{
		Version = Root["Version"].ToInt();
	}

	if (Root.hasKey("ShakeType"))
	{
		const FString Type = Root["ShakeType"].ToString();

		if (Type == "Sequence")
		{
			ShakeType = ECameraShakeType::Sequence;
		}
		else if (Type == "WaveOscillator")
		{
			ShakeType = ECameraShakeType::WaveOscillator;
		}
		else
		{
			return false;
		}
	}

	if (Root.hasKey("Duration"))
	{
		Duration = Root["Duration"].ToFloat();
	}
	if (Root.hasKey("BlendInTime"))
	{
		BlendInTime = Root["BlendInTime"].ToFloat();
	}
	if (Root.hasKey("BlendOutTime"))
	{
		BlendOutTime = Root["BlendOutTime"].ToFloat();
	}
	if (Root.hasKey("bSingleInstance"))
	{
		bSingleInstance = Root["bSingleInstance"].ToBool();
	}

	if (Root.hasKey("Sequence"))
	{
		JSON& SequenceJson = Root["Sequence"];

		if (SequenceJson.hasKey("LocXCurve")) Sequence.LocXCurvePath = SequenceJson["LocXCurve"].ToString();
		if (SequenceJson.hasKey("LocYCurve")) Sequence.LocYCurvePath = SequenceJson["LocYCurve"].ToString();
		if (SequenceJson.hasKey("LocZCurve")) Sequence.LocZCurvePath = SequenceJson["LocZCurve"].ToString();

		if (SequenceJson.hasKey("PitchCurve")) Sequence.PitchCurvePath = SequenceJson["PitchCurve"].ToString();
		if (SequenceJson.hasKey("YawCurve")) Sequence.YawCurvePath = SequenceJson["YawCurve"].ToString();
		if (SequenceJson.hasKey("RollCurve")) Sequence.RollCurvePath = SequenceJson["RollCurve"].ToString();

		if (SequenceJson.hasKey("FOVCurve")) Sequence.FOVCurvePath = SequenceJson["FOVCurve"].ToString();
	}

	if (Root.hasKey("WaveOscillator"))
	{
		JSON& WaveJson = Root["WaveOscillator"];

		WaveOscillator.LocationAmplitude = ReadVector(WaveJson, "LocationAmplitude", WaveOscillator.LocationAmplitude);
		WaveOscillator.LocationFrequency = ReadVector(WaveJson, "LocationFrequency", WaveOscillator.LocationFrequency);

		WaveOscillator.RotationAmplitude = ReadRotator(WaveJson, "RotationAmplitude", WaveOscillator.RotationAmplitude);
		WaveOscillator.RotationFrequency = ReadRotator(WaveJson, "RotationFrequency", WaveOscillator.RotationFrequency);

		if (WaveJson.hasKey("FOVAmplitude"))
		{
			WaveOscillator.FOVAmplitude = WaveJson["FOVAmplitude"].ToFloat();
		}
		if (WaveJson.hasKey("FOVFrequency"))
		{
			WaveOscillator.FOVFrequency = WaveJson["FOVFrequency"].ToFloat();
		}
	}

	return true;
}

bool UCameraShakeAsset::SaveToFile(const FString& Path) const
{
	using namespace json;

	JSON Root = json::Object();
	Root["Version"] = Version;
	Root["ShakeType"] = ShakeType == ECameraShakeType::Sequence ? "Sequence" : "WaveOscillator";
	Root["Duration"] = Duration;
	Root["BlendInTime"] = BlendInTime;
	Root["BlendOutTime"] = BlendOutTime; 

	Root["bSingleInstance"] = bSingleInstance;

	JSON SequenceJson = json::Object();
	SequenceJson["LocXCurve"] = Sequence.LocXCurvePath;
	SequenceJson["LocYCurve"] = Sequence.LocYCurvePath;
	SequenceJson["LocZCurve"] = Sequence.LocZCurvePath;
	SequenceJson["PitchCurve"] = Sequence.PitchCurvePath;
	SequenceJson["YawCurve"] = Sequence.YawCurvePath;
	SequenceJson["RollCurve"] = Sequence.RollCurvePath;
	SequenceJson["FOVCurve"] = Sequence.FOVCurvePath;
	Root["Sequence"] = SequenceJson;

	JSON WaveJson = json::Object();
	auto WriteVector = [](json::JSON& Object, const FString& Key, const FVector& Vec)
	{
		json::JSON Array = json::Array();
		Array.append(Vec.X);
		Array.append(Vec.Y);
		Array.append(Vec.Z);
		Object[Key] = Array;
	};
	auto WriteRotator = [](json::JSON& Object, const FString& Key, const FRotator& Rot)
	{
		json::JSON Array = json::Array();
		Array.append(Rot.Pitch);
		Array.append(Rot.Yaw);
		Array.append(Rot.Roll);
		Object[Key] = Array;
	};
	WriteVector(WaveJson, "LocationAmplitude", WaveOscillator.LocationAmplitude);
	WriteVector(WaveJson, "LocationFrequency", WaveOscillator.LocationFrequency);
	WriteRotator(WaveJson, "RotationAmplitude", WaveOscillator.RotationAmplitude);
	WriteRotator(WaveJson, "RotationFrequency", WaveOscillator.RotationFrequency);
	WaveJson["FOVAmplitude"] = WaveOscillator.FOVAmplitude;
	WaveJson["FOVFrequency"] = WaveOscillator.FOVFrequency;
	Root["WaveOscillator"] = WaveJson;

	std::ofstream File(FPaths::ToWide(Path));
	if (!File.is_open())
	{
		return false;
	}

	File << Root.dump(4);
	return true;
}
