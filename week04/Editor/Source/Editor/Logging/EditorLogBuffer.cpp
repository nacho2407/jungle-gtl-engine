#include "EditorLogBuffer.h"

FEditorLogBuffer::~FEditorLogBuffer()
{
    LogEntries.clear();
}

void FEditorLogBuffer::Log(ELogVerbosity Verbosity, const char* Message)
{
    FEditorLogEntry LogEntry;
    LogEntry.Verbosity = Verbosity;
    LogEntry.Message = (Message != nullptr) ? Message : "";
    LogEntries.push_back(LogEntry);
}

void FEditorLogBuffer::Clear()
{
    LogEntries.clear();
}