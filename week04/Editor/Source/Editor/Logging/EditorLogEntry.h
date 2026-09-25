#pragma once

#include "Core/CoreMinimal.h"

#include "Core/Logging/LogOutputDevice.h"

struct FEditorLogEntry
{
    ELogVerbosity Verbosity;
    FString Message;  
};