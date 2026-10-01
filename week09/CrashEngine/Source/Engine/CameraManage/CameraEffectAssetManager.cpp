#include "CameraManage/CameraEffectAssetManager.h"

#include "Engine/Platform/Paths.h"
#include "SimpleJSON/json.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <unordered_map>

namespace
{
std::unordered_map<FString, FCameraEffectAsset> GCameraEffectAssetCache;

std::filesystem::path ResolveCameraEffectPath(const FString& AssetPath)
{
    const std::filesystem::path Input = FPaths::ToPath(AssetPath);
    if (Input.is_absolute() && std::filesystem::exists(Input))
    {
        return Input;
    }

    TArray<std::filesystem::path> Candidates;
    Candidates.push_back((std::filesystem::path(FPaths::AssetDir()) / L"CameraEffects" / Input).lexically_normal());
    Candidates.push_back((std::filesystem::path(FPaths::RootDir()) / Input).lexically_normal());
    Candidates.push_back((std::filesystem::path(FPaths::AssetDir()) / Input).lexically_normal());
    Candidates.push_back((std::filesystem::path(FPaths::ContentDir()) / Input).lexically_normal());

    for (const std::filesystem::path& Candidate : Candidates)
    {
        if (std::filesystem::exists(Candidate))
        {
            return Candidate;
        }
    }

    return Candidates.empty() ? Input : Candidates.front();
}

FString MakeCacheKey(const FString& AssetPath)
{
    return FPaths::FromPath(ResolveCameraEffectPath(AssetPath).lexically_normal());
}

ECameraEffectType ParseEffectType(const FString& Type)
{
    if (Type == "Shake")
    {
        return ECameraEffectType::Shake;
    }
    if (Type == "Fade")
    {
        return ECameraEffectType::Fade;
    }
    if (Type == "LetterBox")
    {
        return ECameraEffectType::LetterBox;
    }
    if (Type == "GammaCorrection")
    {
        return ECameraEffectType::GammaCorrection;
    }
    return ECameraEffectType::Vignette;
}

float ReadJsonNumber(json::JSON& Object)
{
    bool bOk = false;
    const double FloatValue = Object.ToFloat(bOk);
    if (bOk)
    {
        return static_cast<float>(FloatValue);
    }

    const long IntValue = Object.ToInt(bOk);
    return bOk ? static_cast<float>(IntValue) : 0.0f;
}

void ReadFloat(json::JSON& Object, const char* Key, float& OutValue)
{
    if (Object.hasKey(Key))
    {
        OutValue = ReadJsonNumber(Object[Key]);
    }
}

FCameraFloatCurve ReadCurve(json::JSON& Object, const char* Key)
{
    FCameraFloatCurve Curve;
    if (!Object.hasKey(Key))
    {
        return Curve;
    }

    for (auto& PointJson : Object[Key].ArrayRange())
    {
        FCameraCurveKey Point;
        Point.Time = PointJson.hasKey("Time") ? ReadJsonNumber(PointJson["Time"]) : 0.0f;
        Point.Value = PointJson.hasKey("Value") ? ReadJsonNumber(PointJson["Value"]) : 0.0f;
        Point.ArriveTangent = PointJson.hasKey("ArriveTangent") ? ReadJsonNumber(PointJson["ArriveTangent"]) : 0.0f;
        Point.LeaveTangent = PointJson.hasKey("LeaveTangent") ? ReadJsonNumber(PointJson["LeaveTangent"]) : 0.0f;
        Point.bUseTangents = PointJson.hasKey("ArriveTangent") || PointJson.hasKey("LeaveTangent");
        Curve.Keys.push_back(Point);
    }

    std::sort(Curve.Keys.begin(), Curve.Keys.end(), [](const FCameraCurveKey& A, const FCameraCurveKey& B)
    {
        return A.Time < B.Time;
    });
    return Curve;
}

void ReadColor(json::JSON& Object, FVector4& OutColor)
{
    if (!Object.hasKey("Color"))
    {
        return;
    }

    json::JSON& Color = Object["Color"];
    OutColor.X = ReadJsonNumber(Color[0]);
    OutColor.Y = ReadJsonNumber(Color[1]);
    OutColor.Z = ReadJsonNumber(Color[2]);
    OutColor.W = ReadJsonNumber(Color[3]);
}
} // namespace

bool FCameraEffectAssetManager::Load(const FString& AssetPath, FCameraEffectAsset& OutAsset)
{
    const std::filesystem::path FullPath = ResolveCameraEffectPath(AssetPath);
    std::ifstream File(FullPath);
    if (!File.is_open())
    {
        return false;
    }

    const FString Content((std::istreambuf_iterator<char>(File)), std::istreambuf_iterator<char>());
    json::JSON Root = json::JSON::Load(Content);

    const FString TypeString = Root.hasKey("Type") ? Root["Type"].ToString() : FString("Vignette");
    OutAsset = FCameraEffectAsset{};
    OutAsset.Type = ParseEffectType(TypeString);

    if (OutAsset.Type == ECameraEffectType::Shake && Root.hasKey("Shake"))
    {
        json::JSON& Shake = Root["Shake"];
        ReadFloat(Shake, "Duration", OutAsset.Shake.Duration);
        ReadFloat(Shake, "LocationAmplitude", OutAsset.Shake.LocationAmplitude);
        ReadFloat(Shake, "RotationAmplitude", OutAsset.Shake.RotationAmplitude);
        ReadFloat(Shake, "Frequency", OutAsset.Shake.Frequency);
        OutAsset.Shake.LocationCurve = ReadCurve(Shake, "LocationCurve");
        OutAsset.Shake.RotationCurve = ReadCurve(Shake, "RotationCurve");
    }
    else if (OutAsset.Type == ECameraEffectType::Fade && Root.hasKey("Fade"))
    {
        json::JSON& Fade = Root["Fade"];
        ReadFloat(Fade, "Duration", OutAsset.Fade.Duration);
        ReadFloat(Fade, "FromAmount", OutAsset.Fade.FromAmount);
        ReadFloat(Fade, "ToAmount", OutAsset.Fade.ToAmount);
        ReadColor(Fade, OutAsset.Fade.Color);
        OutAsset.Fade.AmountCurve = ReadCurve(Fade, "AmountCurve");
    }
    else if (OutAsset.Type == ECameraEffectType::LetterBox && Root.hasKey("LetterBox"))
    {
        json::JSON& LetterBox = Root["LetterBox"];
        ReadFloat(LetterBox, "Duration", OutAsset.LetterBox.Duration);
        ReadFloat(LetterBox, "FromAmount", OutAsset.LetterBox.FromAmount);
        ReadFloat(LetterBox, "ToAmount", OutAsset.LetterBox.ToAmount);
        OutAsset.LetterBox.AmountCurve = ReadCurve(LetterBox, "AmountCurve");
    }
    else if (OutAsset.Type == ECameraEffectType::GammaCorrection && Root.hasKey("GammaCorrection"))
    {
        json::JSON& Gamma = Root["GammaCorrection"];
        ReadFloat(Gamma, "Duration", OutAsset.GammaCorrection.Duration);
        ReadFloat(Gamma, "FromGamma", OutAsset.GammaCorrection.FromGamma);
        ReadFloat(Gamma, "ToGamma", OutAsset.GammaCorrection.ToGamma);
        OutAsset.GammaCorrection.GammaCurve = ReadCurve(Gamma, "GammaCurve");
    }
    else if (OutAsset.Type == ECameraEffectType::Vignette && Root.hasKey("Vignette"))
    {
        json::JSON& Vignette = Root["Vignette"];
        ReadFloat(Vignette, "Duration", OutAsset.Vignette.Duration);
        ReadFloat(Vignette, "FromIntensity", OutAsset.Vignette.FromIntensity);
        ReadFloat(Vignette, "ToIntensity", OutAsset.Vignette.ToIntensity);
        ReadFloat(Vignette, "Radius", OutAsset.Vignette.Radius);
        ReadFloat(Vignette, "Softness", OutAsset.Vignette.Softness);
        OutAsset.Vignette.IntensityCurve = ReadCurve(Vignette, "IntensityCurve");
    }

    return true;
}

bool FCameraEffectAssetManager::GetOrLoad(const FString& AssetPath, FCameraEffectAsset& OutAsset)
{
    const FString CacheKey = MakeCacheKey(AssetPath);
    auto It = GCameraEffectAssetCache.find(CacheKey);
    if (It != GCameraEffectAssetCache.end())
    {
        OutAsset = It->second;
        return true;
    }

    if (!Load(AssetPath, OutAsset))
    {
        return false;
    }

    GCameraEffectAssetCache[CacheKey] = OutAsset;
    return true;
}

void FCameraEffectAssetManager::Invalidate(const FString& AssetPath)
{
    GCameraEffectAssetCache.erase(MakeCacheKey(AssetPath));
}

void FCameraEffectAssetManager::ClearCache()
{
    GCameraEffectAssetCache.clear();
}
