#include "Editor/Logging/EditorLogBuffer.h"

void FEditorLogBuffer::Log(ELogLevel Level, const char* Category, const char* Message, const char* FormattedMessage)
{
    FEditorLogEntry Entry;
    Entry.Level = Level;
    Entry.Category = Category != nullptr ? Category : "";
    Entry.Message = Message != nullptr ? Message : "";
    Entry.FormattedMessage = FormattedMessage != nullptr ? FormattedMessage : Entry.Message;
    LogEntries.push_back(std::move(Entry));
}

void FEditorLogBuffer::Clear()
{
    LogEntries.clear();
}
