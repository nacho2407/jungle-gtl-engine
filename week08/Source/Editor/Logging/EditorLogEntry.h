#pragma once

#include "Core/CoreTypes.h"
#include "Core/Logging/LogOutputDevice.h"

struct FEditorLogEntry
{
    ELogLevel Level = ELogLevel::Info;
    FString   Category;
    FString   Message;
    FString   FormattedMessage;
};
