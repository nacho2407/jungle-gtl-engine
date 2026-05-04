#include "LuaPersistentStorage.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "Engine/Platform/Paths.h"
#include "SimpleJSON/json.hpp"

namespace
{
constexpr const char* PrefsValuesKey = "values";
constexpr const char* PrefTypeKey = "type";
constexpr const char* PrefValueKey = "value";
constexpr const char* ScoreEntriesKey = "scores";

std::filesystem::path GetPrefsFilePath()
{
    return std::filesystem::path(FPaths::SavedDir()) / L"PlayerPrefs.json";
}

std::filesystem::path GetScoreboardFilePath()
{
    return std::filesystem::path(FPaths::SavedDir()) / L"Scoreboard.json";
}

json::JSON LoadJsonFile(const std::filesystem::path& FilePath, json::JSON Fallback)
{
    std::ifstream File(FilePath);
    if (!File)
    {
        return Fallback;
    }

    std::stringstream Buffer;
    Buffer << File.rdbuf();
    json::JSON Loaded = json::JSON::Load(Buffer.str());
    return Loaded.IsNull() ? Fallback : Loaded;
}

bool SaveJsonFile(const std::filesystem::path& FilePath, const json::JSON& Root)
{
    FPaths::CreateDir(FPaths::SavedDir());

    std::ofstream File(FilePath);
    if (!File)
    {
        return false;
    }

    File << Root.dump();
    return true;
}

json::JSON MakePrefsRoot()
{
    json::JSON Root = json::Object();
    Root[PrefsValuesKey] = json::Object();
    return Root;
}

json::JSON MakeScoreboardRoot()
{
    json::JSON Root = json::Object();
    Root[ScoreEntriesKey] = json::Array();
    return Root;
}

json::JSON LoadPrefs()
{
    json::JSON Root = LoadJsonFile(GetPrefsFilePath(), MakePrefsRoot());
    if (Root.JSONType() != json::JSON::Class::Object)
    {
        Root = MakePrefsRoot();
    }
    if (!Root.hasKey(PrefsValuesKey) || Root[PrefsValuesKey].JSONType() != json::JSON::Class::Object)
    {
        Root[PrefsValuesKey] = json::Object();
    }
    return Root;
}

json::JSON LoadScoreboard()
{
    json::JSON Root = LoadJsonFile(GetScoreboardFilePath(), MakeScoreboardRoot());
    if (Root.JSONType() != json::JSON::Class::Object)
    {
        Root = MakeScoreboardRoot();
    }
    if (!Root.hasKey(ScoreEntriesKey) || Root[ScoreEntriesKey].JSONType() != json::JSON::Class::Array)
    {
        Root[ScoreEntriesKey] = json::Array();
    }
    return Root;
}

double JsonNumberOrDefault(const json::JSON& Value, double DefaultValue)
{
    if (Value.JSONType() == json::JSON::Class::Floating)
    {
        return Value.ToFloat();
    }
    if (Value.JSONType() == json::JSON::Class::Integral)
    {
        return static_cast<double>(Value.ToInt());
    }
    return DefaultValue;
}

FString JsonStringOrDefault(const json::JSON& Value, const FString& DefaultValue)
{
    return Value.JSONType() == json::JSON::Class::String ? Value.ToString() : DefaultValue;
}

bool JsonBoolOrDefault(const json::JSON& Value, bool DefaultValue)
{
    return Value.JSONType() == json::JSON::Class::Boolean ? Value.ToBool() : DefaultValue;
}

FString GetTimestamp()
{
    auto Now = std::chrono::system_clock::now();
    std::time_t NowTime = std::chrono::system_clock::to_time_t(Now);

    std::tm LocalTime{};
    localtime_s(&LocalTime, &NowTime);

    char Buffer[32] = {};
    std::strftime(Buffer, sizeof(Buffer), "%Y-%m-%d %H:%M:%S", &LocalTime);
    return Buffer;
}

bool SetPrefValue(const FString& Key, const FString& Type, json::JSON Value)
{
    if (Key.empty())
    {
        return false;
    }

    json::JSON Root = LoadPrefs();
    Root[PrefsValuesKey][Key][PrefTypeKey] = Type;
    Root[PrefsValuesKey][Key][PrefValueKey] = std::move(Value);
    return SaveJsonFile(GetPrefsFilePath(), Root);
}

json::JSON* FindPrefEntry(json::JSON& Root, const FString& Key)
{
    if (Key.empty() || !Root.hasKey(PrefsValuesKey) || !Root[PrefsValuesKey].hasKey(Key))
    {
        return nullptr;
    }

    return &Root[PrefsValuesKey][Key];
}

bool PrefHasKey(const FString& Key)
{
    json::JSON Root = LoadPrefs();
    return FindPrefEntry(Root, Key) != nullptr;
}

bool PrefSetNumber(const FString& Key, double Value)
{
    return SetPrefValue(Key, "number", json::JSON(Value));
}

double PrefGetNumber(const FString& Key, double DefaultValue)
{
    json::JSON Root = LoadPrefs();
    json::JSON* Entry = FindPrefEntry(Root, Key);
    if (!Entry || !Entry->hasKey(PrefTypeKey) || !Entry->hasKey(PrefValueKey))
    {
        return DefaultValue;
    }

    return (*Entry)[PrefTypeKey].ToString() == "number"
        ? JsonNumberOrDefault((*Entry)[PrefValueKey], DefaultValue)
        : DefaultValue;
}

bool PrefSetString(const FString& Key, const FString& Value)
{
    return SetPrefValue(Key, "string", json::JSON(Value));
}

FString PrefGetString(const FString& Key, const FString& DefaultValue)
{
    json::JSON Root = LoadPrefs();
    json::JSON* Entry = FindPrefEntry(Root, Key);
    if (!Entry || !Entry->hasKey(PrefTypeKey) || !Entry->hasKey(PrefValueKey))
    {
        return DefaultValue;
    }

    return (*Entry)[PrefTypeKey].ToString() == "string"
        ? JsonStringOrDefault((*Entry)[PrefValueKey], DefaultValue)
        : DefaultValue;
}

bool PrefSetBool(const FString& Key, bool Value)
{
    return SetPrefValue(Key, "bool", json::JSON(Value));
}

bool PrefGetBool(const FString& Key, bool DefaultValue)
{
    json::JSON Root = LoadPrefs();
    json::JSON* Entry = FindPrefEntry(Root, Key);
    if (!Entry || !Entry->hasKey(PrefTypeKey) || !Entry->hasKey(PrefValueKey))
    {
        return DefaultValue;
    }

    return (*Entry)[PrefTypeKey].ToString() == "bool"
        ? JsonBoolOrDefault((*Entry)[PrefValueKey], DefaultValue)
        : DefaultValue;
}

bool PrefClear()
{
    return SaveJsonFile(GetPrefsFilePath(), MakePrefsRoot());
}

struct FLuaScoreEntry
{
    FString Name;
    double Score = 0.0;
    double HP = 0.0;
    int32 DocumentCount = 0;
    double Timer = 0.0;
    FString Timestamp;
};

FLuaScoreEntry ReadScoreEntry(const json::JSON& Entry)
{
    FLuaScoreEntry Result;
    if (Entry.JSONType() != json::JSON::Class::Object)
    {
        return Result;
    }

    if (Entry.hasKey("name"))
    {
        Result.Name = JsonStringOrDefault(Entry.at("name"), "Player");
    }
    if (Entry.hasKey("score"))
    {
        Result.Score = JsonNumberOrDefault(Entry.at("score"), 0.0);
    }
    if (Entry.hasKey("hp"))
    {
        Result.HP = JsonNumberOrDefault(Entry.at("hp"), 0.0);
    }
    if (Entry.hasKey("documentCount"))
    {
        Result.DocumentCount = static_cast<int32>(JsonNumberOrDefault(Entry.at("documentCount"), 0.0));
    }
    if (Entry.hasKey("timer"))
    {
        Result.Timer = JsonNumberOrDefault(Entry.at("timer"), 0.0);
    }
    if (Entry.hasKey("timestamp"))
    {
        Result.Timestamp = JsonStringOrDefault(Entry.at("timestamp"), "");
    }

    return Result;
}

json::JSON WriteScoreEntry(const FLuaScoreEntry& Entry)
{
    json::JSON Result = json::Object();
    Result["name"] = Entry.Name;
    Result["score"] = Entry.Score;
    Result["hp"] = Entry.HP;
    Result["documentCount"] = Entry.DocumentCount;
    Result["timer"] = Entry.Timer;
    Result["timestamp"] = Entry.Timestamp;
    return Result;
}

TArray<FLuaScoreEntry> ReadScoreEntries(json::JSON& Root)
{
    TArray<FLuaScoreEntry> Entries;
    for (const json::JSON& EntryJson : Root[ScoreEntriesKey].ArrayRange())
    {
        Entries.push_back(ReadScoreEntry(EntryJson));
    }

    return Entries;
}

void SortScoreEntries(TArray<FLuaScoreEntry>& Entries)
{
    std::sort(Entries.begin(), Entries.end(), [](const FLuaScoreEntry& A, const FLuaScoreEntry& B) {
        if (A.Score != B.Score)
        {
            return A.Score > B.Score;
        }
        return A.Timestamp < B.Timestamp;
    });
}

bool SaveScoreEntries(const TArray<FLuaScoreEntry>& Entries)
{
    json::JSON Root = MakeScoreboardRoot();
    for (const FLuaScoreEntry& Entry : Entries)
    {
        Root[ScoreEntriesKey].append(WriteScoreEntry(Entry));
    }

    return SaveJsonFile(GetScoreboardFilePath(), Root);
}

bool ScoreboardAddScore(const FString& Name, double Score, double HP, int32 DocumentCount, sol::optional<double> Timer)
{
    json::JSON Root = LoadScoreboard();
    TArray<FLuaScoreEntry> Entries = ReadScoreEntries(Root);

    FLuaScoreEntry NewEntry;
    NewEntry.Name = Name.empty() ? "Player" : Name;
    NewEntry.Score = Score;
    NewEntry.HP = HP;
    NewEntry.DocumentCount = DocumentCount;
    NewEntry.Timer = Timer.value_or(0.0);
    NewEntry.Timestamp = GetTimestamp();

    Entries.push_back(NewEntry);
    SortScoreEntries(Entries);
    return SaveScoreEntries(Entries);
}

sol::table ScoreboardGetTopScores(sol::this_state State, int32 Limit)
{
    sol::state_view Lua(State);
    sol::table Result = Lua.create_table();

    json::JSON Root = LoadScoreboard();
    TArray<FLuaScoreEntry> Entries = ReadScoreEntries(Root);
    SortScoreEntries(Entries);

    if (Limit <= 0 || Limit > static_cast<int32>(Entries.size()))
    {
        Limit = static_cast<int32>(Entries.size());
    }

    for (int32 Index = 0; Index < Limit; ++Index)
    {
        const FLuaScoreEntry& Entry = Entries[Index];

        sol::table Row = Lua.create_table();
        Row["rank"] = Index + 1;
        Row["name"] = Entry.Name;
        Row["score"] = Entry.Score;
        Row["hp"] = Entry.HP;
        Row["documentCount"] = Entry.DocumentCount;
        Row["timer"] = Entry.Timer;
        Row["timestamp"] = Entry.Timestamp;

        Result[Index + 1] = Row;
    }

    return Result;
}

bool ScoreboardClear()
{
    return SaveJsonFile(GetScoreboardFilePath(), MakeScoreboardRoot());
}
} // namespace

void FLuaPersistentStorage::Bind(sol::state& Lua)
{
    sol::table Prefs = Lua.create_table();
    Prefs.set_function("SetNumber", &PrefSetNumber);
    Prefs.set_function("GetNumber", &PrefGetNumber);
    Prefs.set_function("SetString", &PrefSetString);
    Prefs.set_function("GetString", &PrefGetString);
    Prefs.set_function("SetBool", &PrefSetBool);
    Prefs.set_function("GetBool", &PrefGetBool);
    Prefs.set_function("HasKey", &PrefHasKey);
    Prefs.set_function("Clear", &PrefClear);
    Lua["Prefs"] = Prefs;

    sol::table Scoreboard = Lua.create_table();
    Scoreboard.set_function("AddScore", &ScoreboardAddScore);
    Scoreboard.set_function("GetTopScores", &ScoreboardGetTopScores);
    Scoreboard.set_function("Clear", &ScoreboardClear);
    Lua["Scoreboard"] = Scoreboard;
}
