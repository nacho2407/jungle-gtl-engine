#pragma once

#include "Core/CoreMinimal.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <Windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi1_5.h>

#ifdef GetCurrentTime
#undef GetCurrentTime
#endif
