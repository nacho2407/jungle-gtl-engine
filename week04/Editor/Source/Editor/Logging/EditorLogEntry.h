#pragma once

#include "Core/Containers/String.h"
#include "Core/Logging/LogOutputDevice.h"

struct FEditorLogEntry
{
    ELogVerbosity Verbosity;
    FString Message;  
};