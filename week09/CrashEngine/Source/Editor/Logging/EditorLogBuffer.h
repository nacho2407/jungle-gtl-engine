#pragma once

#include "Editor/Logging/EditorLogEntry.h"

class FEditorLogBuffer : public ILogOutputDevice
{
public:
    void Log(ELogLevel Level, const char* Category, const char* Message, const char* FormattedMessage) override;

    const TArray<FEditorLogEntry>& GetEntries() const { return LogEntries; }
    void Clear();

private:
    TArray<FEditorLogEntry> LogEntries;
};
