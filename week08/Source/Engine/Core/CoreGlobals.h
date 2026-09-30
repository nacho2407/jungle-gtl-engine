#pragma once

#include "Core/Logging/LogOutputDevice.h"

class ILogOutputDevice;
extern ILogOutputDevice* GLog;
extern ELogLevel         GMinimumLogLevel;
