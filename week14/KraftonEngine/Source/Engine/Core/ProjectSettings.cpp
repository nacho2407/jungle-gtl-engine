#include "Core/ProjectSettings.h"
#include "SimpleJSON/json.hpp"

#include <fstream>
#include <filesystem>

namespace PSKey
{
	constexpr const char* Shadow = "Shadow";
	constexpr const char* bShadows = "bShadows";
	constexpr const char* CSMResolution = "CSMResolution";
	constexpr const char* SpotAtlasResolution = "SpotAtlasResolution";
	constexpr const char* PointAtlasResolution = "PointAtlasResolution";
	constexpr const char* MaxSpotAtlasPages = "MaxSpotAtlasPages";
	constexpr const char* MaxPointAtlasPages = "MaxPointAtlasPages";

	constexpr const char* GameSection = "Game";
	constexpr const char* StartLevelName = "StartLevelName";
	constexpr const char* GameModeClassName = "GameModeClassName";

	constexpr const char* PhysicsSection = "Physics";
	constexpr const char* bEnablePvd = "bEnablePvd";
	constexpr const char* bPvdTransmitContacts = "bPvdTransmitContacts";
	constexpr const char* bPvdTransmitSceneQueries = "bPvdTransmitSceneQueries";
	constexpr const char* bPvdTransmitConstraints = "bPvdTransmitConstraints";
	constexpr const char* FixedTimeStep = "FixedTimeStep";
	constexpr const char* MaxSubSteps = "MaxSubSteps";
	constexpr const char* MaxAccumulatedTime = "MaxAccumulatedTime";

	constexpr const char* DiagnosticsSection = "Diagnostics";
	constexpr const char* CrashDumpShareDir = "CrashDumpShareDir";
}

void FProjectSettings::SaveToFile(const FString& Path) const
{
	using namespace json;

	JSON Root = Object();

	JSON ShadowObj = Object();
	ShadowObj[PSKey::bShadows] = Shadow.bEnabled;
	ShadowObj[PSKey::CSMResolution] = static_cast<int>(Shadow.CSMResolution);
	ShadowObj[PSKey::SpotAtlasResolution] = static_cast<int>(Shadow.SpotAtlasResolution);
	ShadowObj[PSKey::PointAtlasResolution] = static_cast<int>(Shadow.PointAtlasResolution);
	ShadowObj[PSKey::MaxSpotAtlasPages] = static_cast<int>(Shadow.MaxSpotAtlasPages);
	ShadowObj[PSKey::MaxPointAtlasPages] = static_cast<int>(Shadow.MaxPointAtlasPages);
	Root[PSKey::Shadow] = ShadowObj;

	JSON GameObj = Object();
	GameObj[PSKey::StartLevelName] = Game.StartLevelName;
	GameObj[PSKey::GameModeClassName] = Game.GameModeClassName;
	Root[PSKey::GameSection] = GameObj;

	JSON PhysicsObj = Object();
	PhysicsObj[PSKey::bEnablePvd] = Physics.bEnablePvd;
	PhysicsObj[PSKey::bPvdTransmitContacts] = Physics.bPvdTransmitContacts;
	PhysicsObj[PSKey::bPvdTransmitSceneQueries] = Physics.bPvdTransmitSceneQueries;
	PhysicsObj[PSKey::bPvdTransmitConstraints] = Physics.bPvdTransmitConstraints;
	PhysicsObj[PSKey::FixedTimeStep] = Physics.FixedTimeStep;
	PhysicsObj[PSKey::MaxSubSteps] = static_cast<int>(Physics.MaxSubSteps);
	PhysicsObj[PSKey::MaxAccumulatedTime] = Physics.MaxAccumulatedTime;
	Root[PSKey::PhysicsSection] = PhysicsObj;

	JSON DiagnosticsObj = Object();
	DiagnosticsObj[PSKey::CrashDumpShareDir] = Diagnostics.CrashDumpShareDir;
	Root[PSKey::DiagnosticsSection] = DiagnosticsObj;

	std::filesystem::path FilePath(FPaths::ToWide(Path));
	if (FilePath.has_parent_path())
		std::filesystem::create_directories(FilePath.parent_path());

	std::ofstream File(FilePath);
	if (File.is_open())
		File << Root;
}

void FProjectSettings::LoadFromFile(const FString& Path)
{
	using namespace json;

	std::ifstream File(std::filesystem::path(FPaths::ToWide(Path)));
	if (!File.is_open())
		return;

	FString Content((std::istreambuf_iterator<char>(File)),
		std::istreambuf_iterator<char>());

	JSON Root = JSON::Load(Content);

	if (Root.hasKey(PSKey::GameSection))
	{
		JSON G = Root[PSKey::GameSection];
		if (G.hasKey(PSKey::StartLevelName))
			Game.StartLevelName = G[PSKey::StartLevelName].ToString();
		if (G.hasKey(PSKey::GameModeClassName))
			Game.GameModeClassName = G[PSKey::GameModeClassName].ToString();
	}

	if (Root.hasKey(PSKey::PhysicsSection))
	{
		JSON P = Root[PSKey::PhysicsSection];
		if (P.hasKey(PSKey::bEnablePvd))
			Physics.bEnablePvd = P[PSKey::bEnablePvd].ToBool();
		if (P.hasKey(PSKey::bPvdTransmitContacts))
			Physics.bPvdTransmitContacts = P[PSKey::bPvdTransmitContacts].ToBool();
		if (P.hasKey(PSKey::bPvdTransmitSceneQueries))
			Physics.bPvdTransmitSceneQueries = P[PSKey::bPvdTransmitSceneQueries].ToBool();
		if (P.hasKey(PSKey::bPvdTransmitConstraints))
			Physics.bPvdTransmitConstraints = P[PSKey::bPvdTransmitConstraints].ToBool();
		if (P.hasKey(PSKey::FixedTimeStep))
			Physics.FixedTimeStep = static_cast<float>(P[PSKey::FixedTimeStep].ToFloat());
		if (P.hasKey(PSKey::MaxSubSteps))
			Physics.MaxSubSteps = static_cast<int32>(P[PSKey::MaxSubSteps].ToInt());
		if (P.hasKey(PSKey::MaxAccumulatedTime))
			Physics.MaxAccumulatedTime = static_cast<float>(P[PSKey::MaxAccumulatedTime].ToFloat());
	}

	if (Root.hasKey(PSKey::DiagnosticsSection))
	{
		JSON D = Root[PSKey::DiagnosticsSection];
		if (D.hasKey(PSKey::CrashDumpShareDir))
			Diagnostics.CrashDumpShareDir = D[PSKey::CrashDumpShareDir].ToString();
	}

	if (Root.hasKey(PSKey::Shadow))
	{
		JSON S = Root[PSKey::Shadow];
		if (S.hasKey(PSKey::bShadows))
			Shadow.bEnabled = S[PSKey::bShadows].ToBool();
		if (S.hasKey(PSKey::CSMResolution))
		{
			int v = S[PSKey::CSMResolution].ToInt();
			Shadow.CSMResolution = static_cast<uint32>((std::max)(64, (std::min)(v, 8192)));
		}
		if (S.hasKey(PSKey::SpotAtlasResolution))
		{
			int v = S[PSKey::SpotAtlasResolution].ToInt();
			Shadow.SpotAtlasResolution = static_cast<uint32>((std::max)(64, (std::min)(v, 8192)));
		}
		if (S.hasKey(PSKey::PointAtlasResolution))
		{
			int v = S[PSKey::PointAtlasResolution].ToInt();
			Shadow.PointAtlasResolution = static_cast<uint32>((std::max)(64, (std::min)(v, 8192)));
		}
		if (S.hasKey(PSKey::MaxSpotAtlasPages))
		{
			int v = S[PSKey::MaxSpotAtlasPages].ToInt();
			Shadow.MaxSpotAtlasPages = static_cast<uint32>(v > 1 ? v : 1);
		}
		if (S.hasKey(PSKey::MaxPointAtlasPages))
		{
			int v = S[PSKey::MaxPointAtlasPages].ToInt();
			Shadow.MaxPointAtlasPages = static_cast<uint32>(v > 1 ? v : 1);
		}
	}
}
