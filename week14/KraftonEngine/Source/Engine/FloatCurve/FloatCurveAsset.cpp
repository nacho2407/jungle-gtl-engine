#include "FloatCurveAsset.h"
#include "Platform/Paths.h"
#include "Object/ObjectFactory.h"

#include <fstream>
#include <sstream>

#include <SimpleJSON/json.hpp>

IMPLEMENT_CLASS(UFloatCurveAsset, UObject)

UFloatCurveAsset::~UFloatCurveAsset()
{
}

bool UFloatCurveAsset::LoadFromFile(const FString& Path)
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
	if (Root.IsNull())
	{
		return false;
	}

	Curve.Reset();

	if (Root.hasKey("DefaultValue"))
	{
		Curve.DefaultValue = Root["DefaultValue"].ToFloat();
	}
	if (Root.hasKey("PreExtrapMode"))
	{
		Curve.PreExtrapMode = static_cast<ECurveExtrapMode>(Root["PreExtrapMode"].ToInt());
	}
	if (Root.hasKey("PostExtrapMode"))
	{
		Curve.PostExtrapMode = static_cast<ECurveExtrapMode>(Root["PostExtrapMode"].ToInt());
	}
	if (Root.hasKey("Keys"))
	{
		for (auto& KeyJson : Root["Keys"].ArrayRange())
		{
			FCurveKey Key = {};
			if (KeyJson.hasKey("Time")) Key.Time = KeyJson["Time"].ToFloat();
			if (KeyJson.hasKey("Value")) Key.Value = KeyJson["Value"].ToFloat();
			if (KeyJson.hasKey("InterpMode")) Key.InterpMode = static_cast<ECurveInterpMode>(KeyJson["InterpMode"].ToInt());
			if (KeyJson.hasKey("ArriveTangent")) Key.ArriveTangent = KeyJson["ArriveTangent"].ToFloat();
			if (KeyJson.hasKey("LeaveTangent")) Key.LeaveTangent = KeyJson["LeaveTangent"].ToFloat();
			Curve.Keys.push_back(Key);
		}
	}

	Curve.SortKeys();
	return true;
}

bool UFloatCurveAsset::SaveToFile(const FString& Path) const
{
	using namespace json;

	JSON Root = json::Object();
	Root["Version"] = 1;
	Root["DefaultValue"] = Curve.DefaultValue;
	Root["PreExtrapMode"] = static_cast<int>(Curve.PreExtrapMode);
	Root["PostExtrapMode"] = static_cast<int>(Curve.PostExtrapMode);

	JSON Keys = json::Array();
	for (const FCurveKey& Key : Curve.Keys)
	{
		JSON KeyJson = json::Object();
		KeyJson["Time"] = Key.Time;
		KeyJson["Value"] = Key.Value;
		KeyJson["InterpMode"] = static_cast<int>(Key.InterpMode);
		KeyJson["ArriveTangent"] = Key.ArriveTangent;
		KeyJson["LeaveTangent"] = Key.LeaveTangent;
		Keys.append(KeyJson);
	}

	Root["Keys"] = Keys;

	std::ofstream File(FPaths::ToWide(Path));
	if (!File.is_open())
	{
		return false;
	}

	File << Root.dump(4);
	return true;
}
