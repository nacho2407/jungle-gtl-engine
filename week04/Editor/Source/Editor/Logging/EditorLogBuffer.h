#pragma once
#pragma once

#include "EditorLogEntry.h"
#include "Core/Containers/Array.h"
#include "Core/Logging/LogOutputDevice.h"


class FEditorLogBuffer : public ILogOutputDevice
{
public:
    FEditorLogBuffer() = default;
    ~FEditorLogBuffer() override;
    
    void Log(ELogVerbosity Verbosity, const char * Message) override;

    const TArray<FEditorLogEntry>& GetLogBuffer() const { return LogEntries; }
    void Clear();

private:
    TArray<FEditorLogEntry> LogEntries;
};